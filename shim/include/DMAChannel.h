// DMAChannel stub. The only real user left after driver replacement is dead
// code paths; TCD fields exist so code compiles.
#pragma once
#include <Arduino.h>

#define DMAMUX_SOURCE_ADC0 40
#define DMAMUX_SOURCE_ADC_ETC 41

typedef struct {
  volatile const void *SADDR;
  int16_t SOFF;
  uint16_t ATTR;
  uint32_t NBYTES_MLNO;
  int32_t SLAST;
  volatile void *DADDR;
  int16_t DOFF;
  uint16_t CITER_ELINKNO;
  int32_t DLASTSGA;
  uint16_t CSR;
  uint16_t BITER_ELINKNO;
  // Aliases used by firmware
  uint16_t BITER;
  uint16_t CITER;
} xemu_dma_tcd_t;

class DMAChannel {
public:
  explicit DMAChannel(bool allocate = true) { (void)allocate; tcd_ = &tcd_storage_; TCD = tcd_; }
  void begin(bool force = false) { (void)force; }
  void enable() {}
  void disable() {}
  bool complete() { return false; }
  void clearComplete() {}
  void triggerAtHardwareEvent(uint8_t) {}
  void attachInterrupt(void (*fn)()) { (void)fn; }
  void detachInterrupt() {}

  xemu_dma_tcd_t *TCD;

private:
  xemu_dma_tcd_t tcd_storage_ = {};
  xemu_dma_tcd_t *tcd_;
};
