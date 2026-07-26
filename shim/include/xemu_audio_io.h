// Host I2S2 codec bridge: the firmware's audio graph talks to these exactly
// like the Teensy classes; frames move through xemu's 44.1 kHz rings to the
// frontend (VCV Rack AUDIO jacks or the headless harness).
#pragma once

#include <AudioStream.h>

class AudioInputI2S2 : public AudioStream {
public:
  AudioInputI2S2() : AudioStream(0, nullptr) {
    begin();
  }
  void begin() {
    active = true;             // inputs run unconditionally, like hardware DMA
    update_setup();            // make sure the engine tick is running
  }
  void update() override;
};

class AudioOutputI2S2 : public AudioStream {
public:
  AudioOutputI2S2() : AudioStream(2, iq_) {
    begin();
  }
  void begin() {
    active = true;
    update_setup();
  }
  void update() override;

private:
  audio_block_t *iq_[2];
};

// USB audio endpoints stay inert (no USB in the emulator).
class AudioInputUSB : public AudioStream {
public:
  AudioInputUSB() : AudioStream(0, nullptr) {}
  void update() override {}
  float volume() { return 1.f; }
};

class AudioOutputUSB : public AudioStream {
public:
  AudioOutputUSB() : AudioStream(2, iq_) {}
  void update() override {
    // consume and drop, so blocks don't pool up
    audio_block_t *b;
    if ((b = receiveReadOnly(0))) release(b);
    if ((b = receiveReadOnly(1))) release(b);
  }

private:
  audio_block_t *iq_[2];
};
