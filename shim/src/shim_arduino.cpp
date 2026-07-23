// Host implementations of the Arduino/Teensy core API for the XLOC2 emulator.
#include <Arduino.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <thread>

#include "../../emu/xloc_emu.h"

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------
extern "C" uint32_t millis(void) {
  return (uint32_t)(xemu::clock().now_ns.load() / 1000000ull);
}
extern "C" uint32_t micros(void) {
  return (uint32_t)(xemu::clock().now_ns.load() / 1000ull);
}

namespace xemu {
bool self_clocking();
void set_self_clocking(bool);
}

extern "C" void delay(uint32_t ms) {
  if (xemu::self_clocking()) {
    // Boot phase: nobody else advances the clock. Step it ourselves in 1 ms
    // slices so timers fire at their proper times.
    for (uint32_t i = 0; i < ms; ++i) xemu::clock().step(1000000ull);
  } else {
    // Wait (in real time) for virtual time to advance, driven by frontend.
    uint64_t target = xemu::clock().now_ns.load() + (uint64_t)ms * 1000000ull;
    while (xemu::clock().now_ns.load() < target) {
      std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
  }
}

extern "C" void delayMicroseconds(uint32_t us) {
  if (xemu::self_clocking()) {
    xemu::clock().step((uint64_t)us * 1000ull);
  } else {
    uint64_t target = xemu::clock().now_ns.load() + (uint64_t)us * 1000ull;
    while (xemu::clock().now_ns.load() < target) {
      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
  }
}

extern "C" void delayNanoseconds(uint32_t ns) {
  if (xemu::self_clocking()) xemu::clock().step(ns);
  // Otherwise: shorter than our scheduling quantum, treat as no-op.
}

extern "C" void yield(void) { std::this_thread::yield(); }

// ---------------------------------------------------------------------------
// GPIO
// ---------------------------------------------------------------------------
extern "C" void pinMode(uint8_t pin, uint8_t mode) {
  (void)pin;
  (void)mode;
}

extern "C" void digitalWrite(uint8_t pin, uint8_t val) {
  if (pin < xemu::kNumPins) xemu::state().pin_state[pin].store(val ? 1 : 0);
}

extern "C" uint8_t digitalRead(uint8_t pin) {
  if (pin < xemu::kNumPins) return xemu::state().pin_state[pin].load();
  return 1;
}

extern "C" int analogRead(uint8_t pin) {
  if (pin == A17) {
    // ID voltage divider: 0.30 V => CalSynthXL. 10-bit read, 3.3 V ref.
    return (int)(xemu::kIdVoltage / 3.3f * 1023.f + 0.5f);
  }
  return 512;
}

extern "C" void analogWrite(uint8_t, int) {}
extern "C" void analogReadResolution(unsigned int) {}

// Pin-change interrupts: registered but only fired if someone calls
// xemu-side edge injection with handlers attached (T4.1 firmware polls
// triggers in the core ISR, so normally unused).
static void (*g_pin_isr[xemu::kNumPins])(void) = {nullptr};
extern "C" void attachInterrupt(uint8_t pin, void (*fn)(void), int) {
  if (pin < xemu::kNumPins) g_pin_isr[pin] = fn;
}
extern "C" void detachInterrupt(uint8_t pin) {
  if (pin < xemu::kNumPins) g_pin_isr[pin] = nullptr;
}

// ---------------------------------------------------------------------------
// Interrupt control
// ---------------------------------------------------------------------------
// noInterrupts()/interrupts() bracket critical sections. Emulate with the
// shared ISR mutex; recursive so ISRs themselves can call it.
static thread_local int g_irq_depth = 0;
extern "C" void xemu_no_interrupts(void) {
  xemu::clock().isr_mutex.lock();
  ++g_irq_depth;
}
extern "C" void xemu_interrupts(void) {
  if (g_irq_depth > 0) {
    --g_irq_depth;
    xemu::clock().isr_mutex.unlock();
  }
}

// ---------------------------------------------------------------------------
// Random
// ---------------------------------------------------------------------------
static std::mt19937 g_rng{0xC0FFEE};
extern "C" void randomSeed(uint32_t seed) { g_rng.seed(seed ? seed : 1); }
extern "C" long xemu_random_max(long max_) {
  if (max_ <= 0) return 0;
  return (long)(g_rng() % (unsigned long)max_);
}
extern "C" long xemu_random_range(long lo, long hi) {
  if (hi <= lo) return lo;
  return lo + xemu_random_max(hi - lo);
}

// ---------------------------------------------------------------------------
// Memory
// ---------------------------------------------------------------------------
extern "C" void *extmem_malloc(size_t size) { return malloc(size); }
extern "C" void *extmem_calloc(size_t nmemb, size_t size) { return calloc(nmemb, size); }
extern "C" void extmem_free(void *ptr) { free(ptr); }

// ---------------------------------------------------------------------------
// Serial
// ---------------------------------------------------------------------------
FILE *Print::stream_() { return stderr; }

usb_serial_class Serial;
HardwareSerial Serial1, Serial2, Serial3, Serial4, Serial5, Serial6, Serial7, Serial8;
CrashReportClass CrashReport;
usb_midi_class usbMIDI;

// ---------------------------------------------------------------------------
// IntervalTimer
// ---------------------------------------------------------------------------
bool IntervalTimer::begin(void (*fn)(), unsigned int usec) {
  end();
  idx_ = xemu::clock().add_timer(fn, usec);
  return idx_ >= 0;
}

void IntervalTimer::end() {
  if (idx_ >= 0) {
    xemu::clock().remove_timer(idx_);
    idx_ = -1;
  }
}

// ---------------------------------------------------------------------------
// Fake register file (imxrt.h)
// ---------------------------------------------------------------------------
extern "C" uint32_t xemu_cycle_count(void) {
  // 600 MHz core clock on virtual time.
  return (uint32_t)(xemu::clock().now_ns.load() * 3 / 5);
}

static uint32_t g_scratch[8];
extern "C" uint32_t *xemu_scratch_reg(int idx) { return &g_scratch[idx & 7]; }

namespace xemu {
IMXRT_GPIO_t *fake_gpio_port(int idx) {
  static IMXRT_GPIO_t ports[4] = {};
  return &ports[idx & 3];
}

uint32_t &fake_reg(const char *name) {
  static std::map<std::string, uint32_t> regs;
  static std::mutex m;
  std::lock_guard<std::mutex> lk(m);
  return regs[name];
}
}  // namespace xemu
