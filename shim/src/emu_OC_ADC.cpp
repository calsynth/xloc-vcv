// Replacement for firmware OC_ADC.cpp — feeds ADC values from the virtual
// CV inputs instead of the ADC33131D/FlexIO hardware scan.
//
// Scaling matches the real XLOC2 analog front end (verified against the
// desktop emulator): raw12 = 2730 - V * 409.6, i.e. 0 V reads 2730 and each
// volt is 409.6 counts downward (input stage is inverting).
#include "OC_ADC.h"

#include "OC_gpio.h"
#include "OC_io.h"

#include "../../emu/xloc_emu.h"

ADC_CHANNEL ADC_CHANNEL_1 = 0, ADC_CHANNEL_2 = 1, ADC_CHANNEL_3 = 2, ADC_CHANNEL_4 = 3;
#if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)
ADC_CHANNEL ADC_CHANNEL_5 = 4, ADC_CHANNEL_6 = 5, ADC_CHANNEL_7 = 6, ADC_CHANNEL_8 = 7;
#endif

namespace OC {

/*static*/ ::ADC ADC::adc_;
/*static*/ ADC::CalibrationData *ADC::calibration_data_;
/*static*/ uint32_t ADC::raw_[ADC_CHANNEL_COUNT];
/*static*/ uint32_t ADC::smoothed_[ADC_CHANNEL_COUNT];

/*static*/ void ADC::Init(CalibrationData *calibration_data, bool flip180) {
  (void)flip180;  // XLOC2 remap already applied via ADC_CHANNEL_* globals
  calibration_data_ = calibration_data;

  // Ideal XLOC2 calibration: 0 V input reads raw12 = 2730 on every channel.
  for (int i = 0; i < ADC_CHANNEL_COUNT; ++i) calibration_data_->offset[i] = 2730;

  static constexpr uint16_t kZero = 2730u << kAdcSmoothBits;
  std::fill(raw_, raw_ + ADC_CHANNEL_COUNT, kZero);
  std::fill(smoothed_, smoothed_ + ADC_CHANNEL_COUNT, kZero);
}

#if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)
/*static*/ void ADC::ADC33131D_Vref_calibrate() {
  // Real chip needs ~1150 ms; emulation needs none of it.
}
#endif

/*static*/ void ADC::Init_DMA() {}
/*static*/ void ADC::DMA_ISR() {}

/*static*/ void ADC::Scan_DMA() {
  // Rate-limit like the hardware path: effective 180 us update rate.
  static int ratelimit = 0;
  if (++ratelimit < 3) return;
  ratelimit = 0;

  // xemu::adc_read_raw(n) returns panel CV jack n+1; update<ADC_CHANNEL_n>
  // stores it at the storage index the XLOC2 remap assigned to that jack.
  update<ADC_CHANNEL_1>(xemu::adc_read_raw(0));
  update<ADC_CHANNEL_2>(xemu::adc_read_raw(1));
  update<ADC_CHANNEL_3>(xemu::adc_read_raw(2));
  update<ADC_CHANNEL_4>(xemu::adc_read_raw(3));
  update<ADC_CHANNEL_5>(xemu::adc_read_raw(4));
  update<ADC_CHANNEL_6>(xemu::adc_read_raw(5));
  update<ADC_CHANNEL_7>(xemu::adc_read_raw(6));
  update<ADC_CHANNEL_8>(xemu::adc_read_raw(7));
}

/*static*/ void ADC::Read(IOFrame *ioframe) {
  for (int channel = 0; channel < ADC_CHANNEL_COUNT; ++channel) {
    ioframe->cv.values[channel] = value(static_cast<ADC_CHANNEL>(channel));
    ioframe->cv.pitch_values[channel] = value_to_pitch(ioframe->cv.values[channel]);
  }
}

/*static*/ float ADC::Read_ID_Voltage() { return xemu::kIdVoltage; }

/*static*/ void ADC::CalibratePitch(int32_t c2, int32_t c4) {
  if (c2 < c4) {
    int32_t scale = (24 * 128 * 4096L) / (c4 - c2);
    calibration_data_->pitch_cv_scale = scale;
  }
}

}  // namespace OC
