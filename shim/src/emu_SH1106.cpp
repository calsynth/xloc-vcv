// Replacement for firmware SH1106_128x64_driver.cpp — pages land in the
// shared emulator framebuffer instead of going out over SPI1.
#include "src/drivers/SH1106_128x64_driver.h"

#include "../../emu/xloc_emu.h"

static uint8_t s_offset = SH1106_128x64_Driver::kDefaultOffset;
static bool s_flip = false;
static uint8_t s_contrast = 0xCF;

/*static*/ void SH1106_128x64_Driver::Init() {
  Clear();
}

/*static*/ void SH1106_128x64_Driver::Clear() {
  uint8_t zeros[kPageSize] = {0};
  for (size_t p = 0; p < kNumPages; ++p) xemu::oled_page((int)p, zeros);
  xemu::oled_flush();
}

/*static*/ void SH1106_128x64_Driver::Flush() {
  xemu::oled_flush();
}

/*static*/ bool SH1106_128x64_Driver::SendPage(uint_fast8_t index, const uint8_t *data) {
  if (s_flip) {
    // 180-degree flip: reverse page order and mirror bits/columns.
    uint8_t flipped[kPageSize];
    for (size_t col = 0; col < kPageSize; ++col) {
      uint8_t b = data[kPageSize - 1 - col];
      // reverse bit order within the byte
      b = (uint8_t)(((b & 0xF0) >> 4) | ((b & 0x0F) << 4));
      b = (uint8_t)(((b & 0xCC) >> 2) | ((b & 0x33) << 2));
      b = (uint8_t)(((b & 0xAA) >> 1) | ((b & 0x55) << 1));
      flipped[col] = b;
    }
    xemu::oled_page((int)(kNumPages - 1 - index), flipped);
  } else {
    xemu::oled_page((int)index, data);
  }
  return true;  // "transfer complete" immediately
}

/*static*/ void SH1106_128x64_Driver::SPI_send(void *, size_t) {}

/*static*/ void SH1106_128x64_Driver::AdjustOffset(uint8_t offset) { s_offset = offset; }
/*static*/ void SH1106_128x64_Driver::ChangeSpeed(uint32_t) {}
/*static*/ void SH1106_128x64_Driver::SetFlipMode(bool flip180) { s_flip = flip180; }
/*static*/ void SH1106_128x64_Driver::SetContrast(uint8_t contrast) { s_contrast = contrast; }
