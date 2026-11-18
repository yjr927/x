/*
  Tangible Earth | Arduino Uno R3, WonderCam Feature Learning, 5641AS, SG90.
  IDs: 1 penguin, 2 polar bear, 3 bicycle, 4 car, 5 hand rake, 6 electric leaf blower.
  Each new car or electric leaf blower adds 20 F and moves the ice servo by 180 degrees.
  With two connected digits, three-digit and larger values scroll left.
  Serial Monitor: 9600 baud. Send r to start another demo at 70 F and 180 degrees.

  Existing wiring: SDA=A4, SCL=A5, common GND, WonderCam powered via USB.
  SG90 signal=D6. Keep the display wiring and its nine resistors:
  segments A..G on D2,D3,D4,D7,D8,D9,D10; tens=A2, ones=A3.
  Timer2 refreshes the display; the Servo library uses Timer1.
*/
#include <Arduino.h>
#include <Servo.h>
#include <avr/interrupt.h>
#include "WonderCamFeatureReader.h"
#include "EarthMemory.h"

#if !defined(__AVR_ATmega328P__) || F_CPU != 16000000UL
#error Select Arduino Uno R3 (ATmega328P, 16 MHz).
#endif

const byte PENGUIN = 1, POLAR_BEAR = 2, BICYCLE = 3, CAR = 4, HAND_RAKE = 5, LEAF_BLOWER = 6;
const byte SERVO_PIN = 6;
const byte HOME_DEG = 180, AWAY_DEG = 0;
const uint32_t START_TEMP_F = 70, TEMP_RISE_F = 20;
const unsigned long POLL_MS = 100, TRIGGER_DWELL_MS = 500, CLEAR_DWELL_MS = 1000;
const unsigned long MAX_SAMPLE_GAP_MS = 300;

const byte SEGMENTS[] = {2, 3, 4, 7, 8, 9, 10};
const byte DIGITS[] = {A2, A3};
const byte PATTERNS[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66,
                         0x6d, 0x7d, 0x07, 0x7f, 0x6f};
const byte BLANK_DIGIT = 15;
volatile byte displayPair = 0x70;
char temperatureText[11] = "70";
byte temperatureLength = 2;
unsigned long displayCycleMs = 0;
uint32_t temperatureF = START_TEMP_F;

WonderCamFeatureReader camera;
Servo iceServo;
int commandedDeg = HOME_DEG, targetDeg = HOME_DEG;
uint32_t pendingHalfTurns = 0;
unsigned long lastStepMs = 0, lastPollMs = 0, lastReportMs = 0;

Scene candidate = Scene::Unknown;
unsigned long candidateSince = 0, lastSampleMs = 0;
Scene latched = Scene::Clear;
bool automatic = true;
EarthMemory memory;
WonderCamFeatureReader::Result latest = {WonderCamFeatureReader::BusError, 0, 0};

void saveState() {
  EarthState state = {temperatureF, HOME_DEG, byte(automatic), latched};
  memory.save(state);
}

// The ISR reads one packed byte for the whole two-digit scan. Neither camera
// I/O nor servo positioning runs in this interrupt.
ISR(TIMER2_COMPA_vect) {
  static byte digit = 0, frame = 0x70;
  pinMode(DIGITS[digit ^ 1], INPUT);
  if (digit == 0) frame = displayPair;
  const byte value = digit == 0 ? frame >> 4 : frame & 0x0f;
  const byte pattern = value == BLANK_DIGIT ? 0 : PATTERNS[value];
  for (byte i = 0; i < 7; ++i)
    digitalWrite(SEGMENTS[i], (pattern & (1U << i)) ? HIGH : LOW);
  pinMode(DIGITS[digit], OUTPUT);  // output latch is LOW (common cathode)
  digit ^= 1;
}

void startDisplay() {
  for (byte pin : DIGITS) { pinMode(pin, INPUT); digitalWrite(pin, LOW); }
  for (byte pin : SEGMENTS) { digitalWrite(pin, LOW); pinMode(pin, OUTPUT); }
  const byte saved = SREG;
  cli();
  TCCR2B = 0;
  TCCR2A = _BV(WGM21);
  TCNT2 = 0;
  OCR2A = 249;
  TIFR2 = _BV(OCF2A);
  TIMSK2 = _BV(OCIE2A);
  TCCR2B = _BV(CS22) | _BV(CS20);  // 500 Hz, 250 Hz per digit
  SREG = saved;
}

