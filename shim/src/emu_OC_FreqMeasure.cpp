// Replacement for OC_FreqMeasure.cpp — frequency measurement on trigger
// inputs (used by the autotuner). No signal to measure in CV-only mode.
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"

FreqMeasureClass *FreqMeasureClass::pin_inst[4] = {nullptr, nullptr, nullptr, nullptr};

void FreqMeasureClass::begin(uint8_t pin) {
  (void)pin;
  running = true;
  buffer_head = buffer_tail = 0;
}

uint8_t FreqMeasureClass::available(void) { return 0; }

uint32_t FreqMeasureClass::read(void) { return 0; }

void FreqMeasureClass::end(void) { running = false; }

void FreqMeasureClass::isr() {}

FreqMeasureClass FreqMeasure;
