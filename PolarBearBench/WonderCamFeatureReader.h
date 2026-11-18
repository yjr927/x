#pragma once

#include <Arduino.h>
#include <Wire.h>

class WonderCamFeatureReader {
 public:
  enum Status : uint8_t { Ok, BusError, WrongMode, InvalidResult };
  struct Result { Status status; uint8_t id; uint16_t confidence; };

  void begin() {
    Wire.begin();
    Wire.setClock(80000UL);
    Wire.setWireTimeout(25000UL, true); // real bus timeout, not Stream timeout
  }

  Result poll() {
    Result result = {BusError, 0, 0};
    uint8_t mode = 0;
    if (!readRegister(0x0035, &mode, 1)) return result;
    if (mode != 4) { result.status = WrongMode; return result; }
    uint8_t bytes[64] = {};
    if (!readRegister(0x0E00, bytes, sizeof(bytes))) return result;
    // Official register table: byte0 is the current mode captured with this
    // result snapshot. Verify it as well as the live mode before/after.
    if (bytes[0] != 4) { result.status = WrongMode; return result; }
    // Reject results if the user switched modes while a read was in progress.
    if (!readRegister(0x0035, &mode, 1)) return result;
    if (mode != 4) { result.status = WrongMode; return result; }
    const uint16_t confidence = uint16_t(bytes[2]) | (uint16_t(bytes[3]) << 8);
    if (confidence > 10000) { result.status = InvalidResult; return result; }
    result.status = Ok;
    result.id = bytes[1];
    result.confidence = confidence;
    return result;
  }

 private:
  static const uint8_t kAddress = 0x32;
  bool readRegister(uint16_t address, uint8_t *destination, uint8_t length) {
    Wire.clearWireTimeoutFlag();
    Wire.beginTransmission(kAddress);
    Wire.write(uint8_t(address & 0xff));
    Wire.write(uint8_t(address >> 8));
    if (Wire.endTransmission() != 0 || Wire.getWireTimeoutFlag()) return false;
    // The vendor driver sets the register once, then reads successive 32-byte
    // chunks with STOP after each. UNO Wire has a 32-byte receive buffer.
    uint8_t offset = 0;
    while (offset < length) {
      const uint8_t remaining = length - offset;
      const uint8_t count = remaining > 32 ? 32 : remaining;
      const uint8_t received = Wire.requestFrom(kAddress, count, uint8_t(true));
      if (received != count || Wire.getWireTimeoutFlag()) return false;
      for (uint8_t i = 0; i < count; ++i) {
        if (!Wire.available()) return false;
        destination[offset++] = uint8_t(Wire.read());
      }
    }
    return true;
  }
};
