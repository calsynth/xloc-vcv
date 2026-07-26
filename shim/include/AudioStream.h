// Host port of the Teensy 4 AudioStream engine for the XLOC2 emulator.
//
// Faithful semantics: refcounted 128-sample int16 blocks from a fixed pool,
// AudioConnection graph, update_all() running every stream's update() in
// construction order. Instead of a DMA-completion software interrupt, the
// update is fired every 128/44100 s of *virtual time* by the emulator clock
// (see xemu::audio_engine_start), under the same lock as the other ISRs.
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

// Engine-wide lock helpers (implemented in shim_audiostream.cpp; they lock
// the emulator's ISR mutex so graph edits exclude the update tick).
void xemu_audio_lock(void);
void xemu_audio_unlock(void);

class AudioStream;

class AudioConnection {
public:
  AudioConnection() {}
  AudioConnection(AudioStream &source, AudioStream &destination)
      : AudioConnection(source, 0, destination, 0) {}
  AudioConnection(AudioStream &source, unsigned char sourceOutput,
                  AudioStream &destination, unsigned char destinationInput) {
    connect(source, sourceOutput, destination, destinationInput);
  }
  ~AudioConnection() { disconnect(); }

  int connect(void);
  int connect(AudioStream &source, AudioStream &destination) {
    return connect(source, 0, destination, 0);
  }
  int connect(AudioStream &source, unsigned char sourceOutput,
              AudioStream &destination, unsigned char destinationInput);
  int disconnect(void);

protected:
  friend class AudioStream;
  AudioStream *src = nullptr;
  AudioStream *dst = nullptr;
  unsigned char src_index = 0;
  unsigned char dest_index = 0;
  AudioConnection *next_dest = nullptr;
  bool isConnected = false;
};

class AudioStream {
public:
  AudioStream(unsigned char ninput, audio_block_t **iqueue)
      : num_inputs(ninput), inputQueue(iqueue) {
    active = false;
    destination_list = nullptr;
    for (int i = 0; i < ninput; ++i) inputQueue[i] = nullptr;
    // Register on the global update list in construction order.
    xemu_audio_lock();
    if (first_update == nullptr) {
      first_update = this;
    } else {
      AudioStream *p;
      for (p = first_update; p->next_update; p = p->next_update) {}
      p->next_update = this;
    }
    next_update = nullptr;
    xemu_audio_unlock();
  }
  virtual ~AudioStream();

  virtual void update(void) = 0;

  static void initialize_memory(audio_block_t *data, unsigned int num);
  bool isActive(void) { return active; }
  uint8_t numInputs(void) const { return num_inputs; }

  // stats (informational)
  static uint16_t memory_used, memory_used_max;
  static uint16_t cpu_cycles_total, cpu_cycles_total_max;
  uint16_t cpu_cycles = 0, cpu_cycles_max = 0;

  static bool update_setup(void);   // begin periodic updates (idempotent)
  static void update_stop(void) {}
  static void update_all(void);     // called by the emulator clock

  static AudioStream *first_update;

protected:
  static audio_block_t *allocate(void);
  static void release(audio_block_t *block);
  void transmit(audio_block_t *block, unsigned char index = 0);
  audio_block_t *receiveReadOnly(unsigned int index = 0);
  audio_block_t *receiveWritable(unsigned int index = 0);

  bool active;
  unsigned char num_inputs;

private:
  friend class AudioConnection;
  AudioConnection *destination_list;
  audio_block_t **inputQueue;
  AudioStream *next_update;
};

// Teensy-style pool declaration
#define AudioMemory(num)                                  \
  {                                                       \
    static audio_block_t xemu_audio_pool_data[num];       \
    AudioStream::initialize_memory(xemu_audio_pool_data, num); \
  }

#define AudioMemoryUsage() (AudioStream::memory_used)
#define AudioMemoryUsageMax() (AudioStream::memory_used_max)
#define AudioMemoryUsageMaxReset() (AudioStream::memory_used_max = AudioStream::memory_used)
#define AudioProcessorUsage() (0.0f)
#define AudioProcessorUsageMax() (0.0f)
#define AudioProcessorUsageMaxReset() ((void)0)

static inline void AudioNoInterrupts() { xemu_audio_lock(); }
static inline void AudioInterrupts() { xemu_audio_unlock(); }
