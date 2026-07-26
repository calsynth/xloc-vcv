// Host implementation of the Teensy AudioStream engine (see AudioStream.h).
#include <AudioStream.h>

#include <vector>

#include "../../emu/xloc_emu.h"

// ---------------------------------------------------------------------------
// Engine lock — the emulator's ISR mutex, so graph edits and block ops
// exclude the update tick (single lock => no ordering inversions).
// ---------------------------------------------------------------------------
void xemu_audio_lock(void) { xemu::clock().isr_mutex.lock(); }
void xemu_audio_unlock(void) { xemu::clock().isr_mutex.unlock(); }

struct AudioLock {
  AudioLock() { xemu_audio_lock(); }
  ~AudioLock() { xemu_audio_unlock(); }
};

// ---------------------------------------------------------------------------
// Block pool
// ---------------------------------------------------------------------------
AudioStream *AudioStream::first_update = nullptr;
uint16_t AudioStream::memory_used = 0;
uint16_t AudioStream::memory_used_max = 0;
uint16_t AudioStream::cpu_cycles_total = 0;
uint16_t AudioStream::cpu_cycles_total_max = 0;

static audio_block_t *pool_data = nullptr;
static std::vector<uint16_t> pool_free;

void AudioStream::initialize_memory(audio_block_t *data, unsigned int num) {
  AudioLock lk;
  pool_data = data;
  pool_free.clear();
  pool_free.reserve(num);
  // LIFO free list; indices in reverse so low indices come out first
  for (unsigned int i = 0; i < num; ++i) {
    data[i].ref_count = 0;
    data[i].memory_pool_index = (uint16_t)i;
    pool_free.push_back((uint16_t)(num - 1 - i));
  }
  memory_used = 0;
  memory_used_max = 0;
}

// Overflow blocks are heap-allocated when the static pool is exhausted and
// are marked with this pool index. On the Teensy, allocate() returning NULL
// is silently tolerated by much firmware code (a NULL deref lands in mapped
// ITCM); on host it would segfault, and we have RAM to spare — never return
// NULL.
static constexpr uint16_t kOverflowBlock = 0xFFFF;

audio_block_t *AudioStream::allocate(void) {
  AudioLock lk;
  audio_block_t *b;
  if (pool_data && !pool_free.empty()) {
    uint16_t idx = pool_free.back();
    pool_free.pop_back();
    b = &pool_data[idx];
  } else {
    b = new audio_block_t();
    b->memory_pool_index = kOverflowBlock;
  }
  b->ref_count = 1;
  ++memory_used;
  if (memory_used > memory_used_max) memory_used_max = memory_used;
  return b;
}

void AudioStream::release(audio_block_t *block) {
  if (!block) return;
  AudioLock lk;
  if (block->ref_count > 1) {
    --block->ref_count;
  } else {
    block->ref_count = 0;
    if (block->memory_pool_index == kOverflowBlock) {
      delete block;
    } else {
      pool_free.push_back(block->memory_pool_index);
    }
    if (memory_used > 0) --memory_used;
  }
}

// ---------------------------------------------------------------------------
// Transmit / receive
// ---------------------------------------------------------------------------
void AudioStream::transmit(audio_block_t *block, unsigned char index) {
  if (!block) return;
  AudioLock lk;
  for (AudioConnection *c = destination_list; c; c = c->next_dest) {
    if (c->src_index == index) {
      if (c->dst->inputQueue[c->dest_index] == nullptr) {
        c->dst->inputQueue[c->dest_index] = block;
        ++block->ref_count;
      }
    }
  }
}

audio_block_t *AudioStream::receiveReadOnly(unsigned int index) {
  AudioLock lk;
  if (index >= num_inputs) return nullptr;
  audio_block_t *in = inputQueue[index];
  inputQueue[index] = nullptr;
  return in;
}

audio_block_t *AudioStream::receiveWritable(unsigned int index) {
  AudioLock lk;
  if (index >= num_inputs) return nullptr;
  audio_block_t *in = inputQueue[index];
  inputQueue[index] = nullptr;
  if (in && in->ref_count > 1) {
    audio_block_t *p = allocate();
    if (p) memcpy(p->data, in->data, sizeof(p->data));
    // release our reference to the shared original
    --in->ref_count;
    in = p;
  }
  return in;
}

// ---------------------------------------------------------------------------
// Stream teardown (applet swapping deletes objects & connections)
// ---------------------------------------------------------------------------
AudioStream::~AudioStream() {
  AudioLock lk;
  // Drop any pending input blocks
  for (int i = 0; i < num_inputs; ++i) {
    if (inputQueue[i]) {
      release(inputQueue[i]);
      inputQueue[i] = nullptr;
    }
  }
  // Disconnect all outgoing connections
  while (destination_list) destination_list->disconnect();
  // Disconnect any incoming connections (scan all streams)
  for (AudioStream *p = first_update; p; p = p->next_update) {
    if (p == this) continue;
    AudioConnection *c = p->destination_list;
    while (c) {
      AudioConnection *next = c->next_dest;
      if (c->dst == this) c->disconnect();
      c = next;
    }
  }
  // Remove from update list
  if (first_update == this) {
    first_update = next_update;
  } else {
    for (AudioStream *p = first_update; p; p = p->next_update) {
      if (p->next_update == this) {
        p->next_update = next_update;
        break;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Connections
// ---------------------------------------------------------------------------
int AudioConnection::connect(AudioStream &source, unsigned char sourceOutput,
                             AudioStream &destination, unsigned char destinationInput) {
  AudioLock lk;
  if (isConnected) disconnect();
  if (destinationInput >= destination.num_inputs) return 1;
  src = &source;
  dst = &destination;
  src_index = sourceOutput;
  dest_index = destinationInput;
  // append to source's destination list (order matters on hardware)
  next_dest = nullptr;
  if (!source.destination_list) {
    source.destination_list = this;
  } else {
    AudioConnection *p = source.destination_list;
    while (p->next_dest) {
      if (p == this) return 2;  // already listed
      p = p->next_dest;
    }
    if (p == this) return 2;
    p->next_dest = this;
  }
  source.active = true;
  destination.active = true;
  isConnected = true;
  return 0;
}

int AudioConnection::connect(void) {
  if (!src || !dst) return 1;
  return connect(*src, src_index, *dst, dest_index);
}

int AudioConnection::disconnect(void) {
  AudioLock lk;
  if (!isConnected || !src) {
    isConnected = false;
    return 1;
  }
  AudioConnection **pp = &src->destination_list;
  while (*pp && *pp != this) pp = &(*pp)->next_dest;
  if (*pp) *pp = next_dest;
  next_dest = nullptr;
  // If a block is waiting on the destination input from us, leave it; the
  // destination will consume or leak-to-pool naturally on next update.
  isConnected = false;
  return 0;
}

// ---------------------------------------------------------------------------
// Update scheduling
// ---------------------------------------------------------------------------
void AudioStream::update_all(void) {
  // Called from the emulator clock (ISR lock already held by the timer
  // dispatcher, but the lock is recursive so this is safe regardless).
  AudioLock lk;
  for (AudioStream *p = first_update; p; p = p->next_update) {
    if (p->active) p->update();
  }
}

bool AudioStream::update_setup(void) {
  xemu::audio_engine_start();
  return true;
}

// Bridge for the emulator clock (xloc_emu.cpp fires this from its timer).
extern "C" void xemu_audio_update_all() { AudioStream::update_all(); }
