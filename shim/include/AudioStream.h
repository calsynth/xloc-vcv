// Teensy AudioStream stub for the XLOC2 emulator (CV-only phase).
//
// The audio graph compiles and objects exist, but no scheduler ever calls
// update(), so audio applets are silent by design (matching phase 1/2 of the
// desktop emulator). Phase 3 would add a host-side 44.1 kHz block scheduler.
#pragma once

#include <stdint.h>
#include <string.h>

#ifndef AUDIO_BLOCK_SAMPLES
#define AUDIO_BLOCK_SAMPLES 128
#endif
#define AUDIO_SAMPLE_RATE 44100.0f
#define AUDIO_SAMPLE_RATE_EXACT 44100.0f

typedef struct audio_block_struct {
  uint8_t ref_count;
  uint8_t reserved1;
  uint16_t memory_pool_index;
  int16_t data[AUDIO_BLOCK_SAMPLES];
} audio_block_t;

class AudioConnection;

class AudioStream {
public:
  AudioStream(unsigned char ninput, audio_block_t **iqueue)
      : num_inputs(ninput), inputQueue(iqueue) {
    active = false;
    if (iqueue) {
      for (int i = 0; i < ninput; ++i) iqueue[i] = nullptr;
    }
  }
  virtual ~AudioStream() {}
  virtual void update() = 0;

  bool isActive() { return active; }
  uint8_t numInputs() const { return num_inputs; }

  static uint16_t memory_used, memory_used_max;
  static uint16_t cpu_cycles_total, cpu_cycles_total_max;
  uint16_t cpu_cycles = 0, cpu_cycles_max = 0;

protected:
  // Block pool: allocation works (some applets build blocks outside update()),
  // transmit/receive are inert because no scheduler runs.
  static audio_block_t *allocate();
  static void release(audio_block_t *block);
  void transmit(audio_block_t *block, unsigned char index = 0) {
    (void)block;
    (void)index;
  }
  audio_block_t *receiveReadOnly(unsigned int index = 0) {
    (void)index;
    return nullptr;
  }
  audio_block_t *receiveWritable(unsigned int index = 0) {
    (void)index;
    return nullptr;
  }

  bool active;
  unsigned char num_inputs;
  audio_block_t **inputQueue;

  friend class AudioConnection;
};

class AudioConnection {
public:
  AudioConnection() {}
  AudioConnection(AudioStream &source, AudioStream &destination) {
    (void)source;
    (void)destination;
  }
  AudioConnection(AudioStream &source, unsigned char sourceOutput,
                  AudioStream &destination, unsigned char destinationInput) {
    (void)source;
    (void)sourceOutput;
    (void)destination;
    (void)destinationInput;
  }
  ~AudioConnection() {}
  int connect() { return 0; }
  int connect(AudioStream &, AudioStream &) { return 0; }
  int connect(AudioStream &, unsigned char, AudioStream &, unsigned char) { return 0; }
  int disconnect() { return 0; }
};

#define AudioMemory(num) ((void)0)
#define AudioMemoryUsage() (0)
#define AudioMemoryUsageMax() (0)
#define AudioMemoryUsageMaxReset() ((void)0)
#define AudioProcessorUsage() (0.0f)
#define AudioProcessorUsageMax() (0.0f)
#define AudioProcessorUsageMaxReset() ((void)0)

static inline void AudioNoInterrupts() {}
static inline void AudioInterrupts() {}
