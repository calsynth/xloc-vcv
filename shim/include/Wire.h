// I2C stub — no devices on the virtual bus; endTransmission returns NACK.
#pragma once
#include <Arduino.h>

class TwoWire : public Stream {
public:
  void begin() {}
  void begin(uint8_t) {}
  void end() {}
  void setClock(uint32_t) {}
  void beginTransmission(uint8_t) {}
  uint8_t endTransmission(bool = true) { return 2; }  // 2 = addr NACK
  uint8_t requestFrom(uint8_t, uint8_t, bool = true) { return 0; }
  size_t write(uint8_t) override { return 1; }
  using Print::write;
  int available() override { return 0; }
  int read() override { return -1; }
};

extern TwoWire Wire, Wire1, Wire2;
