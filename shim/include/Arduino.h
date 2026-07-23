// Host-side Arduino/Teensy core shim for the XLOC2 emulator.
// Provides just enough of the Teensyduino API surface for the Phazerville
// firmware to compile and run unmodified on a desktop host.
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Attribute / memory-section macros (no-ops on host)
// ---------------------------------------------------------------------------
#ifndef FLASHMEM
#define FLASHMEM
#endif
#ifndef FASTRUN
#define FASTRUN
#endif
#ifndef DMAMEM
#define DMAMEM
#endif
#ifndef EXTMEM
#define EXTMEM
#endif
#ifndef PROGMEM
#define PROGMEM
#endif
#define PGM_P const char *
#define PSTR(s) (s)

#define F_CPU 600000000
#define F_CPU_ACTUAL 600000000
#define F_BUS_ACTUAL 150000000

#include <imxrt.h>

// ---------------------------------------------------------------------------
// Basic types & constants
// ---------------------------------------------------------------------------
typedef bool boolean;
typedef uint8_t byte;

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define INPUT_PULLDOWN 3
#define OUTPUT_OPENDRAIN 4
#define INPUT_DISABLE 5

#define LSBFIRST 0
#define MSBFIRST 1

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

#define CHANGE 4
#define FALLING 2
#define RISING 3

// Analog pin aliases (Teensy 4.1)
#define A0 14
#define A1 15
#define A2 16
#define A3 17
#define A4 18
#define A5 19
#define A6 20
#define A7 21
#define A8 22
#define A9 23
#define A10 24
#define A11 25
#define A12 26
#define A13 27
#define A14 38
#define A15 39
#define A16 40
#define A17 41

#define LED_BUILTIN 13
#define BUILTIN_SDCARD 254

// min/max/abs/constrain as macros would break <algorithm>; Teensyduino for T4
// uses template functions in C++. Provide the common Arduino helpers:
#ifdef __cplusplus
#include <algorithm>
#include <type_traits>
// Teensy 4 core defines heterogeneous min/max templates in the global
// namespace (std::min/std::max still work when qualified).
template <class A, class B>
static constexpr auto min(A a, B b) -> decltype(a < b ? a : b) {
  return a < b ? a : b;
}
template <class A, class B>
static constexpr auto max(A a, B b) -> decltype(a > b ? a : b) {
  return a > b ? a : b;
}
template <typename T, typename L, typename H>
static inline T constrain(T x, L lo, H hi) {
  return x < (T)lo ? (T)lo : (x > (T)hi ? (T)hi : x);
}
#endif

static inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// ---------------------------------------------------------------------------
// Timing (virtual clock)
// ---------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif
uint32_t millis(void);
uint32_t micros(void);
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void delayNanoseconds(uint32_t ns);
void yield(void);
#ifdef __cplusplus
}
#endif

// ---------------------------------------------------------------------------
// GPIO
// ---------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
uint8_t digitalRead(uint8_t pin);
int analogRead(uint8_t pin);
void analogWrite(uint8_t pin, int val);
void analogReadResolution(unsigned int bits);
void attachInterrupt(uint8_t pin, void (*fn)(void), int mode);
void detachInterrupt(uint8_t pin);
#ifdef __cplusplus
}
#endif
#define digitalReadFast(pin) digitalRead(pin)
#define digitalWriteFast(pin, val) digitalWrite(pin, val)
static inline uint8_t digitalPinToInterrupt(uint8_t pin) { return pin; }

// ---------------------------------------------------------------------------
// Interrupt control — emulated with a global recursive mutex shared with the
// virtual-timer ISRs (see xloc_emu).
// ---------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif
void xemu_no_interrupts(void);
void xemu_interrupts(void);
#ifdef __cplusplus
}
#endif
#define noInterrupts() xemu_no_interrupts()
#define interrupts() xemu_interrupts()
#define __disable_irq() xemu_no_interrupts()
#define __enable_irq() xemu_interrupts()
#define cli() xemu_no_interrupts()
#define sei() xemu_interrupts()

#define NVIC_SET_PRIORITY(irq, prio) ((void)0)
#define NVIC_ENABLE_IRQ(irq) ((void)0)
#define NVIC_DISABLE_IRQ(irq) ((void)0)

