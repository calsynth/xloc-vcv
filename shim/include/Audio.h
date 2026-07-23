// Teensy Audio Library stub for the XLOC2 emulator (CV-only phase).
// Classes exist so audio applets compile and hold state; no DSP runs.
#pragma once

#include <Arduino.h>
#include <AudioStream.h>
#include <arm_math.h>

// Waveform types (synth_waveform.h)
#define WAVEFORM_SINE 0
#define WAVEFORM_SAWTOOTH 1
#define WAVEFORM_SQUARE 2
#define WAVEFORM_TRIANGLE 3
#define WAVEFORM_ARBITRARY 4
#define WAVEFORM_PULSE 5
#define WAVEFORM_SAWTOOTH_REVERSE 6
#define WAVEFORM_SAMPLE_HOLD 7
#define WAVEFORM_TRIANGLE_VARIABLE 8
#define WAVEFORM_BANDLIMIT_SAWTOOTH 9
#define WAVEFORM_BANDLIMIT_SAWTOOTH_REVERSE 10
#define WAVEFORM_BANDLIMIT_SQUARE 11
#define WAVEFORM_BANDLIMIT_PULSE 12

// ---------------------------------------------------------------------------
// Synth sources
// ---------------------------------------------------------------------------
class AudioSynthWaveform : public AudioStream {
public:
  AudioSynthWaveform() : AudioStream(0, nullptr) {}
  void update() override {}
  void begin(short waveform) { waveform_ = waveform; }
  void begin(float amp, float freq, short waveform) {
    amplitude_ = amp; frequency_ = freq; waveform_ = waveform;
  }
  void frequency(float f) { frequency_ = f; }
  void amplitude(float a) { amplitude_ = a; }
  void offset(float o) { offset_ = o; }
  void phase(float deg) { phase_ = deg; }
  void pulseWidth(float w) { pw_ = w; }
  void arbitraryWaveform(const int16_t *data, float maxFreq) {
    (void)data; (void)maxFreq;
  }
protected:
  float frequency_ = 440.f, amplitude_ = 0.f, offset_ = 0.f, phase_ = 0.f, pw_ = 0.5f;
  short waveform_ = 0;
};

class AudioSynthWaveformModulated : public AudioStream {
public:
  AudioSynthWaveformModulated() : AudioStream(2, iq_) {}
  void update() override {}
  void begin(short waveform) { waveform_ = waveform; }
  void begin(float amp, float freq, short waveform) {
    amplitude_ = amp; frequency_ = freq; waveform_ = waveform;
  }
  void frequency(float f) { frequency_ = f; }
  void amplitude(float a) { amplitude_ = a; }
  void offset(float o) { offset_ = o; }
  void frequencyModulation(float octaves) { fm_octaves_ = octaves; }
  void phaseModulation(float degrees) { pm_deg_ = degrees; }
  void arbitraryWaveform(const int16_t *data, float maxFreq) {
    (void)data; (void)maxFreq;
  }
protected:
  audio_block_t *iq_[2];
  float frequency_ = 440.f, amplitude_ = 0.f, offset_ = 0.f;
  float fm_octaves_ = 8.f, pm_deg_ = 180.f;
  short waveform_ = 0;
};

class AudioSynthWaveformDc : public AudioStream {
public:
  AudioSynthWaveformDc() : AudioStream(0, nullptr) {}
  void update() override {}
  void amplitude(float a) { amplitude_ = a; }
  void amplitude(float a, float ms) { (void)ms; amplitude_ = a; }
  float read() { return amplitude_; }
protected:
  float amplitude_ = 0.f;
};

class AudioSynthNoiseWhite : public AudioStream {
public:
  AudioSynthNoiseWhite() : AudioStream(0, nullptr) {}
  void update() override {}
  void amplitude(float a) { amplitude_ = a; }
protected:
  float amplitude_ = 0.f;
};

class AudioSynthNoisePink : public AudioSynthNoiseWhite {};

class AudioSynthKarplusStrong : public AudioStream {
public:
  AudioSynthKarplusStrong() : AudioStream(0, nullptr) {}
  void update() override {}
  void noteOn(float freq, float velocity) { (void)freq; (void)velocity; }
  void noteOff(float velocity) { (void)velocity; }
};

// ---------------------------------------------------------------------------
// Filters
// ---------------------------------------------------------------------------
class AudioFilterStateVariable : public AudioStream {
public:
  AudioFilterStateVariable() : AudioStream(2, iq_) {}
  void update() override {}
  void frequency(float f) { (void)f; }
  void resonance(float r) { (void)r; }
  void octaveControl(float o) { (void)o; }
protected:
  audio_block_t *iq_[2];
};

class AudioFilterLadder : public AudioStream {
public:
  AudioFilterLadder() : AudioStream(3, iq_) {}
  void update() override {}
  void frequency(float f) { (void)f; }
  void resonance(float r) { (void)r; }
  void octaveControl(float o) { (void)o; }
  void inputDrive(float d) { (void)d; }
  void passbandGain(float g) { (void)g; }
  void interpolationMethod(int m) { (void)m; }
protected:
  audio_block_t *iq_[3];
};

