#include <xemu_audio_io.h>

#include "../../emu/xloc_emu.h"

void AudioInputI2S2::update() {
  audio_block_t *left = allocate();
  audio_block_t *right = allocate();
  xemu::audio_in_pull(left ? left->data : nullptr, right ? right->data : nullptr,
                      AUDIO_BLOCK_SAMPLES);
  if (left) {
    transmit(left, 0);
    release(left);
  }
  if (right) {
    transmit(right, 1);
    release(right);
  }
}

void AudioOutputI2S2::update() {
  audio_block_t *left = receiveReadOnly(0);
  audio_block_t *right = receiveReadOnly(1);
  xemu::audio_out_push(left ? left->data : nullptr, right ? right->data : nullptr,
                       AUDIO_BLOCK_SAMPLES);
  if (left) release(left);
  if (right) release(right);
}