// CMSIS-style intrinsics
#ifdef __cplusplus
#include <atomic>
static inline void __DMB() { std::atomic_thread_fence(std::memory_order_seq_cst); }
static inline uint32_t __LDREXW(volatile uint32_t *addr) { return *addr; }
static inline uint32_t __STREXW(uint32_t value, volatile uint32_t *addr) {
  *addr = value;
  return 0;  // always succeeds (host locking handled by the ISR mutex)
}
static inline void __CLREX() {}
static inline int32_t __SSAT(int32_t val, uint32_t sat) {
  const int32_t lim = (1 << (sat - 1));
  if (val >= lim) return lim - 1;
  if (val < -lim) return -lim;
  return val;
}
static inline uint32_t __USAT(int32_t val, uint32_t sat) {
  const int32_t lim = (1 << sat) - 1;
  if (val < 0) return 0;
  if (val > lim) return (uint32_t)lim;
  return (uint32_t)val;
}
#endif

// DWT cycle counter (profiling): derive from virtual time @600 MHz.
#ifdef __cplusplus
extern "C" uint32_t xemu_cycle_count(void);
extern "C" uint32_t *xemu_scratch_reg(int idx);
#define ARM_DWT_CYCCNT (xemu_cycle_count())
#define ARM_DEMCR (*xemu_scratch_reg(0))
#define ARM_DEMCR_TRCENA (1u << 24)
#define ARM_DWT_CTRL (*xemu_scratch_reg(1))
#define ARM_DWT_CTRL_CYCCNTENA (1u << 0)
#endif

// ---------------------------------------------------------------------------
// Random
// ---------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif
void randomSeed(uint32_t seed);
long xemu_random_max(long max);
long xemu_random_range(long lo, long hi);
#ifdef __cplusplus
}
// Arduino random() overloads (no-arg random() comes from the host libc)
template <typename T>
static inline long random(T max_) {
  return xemu_random_max((long)max_);
}
template <typename A, typename B>
static inline long random(A lo, B hi) {
  return xemu_random_range((long)lo, (long)hi);
}
#endif

// ---------------------------------------------------------------------------
// Memory helpers (Teensy 4.1)
// ---------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif
void *extmem_malloc(size_t size);
void *extmem_calloc(size_t nmemb, size_t size);
void extmem_free(void *ptr);
#ifdef __cplusplus
}
#endif
static inline void arm_dcache_flush(void *addr, uint32_t size) { (void)addr; (void)size; }
static inline void arm_dcache_delete(void *addr, uint32_t size) { (void)addr; (void)size; }
static inline void arm_dcache_flush_delete(void *addr, uint32_t size) { (void)addr; (void)size; }

// pgmspace-ish accessors
#define pgm_read_byte(x) (*(const uint8_t *)(x))
#define pgm_read_word(x) (*(const uint16_t *)(x))
#define pgm_read_dword(x) (*(const uint32_t *)(x))
#define memcpy_P memcpy
#define strcpy_P strcpy

// ---------------------------------------------------------------------------
// Print / Stream / Serial
// ---------------------------------------------------------------------------
#ifdef __cplusplus