void setTemperature(uint32_t value, unsigned long now) {
  temperatureF = value;
  ultoa(value, temperatureText, 10);
  temperatureLength = strlen(temperatureText);
  displayCycleMs = now;
}

byte displayDigit(int index) {
  if (index < 0 || index >= temperatureLength) return BLANK_DIGIT;
  return temperatureText[index] - '0';
}

void updateDisplay(unsigned long now) {
  int first = 0;
  if (temperatureLength == 1) first = -1;
  else if (temperatureLength > 2) {
    // For 110: " 1", "11", "10", "0 ", "  ", then repeat.
    const unsigned long frameMs = 450;
    const unsigned long elapsed = now - displayCycleMs;
    const byte frames = temperatureLength + 2;
    first = int((elapsed / frameMs) % frames) - 1;
  }
  displayPair = (displayDigit(first) << 4) | displayDigit(first + 1);
}

void startNextHalfTurn() {
  if (!pendingHalfTurns || commandedDeg != targetDeg) return;
  if (commandedDeg != HOME_DEG && commandedDeg != AWAY_DEG) {
    targetDeg = HOME_DEG;  // Align after a manual angle before counting a full stroke.
    return;
  }
  targetDeg = commandedDeg == HOME_DEG ? AWAY_DEG : HOME_DEG;
  --pendingHalfTurns;
}

void updateServo(unsigned long now) {
  startNextHalfTurn();
  if (commandedDeg == targetDeg || now - lastStepMs < 20) return;
  lastStepMs = now;
  commandedDeg += targetDeg > commandedDeg ? 1 : -1;
  iceServo.write(commandedDeg);
}

Scene sceneOf(const WonderCamFeatureReader::Result &r) {
  if (r.status != WonderCamFeatureReader::Ok) return Scene::Unknown;
  if (r.id == 0) return Scene::Clear;  // explicit successful "no match" result
  if (r.confidence == 0) return Scene::Unknown;
  if (r.id == CAR) return Scene::Car;
  if (r.id == LEAF_BLOWER) return Scene::Blower;
  if (r.id == PENGUIN || r.id == POLAR_BEAR || r.id == BICYCLE || r.id == HAND_RAKE) return Scene::Clear;
  return Scene::Unknown;
}

void observe(Scene scene, unsigned long now) {
  if (scene != candidate || now - lastSampleMs > MAX_SAMPLE_GAP_MS) {
    candidate = scene;
    candidateSince = now;
  }
  lastSampleMs = now;
  const unsigned long held = now - candidateSince;
  if (scene == Scene::Clear) {
    if (held >= CLEAR_DWELL_MS && latched != Scene::Clear) {
      latched = Scene::Clear;
      saveState();
    }
    return;
  }
  if (!automatic || scene == Scene::Unknown || latched == Scene::Unknown ||
      latched == scene || held < TRIGGER_DWELL_MS) return;
  latched = scene;
  if (temperatureF <= UINT32_MAX - TEMP_RISE_F)
    setTemperature(temperatureF + TEMP_RISE_F, now);
  if (pendingHalfTurns < UINT32_MAX) ++pendingHalfTurns;
  startNextHalfTurn();
  saveState();
  Serial.print(scene == Scene::Car ? F("Car") : F("Electric leaf blower"));
  Serial.print(F(" +20F | T="));
  Serial.print(temperatureF);
  Serial.print(F("F | servo target="));
  Serial.println(targetDeg);
}

const __FlashStringHelper *featureName(byte id) {
  switch (id) {
    case PENGUIN: return F("Penguin");
    case POLAR_BEAR: return F("Polar bear");
    case CAR: return F("Car");
    case BICYCLE: return F("Bicycle");
    case HAND_RAKE: return F("Hand rake");
    case LEAF_BLOWER: return F("Electric leaf blower");
    case 0: return F("No match");
    default: return F("Other");
  }
}

