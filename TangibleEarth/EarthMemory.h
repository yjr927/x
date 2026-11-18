#pragma once
#include <Arduino.h>
#include <EEPROM.h>

// Opening USB serial may restart an Uno. Commit each change to the next slot,
// marking it valid last, so a partial write leaves the previous state usable.
struct __attribute__((packed)) EarthState {
  uint32_t temperature;
  uint8_t angle;
  uint8_t automatic;
  uint8_t carLatched;
};

class EarthMemory {
  struct __attribute__((packed)) Record {
    uint8_t valid, sequence;
    EarthState state;
    uint8_t checksum;
  };
  static const uint8_t SLOTS = 24, VALID = 0xD5;
  int slot = -1;
  Record current = {};

  uint8_t checksum(const Record &r) const {
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&r);
    uint8_t crc = 0;
    for (size_t i = 1; i < sizeof(r) - 1; ++i) {
      crc ^= bytes[i];
      for (uint8_t j = 0; j < 8; ++j) crc = (crc & 0x80) ? (crc << 1) ^ 7 : crc << 1;
    }
    return crc;
  }
public:
  bool load(EarthState &out) {
    for (uint8_t i = 0; i < SLOTS; ++i) {
      Record r;
      EEPROM.get(i * sizeof(r), r);
      if (r.valid != VALID || r.checksum != checksum(r) || r.state.angle > 180 ||
          r.state.automatic > 1 || r.state.carLatched > 1) continue;
      if (slot < 0 || int8_t(r.sequence - current.sequence) > 0) { slot = i; current = r; }
    }
    if (slot < 0) return false;
    out = current.state;
    return true;
  }
  void save(const EarthState &s) {
    if (slot >= 0 && memcmp(&s, &current.state, sizeof(s)) == 0) return;
    slot = (slot + 1) % SLOTS;
    Record next = {0, uint8_t(current.sequence + 1), s, 0};
    next.checksum = checksum(next);
    const int address = slot * sizeof(next);
    EEPROM.update(address, 0);
    EEPROM.put(address, next);
    EEPROM.update(address, VALID);
    current = next;
  }
};