class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t b) { fputc(b, stream_()); return 1; }
  virtual size_t write(const uint8_t *buffer, size_t size) {
    for (size_t i = 0; i < size; ++i) write(buffer[i]);
    return size;
  }
  size_t write(const char *s) { return write((const uint8_t *)s, strlen(s)); }

  size_t print(const char *s) { return write(s); }
  size_t print(char c) { return write((uint8_t)c); }
  size_t print(int v, int base = 10) { return print((long)v, base); }
  size_t print(unsigned int v, int base = 10) { return print((unsigned long)v, base); }
  size_t print(long v, int base = 10) {
    char buf[34];
    if (base == 16) snprintf(buf, sizeof buf, "%lx", v);
    else if (base == 2) { return print_binary((unsigned long)v); }
    else snprintf(buf, sizeof buf, "%ld", v);
    return write(buf);
  }
  size_t print(unsigned long v, int base = 10) {
    char buf[34];
    if (base == 16) snprintf(buf, sizeof buf, "%lx", v);
    else if (base == 2) { return print_binary(v); }
    else snprintf(buf, sizeof buf, "%lu", v);
    return write(buf);
  }
  size_t print(long long v, int base = 10) {
    char buf[40];
    snprintf(buf, sizeof buf, base == 16 ? "%llx" : "%lld", v);
    return write(buf);
  }
  size_t print(unsigned long long v, int base = 10) {
    char buf[40];
    snprintf(buf, sizeof buf, base == 16 ? "%llx" : "%llu", v);
    return write(buf);
  }
  size_t print(double v, int digits = 2) {
    char buf[48];
    snprintf(buf, sizeof buf, "%.*f", digits, v);
    return write(buf);
  }
  template <typename T> size_t println(T v) { size_t n = print(v); return n + write("\n"); }
  size_t println(long v, int base) { size_t n = print(v, base); return n + write("\n"); }
  size_t println(unsigned long v, int base) { size_t n = print(v, base); return n + write("\n"); }
  size_t println(int v, int base) { size_t n = print((long)v, base); return n + write("\n"); }
  size_t println(double v, int digits) { size_t n = print(v, digits); return n + write("\n"); }
  size_t println() { return write("\n"); }
  size_t printf(const char *fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    if (n > 0) write(buf);
    return n > 0 ? (size_t)n : 0;
  }
  virtual void flush() {}

private:
  size_t print_binary(unsigned long v) {
    char buf[34]; int i = 0;
    if (!v) buf[i++] = '0';
    else { int started = 0;
      for (int b = 31; b >= 0; --b) { int bit = (v >> b) & 1;
        if (bit) started = 1;
        if (started) buf[i++] = '0' + bit; } }
    buf[i] = 0; return write(buf);
  }
  FILE *stream_();
};

class Stream : public Print {
public:
  virtual int available() { return 0; }
  virtual int read() { return -1; }
  virtual int peek() { return -1; }
};

// USB serial: log to stderr (can be silenced via xemu)
class usb_serial_class : public Stream {
public:
  void begin(long) {}
  void end() {}
  operator bool() { return true; }
};

class HardwareSerial : public Stream {
public:
  void begin(long) {}
  void end() {}
  operator bool() { return true; }
};

extern usb_serial_class Serial;
extern HardwareSerial Serial1, Serial2, Serial3, Serial4, Serial5, Serial6, Serial7, Serial8;

// CrashReport stub — evaluates false so firmware skips the report path.
class CrashReportClass : public Print {
public:
  operator bool() { return false; }
  void clear() {}
};
extern CrashReportClass CrashReport;

// ---------------------------------------------------------------------------
// String (minimal)
// ---------------------------------------------------------------------------
#include <string>
class String : public std::string {
public:
  String() {}
  String(const char *s) : std::string(s ? s : "") {}
  String(const std::string &s) : std::string(s) {}
  String(char c) : std::string(1, c) {}
  String(int v) : std::string(std::to_string(v)) {}
  String(unsigned int v) : std::string(std::to_string(v)) {}
  String(long v) : std::string(std::to_string(v)) {}
  String(unsigned long v) : std::string(std::to_string(v)) {}
  String(float v) : std::string(std::to_string(v)) {}
  const char *c_str() const { return std::string::c_str(); }
  unsigned int length() const { return (unsigned int)std::string::length(); }
  char charAt(unsigned int i) const { return i < size() ? (*this)[i] : 0; }
  long toInt() const { return atol(c_str()); }
  float toFloat() const { return (float)atof(c_str()); }
  int indexOf(char c, unsigned int from = 0) const {
    size_t p = find(c, from);
    return p == npos ? -1 : (int)p;
  }
  int indexOf(const char *s, unsigned int from = 0) const {
    size_t p = find(s, from);
    return p == npos ? -1 : (int)p;
  }
  String substring(unsigned int beginIndex) const {
    return String(substr(beginIndex));
  }
  String substring(unsigned int beginIndex, unsigned int endIndex) const {
    return String(substr(beginIndex, endIndex - beginIndex));
  }
  void trim() {
    size_t a = find_first_not_of(" \t\r\n");
    size_t b = find_last_not_of(" \t\r\n");
    if (a == npos) { clear(); return; }
    *this = substr(a, b - a + 1);
  }
  bool startsWith(const String &s) const { return rfind(s, 0) == 0; }
  bool endsWith(const String &s) const {
    return size() >= s.size() && compare(size() - s.size(), s.size(), s) == 0;
  }
  bool equals(const String &s) const { return *this == s; }
  void toUpperCase() { for (auto &c : *this) c = (char)toupper(c); }
  void toLowerCase() { for (auto &c : *this) c = (char)tolower(c); }
};

