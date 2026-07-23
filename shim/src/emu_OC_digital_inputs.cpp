// Replacement for firmware OC_digital_inputs.cpp (T4.1 branch uses GPIO edge
// ISR registers directly). Here we software-detect edges on the virtual
// trigger pins each Scan (called from the core ISR @16.6 kHz — that's better
// timing resolution than any patch needs).
#include "OC_digital_inputs.h"

#include "OC_gpio.h"

#include "../../emu/xloc_emu.h"

namespace OC {

/*static*/ uint32_t DigitalInputs::rising_edges_;
/*static*/ uint32_t DigitalInputs::raised_mask_;
/*static*/ IMXRT_GPIO_t *DigitalInputs::port[DIGITAL_INPUT_LAST];
/*static*/ uint32_t DigitalInputs::bitmask[DIGITAL_INPUT_LAST];

static uint8_t s_prev[DIGITAL_INPUT_LAST];

/*static*/ void DigitalInputs::Init() {
  rising_edges_ = 0;
  raised_mask_ = 0;
  for (int i = 0; i < DIGITAL_INPUT_LAST; ++i) s_prev[i] = 0;
}

/*static*/ void DigitalInputs::Scan() {
  uint32_t rising = 0;
  uint32_t raised = 0;
  for (int i = 0; i < DIGITAL_INPUT_LAST; ++i) {
    bool now = read_immediate(static_cast<DigitalInput>(i));
    if (now && !s_prev[i]) rising |= DIGITAL_INPUT_MASK(i);
    if (now) raised |= DIGITAL_INPUT_MASK(i);
    s_prev[i] = now ? 1 : 0;
  }
  rising_edges_ = rising;
  raised_mask_ = raised;
}

}  // namespace OC
