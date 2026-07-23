// SPI stub. The DAC path writes LPSPI4_TDR directly (decoded in imxrt.h
// proxy); the OLED driver is replaced. So SPI here is inert bookkeeping.
#pragma once
#include <Arduino.h>

#define SPI_MODE0 0
#define SPI_MODE1 1
#define SPI_MODE2 2
#define SPI_MODE3 3

class SPISettings {
public:
  SPISettings() {}
  SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) {
    (void)clock; (void)bitOrder; (void)dataMode;
  }
};

class SPIClass {
public:
  void begin() {}
  void end() {}
  void beginTransaction(SPISettings) {}
  void endTransaction() {}
  uint8_t transfer(uint8_t data) { return data; }
  uint16_t transfer16(uint16_t data) { return data; }
  void transfer(void *buf, size_t count) { (void)buf; (void)count; }
  void setMOSI(uint8_t) {}
  void setMISO(uint8_t) {}
  void setSCK(uint8_t) {}
  void setCS(uint8_t) {}
};

extern SPIClass SPI, SPI1, SPI2;