// ---------------------------------------------------------------------------
// IntervalTimer — registers with the emulator's virtual clock.
// ---------------------------------------------------------------------------
class IntervalTimer {
public:
  IntervalTimer() : idx_(-1) {}
  ~IntervalTimer() { end(); }
  bool begin(void (*fn)(), unsigned int usec);
  bool begin(void (*fn)(), float usec) { return begin(fn, (unsigned int)usec); }
  void end();
  void priority(uint8_t) {}
private:
  int idx_;
};

// ---------------------------------------------------------------------------
// elapsedMillis / elapsedMicros
// ---------------------------------------------------------------------------
class elapsedMillis {
  uint32_t ms_;
public:
  elapsedMillis() { ms_ = millis(); }
  elapsedMillis(uint32_t v) { ms_ = millis() - v; }
  operator uint32_t() const { return millis() - ms_; }
  elapsedMillis &operator=(uint32_t v) { ms_ = millis() - v; return *this; }
};
class elapsedMicros {
  uint32_t us_;
public:
  elapsedMicros() { us_ = micros(); }
  elapsedMicros(uint32_t v) { us_ = micros() - v; }
  operator uint32_t() const { return micros() - us_; }
  elapsedMicros &operator=(uint32_t v) { us_ = micros() - v; return *this; }
};

// usbMIDI stub (USB device MIDI) — inert; VCV frontend doesn't route MIDI yet.
class usb_midi_class {
public:
  void begin() {}
  bool read(uint8_t = 0) { return false; }
  uint8_t getType() { return 0; }
  uint8_t getChannel() { return 1; }
  uint8_t getData1() { return 0; }
  uint8_t getData2() { return 0; }
  uint8_t getCable() { return 0; }
  uint8_t *getSysExArray() { return sysex_dummy_; }
  unsigned int getSysExArrayLength() { return 0; }
  void sendNoteOn(uint8_t, uint8_t, uint8_t, uint8_t = 0) {}
  void sendNoteOff(uint8_t, uint8_t, uint8_t, uint8_t = 0) {}
  void sendControlChange(uint8_t, uint8_t, uint8_t, uint8_t = 0) {}
  void sendProgramChange(uint8_t, uint8_t, uint8_t = 0) {}
  void sendAfterTouch(uint8_t, uint8_t, uint8_t = 0) {}
  void sendPitchBend(int, uint8_t, uint8_t = 0) {}
  void sendSysEx(uint32_t, const uint8_t *, bool = false, uint8_t = 0) {}
  void sendRealTime(uint8_t, uint8_t = 0) {}
  void sendClock(uint8_t = 0) {}
  void sendStart(uint8_t = 0) {}
  void sendStop(uint8_t = 0) {}
  void sendContinue(uint8_t = 0) {}
  void send(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t = 0) {}
  void send_now() {}
  enum {
    InvalidType = 0x00,
    NoteOff = 0x80,
    NoteOn = 0x90,
    AfterTouchPoly = 0xA0,
    ControlChange = 0xB0,
    ProgramChange = 0xC0,
    AfterTouchChannel = 0xD0,
    PitchBend = 0xE0,
    SystemExclusive = 0xF0,
    TimeCodeQuarterFrame = 0xF1,
    SongPosition = 0xF2,
    SongSelect = 0xF3,
    TuneRequest = 0xF6,
    Clock = 0xF8,
    Start = 0xFA,
    Continue = 0xFB,
    Stop = 0xFC,
    ActiveSensing = 0xFE,
    SystemReset = 0xFF
  };
private:
  uint8_t sysex_dummy_[8] = {0};
};
extern usb_midi_class usbMIDI;

#endif  // __cplusplus

// setup/loop provided by firmware Main.cpp
#ifdef __cplusplus
extern "C" {
#endif
void setup(void);
void loop(void);
#ifdef __cplusplus
}
#endif