void report() {
  // TE1 temperature commanded-angle target-angle auto feature score [label]
  const int feature = latest.status == WonderCamFeatureReader::Ok ? int(latest.id) :
                      latest.status == WonderCamFeatureReader::WrongMode ? -2 : -1;
  Serial.print(F("TE1 ")); Serial.print(temperatureF); Serial.print(' ');
  Serial.print(commandedDeg); Serial.print(' '); Serial.print(targetDeg); Serial.print(' ');
  Serial.print(automatic ? 1 : 0); Serial.print(' '); Serial.print(feature); Serial.print(' ');
  Serial.print(latest.confidence); Serial.print(' ');
  if (feature >= 0) Serial.println(featureName(latest.id));
  else if (feature == -2) Serial.println(F("Select Feature Learning"));
  else Serial.println(F("Camera read error"));
}

bool parseUnsigned(const char *s, uint32_t &out) {
  if (*s == '\0') return false;
  out = 0;
  while (*s) {
    if (*s < '0' || *s > '9') return false;
    const byte digit = *s++ - '0';
    if (out > (UINT32_MAX - digit) / 10UL) return false;
    out = out * 10UL + digit;
  }
  return true;
}

void suppressCurrentDetection(unsigned long now) {
  if (now - lastSampleMs > MAX_SAMPLE_GAP_MS || candidate == Scene::Unknown) {
    latched = Scene::Unknown;  // Wait for a clear frame sequence after a lost reading.
  } else if (candidate != Scene::Clear) {
    latched = candidate;
  }
}

void handleCommand(const char *cmd, unsigned long now) {
  uint32_t value;
  if (strcmp(cmd, "!STATE") == 0) { report(); return; }
  if (strcmp(cmd, "!RESET") == 0) {
    setTemperature(START_TEMP_F, now);
    pendingHalfTurns = 0;
    targetDeg = HOME_DEG;
    suppressCurrentDetection(now);
    candidate = Scene::Unknown;
  } else if (strncmp(cmd, "!TEMP ", 6) == 0 && parseUnsigned(cmd + 6, value)) {
    setTemperature(value, now);
  } else if (strcmp(cmd, "!WARM") == 0) {
    if (temperatureF > UINT32_MAX - TEMP_RISE_F) { Serial.println(F("ERR RANGE")); return; }
    setTemperature(temperatureF + TEMP_RISE_F, now);
  } else if (strncmp(cmd, "!MOTOR ", 7) == 0 && parseUnsigned(cmd + 7, value) && value <= 180) {
    pendingHalfTurns = 0;
    targetDeg = int(value);
  } else if (strcmp(cmd, "!AUTO 0") == 0 || strcmp(cmd, "!AUTO 1") == 0) {
    const bool enable = cmd[6] == '1';
    if (enable != automatic) {
      automatic = enable;
      if (!enable) pendingHalfTurns = 0;
      if (enable) suppressCurrentDetection(now);
      candidate = Scene::Unknown;
    }
  } else {
    Serial.println(F("ERR COMMAND")); return;
  }
  saveState();
  Serial.print(F("OK ")); Serial.println(cmd);
  report();
}

void readCommands(unsigned long now) {
  static char line[28];
  static byte used = 0;
  static bool discard = false;
  for (byte i = 0; i < 40 && Serial.available(); ++i) {
    const char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (!discard && used) { line[used] = '\0'; handleCommand(line, now); }
      used = 0; discard = false;
    } else if (!used && !discard && (c == 'r' || c == 'R')) {
      handleCommand("!RESET", now);
    } else if (!discard) {
      if (used < sizeof(line) - 1) line[used++] = c;
      else { discard = true; Serial.println(F("ERR LENGTH")); }
    }
  }
}

void setup() {
  Serial.begin(9600);
  EarthState saved;
  if (memory.load(saved)) {
    setTemperature(saved.temperature, millis());
    automatic = saved.automatic;
    latched = saved.latched;
  }
  updateDisplay(millis());
  startDisplay();
  camera.begin();
  iceServo.write(targetDeg);
  iceServo.attach(SERVO_PIN);
  Serial.println(F("Earth ready | 1 Penguin, 2 Polar bear, 3 Bicycle, 4 Car, 5 Hand rake, 6 Electric leaf blower"));
}

void loop() {
  unsigned long now = millis();
  readCommands(now);
  if (now - lastPollMs >= POLL_MS) {
    lastPollMs = now;
    latest = camera.poll();
    now = millis();
    observe(sceneOf(latest), now);
    if (now - lastReportMs >= 500) {
      lastReportMs = now;
      report();
    }
  }
  updateServo(now);
  updateDisplay(now);
}
