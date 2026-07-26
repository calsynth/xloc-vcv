// Global objects and linker-symbol stand-ins the firmware expects.
#include <Arduino.h>
#include <Audio.h>
#include <SPI.h>
#include <Wire.h>
#include <smalloc.h>

SPIClass SPI, SPI1, SPI2;
TwoWire Wire, Wire1, Wire2;

// Teensy linker symbols used for free-RAM math: fake a 448 KB RAM2 heap.
static char fake_ram2[448 * 1024];
extern "C" {
char *__brkval = fake_ram2;
// _heap_end / _ebss / _estack are declared as arrays in firmware code;
// providing them as symbols at fixed addresses is enough.
}
// Firmware declares: extern "C" char _heap_end[], *__brkval;
// Define _heap_end as an alias for the end of the fake heap via a small
// assembly-free trick: a one-byte array placed... we can't control layout
// portably, so instead define it as a real array adjacent in memory and fix
// the math in FreeRam by pointing __brkval near it:
extern "C" char _heap_end[1];
char _heap_end[1];
extern "C" char _ebss[1];
char _ebss[1];
extern "C" char _estack;
char _estack;

namespace {
// Set __brkval so that (_heap_end - __brkval) is a healthy positive number.
struct BrkvalInit {
  BrkvalInit() { __brkval = (char *)_heap_end - (448 * 1024); }
} brkval_init;
}  // namespace

// (AudioStream engine statics live in shim_audiostream.cpp)

// Teensy 4.1 PSRAM globals
extern "C" {
uint8_t external_psram_size = 16;  // pretend a 16 MB PSRAM chip is fitted
char _extram_start[1];
void _reboot_Teensyduino_(void) {}
}

// ::ADC utility class (T3-era driver header) — only construction is needed;
// the emulated OC::ADC never touches it.
#include "src/drivers/ADC/OC_util_ADC.h"

static volatile uint32_t xemu_adc_scratch[64];

ADC_Module::ADC_Module(uint8_t ADC_number, const uint8_t *const a_channel2sc1a,
                       const ADC_NLIST *const a_diff_table)
    : ADC_num(ADC_number),
      channel2sc1a(a_channel2sc1a),
      diff_table(a_diff_table),
      ADC_SC1A(&xemu_adc_scratch[0]),
      ADC_SC1B(&xemu_adc_scratch[1]),
      ADC_CFG1(&xemu_adc_scratch[2]),
      ADC_CFG2(&xemu_adc_scratch[3]),
      ADC_RA(&xemu_adc_scratch[4]),
      ADC_RB(&xemu_adc_scratch[5]),
      ADC_CV1(&xemu_adc_scratch[6]),
      ADC_CV2(&xemu_adc_scratch[7]),
      ADC_SC2(&xemu_adc_scratch[8]),
      ADC_SC3(&xemu_adc_scratch[9]),
      ADC_PGA(&xemu_adc_scratch[10]),
      ADC_OFS(&xemu_adc_scratch[11]),
      ADC_PG(&xemu_adc_scratch[12]),
      ADC_MG(&xemu_adc_scratch[13]),
      ADC_CLPD(&xemu_adc_scratch[14]),
      ADC_CLPS(&xemu_adc_scratch[15]),
      ADC_CLP4(&xemu_adc_scratch[16]),
      ADC_CLP3(&xemu_adc_scratch[17]),
      ADC_CLP2(&xemu_adc_scratch[18]),
      ADC_CLP1(&xemu_adc_scratch[19]),
      ADC_CLP0(&xemu_adc_scratch[20]),
      ADC_CLMD(&xemu_adc_scratch[21]),
      ADC_CLMS(&xemu_adc_scratch[22]),
      ADC_CLM4(&xemu_adc_scratch[23]),
      ADC_CLM3(&xemu_adc_scratch[24]),
      ADC_CLM2(&xemu_adc_scratch[25]),
      ADC_CLM1(&xemu_adc_scratch[26]),
      ADC_CLM0(&xemu_adc_scratch[27]),
      PDB0_CHnC1(&xemu_adc_scratch[28]) {}

ADC::ADC()
    : adc0_obj(0, nullptr, nullptr),
#if ADC_NUM_ADCS > 1
      adc1_obj(1, nullptr, nullptr),
#endif
      adc0(&adc0_obj)
#if ADC_NUM_ADCS > 1
      ,
      adc1(&adc1_obj)
#endif
{
}

// smalloc default pool object
extern "C" {
struct smalloc_pool smalloc_curr_pool = {nullptr, 0, 0};
}