class AudioFilterBiquad : public AudioStream {
public:
  AudioFilterBiquad() : AudioStream(1, iq_) {}
  void update() override {}
  void setCoefficients(uint32_t stage, const double *coeffs) { (void)stage; (void)coeffs; }
  void setCoefficients(uint32_t stage, const int *coeffs) { (void)stage; (void)coeffs; }
  void setLowpass(uint32_t stage, float freq, float q = 0.7071f) { (void)stage; (void)freq; (void)q; }
  void setHighpass(uint32_t stage, float freq, float q = 0.7071f) { (void)stage; (void)freq; (void)q; }
  void setBandpass(uint32_t stage, float freq, float q = 1.0f) { (void)stage; (void)freq; (void)q; }
  void setNotch(uint32_t stage, float freq, float q = 1.0f) { (void)stage; (void)freq; (void)q; }
  void setLowShelf(uint32_t stage, float freq, float gain, float slope = 1.0f) { (void)stage; (void)freq; (void)gain; (void)slope; }
  void setHighShelf(uint32_t stage, float freq, float gain, float slope = 1.0f) { (void)stage; (void)freq; (void)gain; (void)slope; }
protected:
  audio_block_t *iq_[1];
};

// ---------------------------------------------------------------------------
// Mixers / amps
// ---------------------------------------------------------------------------
class AudioMixer4 : public AudioStream {
public:
  AudioMixer4() : AudioStream(4, iq_) {
    for (auto &g : gains_) g = 1.f;
  }
  void update() override {}
  void gain(unsigned int channel, float g) {
    if (channel < 4) gains_[channel] = g;
  }
protected:
  audio_block_t *iq_[4];
  float gains_[4];
};

class AudioAmplifier : public AudioStream {
public:
  AudioAmplifier() : AudioStream(1, iq_) {}
  void update() override {}
  void gain(float g) { gain_ = g; }
protected:
  audio_block_t *iq_[1];
  float gain_ = 1.f;
};

// ---------------------------------------------------------------------------
// Analyzers
// ---------------------------------------------------------------------------
class AudioAnalyzePeak : public AudioStream {
public:
  AudioAnalyzePeak() : AudioStream(1, iq_) {}
  void update() override {}
  bool available() { return false; }
  float read() { return 0.f; }
  float readPeakToPeak() { return 0.f; }
protected:
  audio_block_t *iq_[1];
};

class AudioAnalyzeRMS : public AudioStream {
public:
  AudioAnalyzeRMS() : AudioStream(1, iq_) {}
  void update() override {}
  bool available() { return false; }
  float read() { return 0.f; }
protected:
  audio_block_t *iq_[1];
};

class AudioAnalyzeNoteFrequency : public AudioStream {
public:
  AudioAnalyzeNoteFrequency() : AudioStream(1, iq_) {}
  void update() override {}
  void begin(float threshold) { (void)threshold; }
  void threshold(float t) { (void)t; }
  bool available() { return false; }
  float read() { return 0.f; }
  float probability() { return 0.f; }
protected:
  audio_block_t *iq_[1];
};

// ---------------------------------------------------------------------------
// Effects
// ---------------------------------------------------------------------------
class AudioEffectFreeverb : public AudioStream {
public:
  AudioEffectFreeverb() : AudioStream(1, iq_) {}
  void update() override {}
  void roomsize(float n) { (void)n; }
  void damping(float n) { (void)n; }
protected:
  audio_block_t *iq_[1];
};

class AudioEffectFreeverbStereo : public AudioStream {
public:
  AudioEffectFreeverbStereo() : AudioStream(1, iq_) {}
  void update() override {}
  void roomsize(float n) { (void)n; }
  void damping(float n) { (void)n; }
protected:
  audio_block_t *iq_[1];
};

class AudioEffectWaveFolder : public AudioStream {
public:
  AudioEffectWaveFolder() : AudioStream(2, iq_) {}
  void update() override {}
protected:
  audio_block_t *iq_[2];
};

class AudioEffectDelay : public AudioStream {
public:
  AudioEffectDelay() : AudioStream(1, iq_) {}
  void update() override {}
  void delay(uint8_t channel, float ms) { (void)channel; (void)ms; }
  void disable(uint8_t channel) { (void)channel; }
protected:
  audio_block_t *iq_[1];
};

// ---------------------------------------------------------------------------
// Queues
// ---------------------------------------------------------------------------
class AudioRecordQueue : public AudioStream {
public:
  AudioRecordQueue() : AudioStream(1, iq_) {}
  void update() override {}
  void begin() {}
  void end() {}
  int available() { return 0; }
  void clear() {}
  int16_t *readBuffer() { return nullptr; }
  void freeBuffer() {}
protected:
  audio_block_t *iq_[1];
};

class AudioPlayQueue : public AudioStream {
public:
  AudioPlayQueue() : AudioStream(0, nullptr) {}
  void update() override {}
  int16_t *getBuffer() { return buffer_; }
  void playBuffer() {}
  bool available() { return true; }
protected:
  int16_t buffer_[AUDIO_BLOCK_SAMPLES] = {0};
};

// ---------------------------------------------------------------------------
// I/O endpoints (inert)
// ---------------------------------------------------------------------------
class AudioInputI2S2 : public AudioStream {
public:
  AudioInputI2S2() : AudioStream(0, nullptr) {}
  void update() override {}
  void begin() {}
};

class AudioOutputI2S2 : public AudioStream {
public:
  AudioOutputI2S2() : AudioStream(2, iq_) {}
  void update() override {}
  void begin() {}
protected:
  audio_block_t *iq_[2];
};

class AudioInputUSB : public AudioStream {
public:
  AudioInputUSB() : AudioStream(0, nullptr) {}
  void update() override {}
  float volume() { return 1.f; }
};

class AudioOutputUSB : public AudioStream {
public:
  AudioOutputUSB() : AudioStream(2, iq_) {}
  void update() override {}
protected:
  audio_block_t *iq_[2];
};
