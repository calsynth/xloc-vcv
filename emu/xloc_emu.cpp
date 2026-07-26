#include "xloc_emu.h"

#include <imxrt.h>  // Lpspi4TdrProxy

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <thread>

extern "C" void setup(void);
extern "C" void loop(void);

namespace xemu {

State &state() {
  static State s;
  return s;
}

Clock &clock() {
  static Clock c;
  return c;
}

static std::string g_storage_dir;
void set_storage_dir(const std::string &dir) { g_storage_dir = dir; }
const std::string &storage_dir() { return g_storage_dir; }

// ---------------------------------------------------------------------------
// Timers
// ---------------------------------------------------------------------------
int Clock::add_timer(std::function<void()> fn, uint64_t period_us) {
  return add_timer_ns(std::move(fn), period_us * 1000ull);
}

int Clock::add_timer_ns(std::function<void()> fn, uint64_t period_ns) {
  std::lock_guard<std::recursive_mutex> lk(isr_mutex);
  for (int i = 0; i < kMaxTimers; ++i) {
    if (!timers[i].active) {
      timers[i].fn = std::move(fn);
      timers[i].period_ns = period_ns;
      timers[i].next_due_ns = now_ns.load() + timers[i].period_ns;
      timers[i].active = true;
      return i;
    }
  }
  return -1;
}

void Clock::remove_timer(int idx) {
  if (idx >= 0 && idx < kMaxTimers) timers[idx].active = false;
}

void Clock::step(uint64_t dt_ns) {
  uint64_t target = now_ns.load() + dt_ns;
  // Fire timers in chronological order until we reach the target time.
  for (;;) {
    uint64_t earliest = UINT64_MAX;
    int which = -1;
    for (int i = 0; i < kMaxTimers; ++i) {
      if (timers[i].active && timers[i].next_due_ns < earliest) {
        earliest = timers[i].next_due_ns;
        which = i;
      }
    }
    if (which < 0 || earliest > target) break;
    now_ns.store(earliest);
    timers[which].next_due_ns += timers[which].period_ns;
    {
      std::lock_guard<std::recursive_mutex> lk(isr_mutex);
      encoder_service();
      if (timers[which].fn) timers[which].fn();
    }
  }
  now_ns.store(target);
}

// ---------------------------------------------------------------------------
// Boot / loop thread
// ---------------------------------------------------------------------------
static std::thread g_loop_thread;
static std::atomic<bool> g_booted{false};
static std::atomic<bool> g_loop_run{false};

// Firmware code (setup() and loop()) runs on its own thread and never steps
// virtual time itself — it spin-waits in delay() while the frontend (or
// boot() during startup) advances the clock and fires ISRs. This matches
// hardware, where timer ISRs keep running while setup() busy-waits.
static std::atomic<bool> g_self_clocking{false};
bool self_clocking() { return g_self_clocking.load(); }
void set_self_clocking(bool v) { g_self_clocking.store(v); }

bool booted() { return g_booted.load(); }

static std::atomic<bool> g_boot_started{false};

// --- Firmware-thread parking (see header) -------------------------------
static std::atomic<bool> g_park_req{false};
static std::atomic<bool> g_parked{false};

void request_park() { g_park_req.store(true); }
bool firmware_parked() { return g_parked.load(); }

void maybe_park_current_thread() {
  if (!g_park_req.load()) return;
  g_parked.store(true);
  // Park forever; process exit reaps the detached thread.
  for (;;) std::this_thread::sleep_for(std::chrono::seconds(1));
}

static void park_at_exit() {
  request_park();
  // Keep virtual time moving so the loop thread reaches a park point even if
  // it is waiting on virtual time (delay()) or a display frame drain.
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!firmware_parked() && std::chrono::steady_clock::now() < deadline) {
    clock().step(1000000ull);
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
}

void boot_async() {
  if (g_boot_started.exchange(true)) return;
  g_loop_run = true;
  // Registered after firmware globals are constructed, so this runs before
  // their destructors during exit teardown.
  std::atexit(park_at_exit);
  g_loop_thread = std::thread([] {
    setup();
    g_booted = true;
    while (g_loop_run.load()) {
      loop();  // firmware loop() contains its own while(true); returns never.
    }
  });
  g_loop_thread.detach();  // never joinable — firmware loop() never returns
}

void boot() {
  boot_async();
  // Drive virtual time until setup() completes (splash screen etc. depend on
  // millis() advancing and the display ISR draining the framebuffer).
  while (!g_booted.load()) {
    clock().step(1000000ull);  // 1 ms
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }
}

void shutdown() {
  // The firmware loop never returns; we simply detach and let the process
  // die with the thread parked. For the VCV module we keep the emu alive for
  // the whole process lifetime instead (module destruction just stops
  // stepping the clock).
  g_loop_run = false;
}

// ---------------------------------------------------------------------------
// Panel input injection
// ---------------------------------------------------------------------------
void set_cv_volts(int ch, float v) {
  if (ch >= 0 && ch < kNumCVIn) state().cv_in_volts[ch].store(v);
}

void set_trigger(int idx, bool gate_high) {
  if (idx < 0 || idx >= kNumTrig) return;
  // ORN8/XLOC2 trigger comparators are active-HIGH (see read_immediate in
  // OC_digital_inputs.h: activated = HIGH when ADC33131D_Uses_FlexIO).
  state().pin_state[kTrigPins[idx]].store(gate_high ? 1 : 0);
}

void set_button(int pin, bool pressed) {
  if (pin < 0 || pin >= kNumPins) return;
  state().pin_state[pin].store(pressed ? 0 : 1);  // active low
}

void turn_encoder(int which, int detents) {
  if (which < 0 || which > 1) return;
  state().enc_accum[which].fetch_add(detents);
}

void press_encoder(int which, bool pressed) {
  set_button(which == 0 ? BTN_ENC_L : BTN_ENC_R, pressed);
}

// Encoder pins: L = 30/31, R = 36/37 (A/B quadrature).
static const int kEncPins[2][2] = {{30, 31}, {36, 37}};

// Advance quadrature state at most one step per (UI timer) service so the
// firmware's edge decoding never misses steps. 4 quadrature steps = 1 detent
// on the hardware encoders? Phazerville counts every step; the UI applies
// its own acceleration. One step per detent turned out closest to hardware
// feel; adjust here if needed.
static const uint8_t kQuadState[4] = {0b00, 0b01, 0b11, 0b10};

void encoder_service() {
  State &s = state();
  // Advance at most one quadrature step per UI-poll period (1 kHz). The
  // firmware samples pins at 1 kHz; stepping faster aliases the waveform.
  static uint64_t last_step_ns = 0;
  uint64_t now = clock().now_ns.load();
  if (now - last_step_ns < 900000ull) return;
  last_step_ns = now;
  for (int e = 0; e < 2; ++e) {
    int32_t pending = s.enc_accum[e].load() - s.enc_consumed[e];
    if (pending == 0) continue;
    int dir = pending > 0 ? 1 : -1;
    s.enc_phase[e] = (uint8_t)((s.enc_phase[e] + dir) & 3);
    uint8_t q = kQuadState[s.enc_phase[e]];
    s.pin_state[kEncPins[e][0]].store((q >> 1) & 1);
    s.pin_state[kEncPins[e][1]].store(q & 1);
    // One full detent = 4 quadrature steps.
    static int substep[2] = {0, 0};  // guarded by isr_mutex
    substep[e] += dir;
    if (substep[e] >= 4 || substep[e] <= -4) {
      substep[e] = 0;
      s.enc_consumed[e] += dir;
    }
  }
}

// ---------------------------------------------------------------------------
// Output readback
// ---------------------------------------------------------------------------
// Panel OUT jack -> DAC8568 wire channel, per the CalSynthXL remap in
// OC_gpio.cpp (DAC_CHANNEL_A..H = 4,5,6,7,0,1,2,3).
static const int kPanelToDacWire[kNumCVOut] = {4, 5, 6, 7, 0, 1, 2, 3};

float get_cv_out_volts(int ch) {
  if (ch < 0 || ch >= kNumCVOut) return 0.f;
  return state().dac_volts[kPanelToDacWire[ch]].load();
}

uint32_t get_framebuffer(uint8_t *dst) {
  State &s = state();
  std::lock_guard<std::recursive_mutex> lk(clock().isr_mutex);
  std::memcpy(dst, s.framebuffer, kFBSize);
  return s.fb_generation.load();
}

// ---------------------------------------------------------------------------
// DAC8568 SPI decode (XLOC2 ideal calibration inversion)
// ---------------------------------------------------------------------------
// Ideal XLOC2 (DAC_20Vpp) calibration table: 11 octave points, 2 V per step,
// index 5 = 0 V. From kDAC20VppDefaults in OC_calibration.cpp.
static const uint16_t kDacOctaves[11] = {936,   7303,  13670, 20037, 26404, 32771,
                                         39138, 45505, 51872, 58239, 64610};

static float dac_code_to_volts(uint16_t code) {
  // Piecewise-linear inversion of the octave table; 2 V per segment.
  if (code <= kDacOctaves[0]) {
    // extrapolate below
    float seg = (float)(kDacOctaves[1] - kDacOctaves[0]);
    return -10.f + 2.f * ((float)code - kDacOctaves[0]) / seg;
  }
  for (int i = 0; i < 10; ++i) {
    if (code <= kDacOctaves[i + 1]) {
      float seg = (float)(kDacOctaves[i + 1] - kDacOctaves[i]);
      float frac = ((float)code - kDacOctaves[i]) / seg;
      return -10.f + 2.f * ((float)i + frac);
    }
  }
  float seg = (float)(kDacOctaves[10] - kDacOctaves[9]);
  return 10.f + 2.f * ((float)code - kDacOctaves[10]) / seg;
}

void dac8568_spi_word(uint32_t w) {
  // dac8568_set_channel: 0x03000000 | ch<<20 | value<<4
  if ((w >> 24) == 0x03) {
    uint32_t ch = (w >> 20) & 0x07;
    uint16_t raw = (uint16_t)((w >> 4) & 0xFFFF);
    // Firmware inverts for non-inverted hardware (MAX_VALUE - value), and the
    // analog stage inverts again. Un-do the wire inversion so raw is the
    // logical DAC code as held in DAC::values_.
    uint16_t code = (uint16_t)(65535u - raw);
    State &s = state();
    s.dac_raw[ch].store(code);
    s.dac_volts[ch].store(dac_code_to_volts(code));
  }
  // 0x08000001 = Vref enable — ignore.
}

Lpspi4TdrProxy lpspi4_tdr_proxy;
void Lpspi4TdrProxy::operator=(uint32_t w) { dac8568_spi_word(w); }

// ---------------------------------------------------------------------------
// ADC — ideal XLOC2 conversion (see emu_OC_ADC.cpp for the scan plumbing)
// raw12 = 2730 - V * 409.6, presented as 16-bit scan values (raw12 << 4).
// ---------------------------------------------------------------------------
int32_t adc_read_raw(int adc_channel) {
  if (adc_channel < 0 || adc_channel >= kNumCVIn) return 2730 << 4;
  float v = state().cv_in_volts[adc_channel].load();
  float raw12 = 2730.f - v * 409.6f;
  if (raw12 < 0.f) raw12 = 0.f;
  if (raw12 > 4095.f) raw12 = 4095.f;
  return (int32_t)(raw12 * 16.f + 0.5f);
}

// ---------------------------------------------------------------------------
// OLED page capture
// ---------------------------------------------------------------------------
void oled_page(int page, const uint8_t *data) {
  if (page < 0 || page >= 8) return;
  std::memcpy(state().framebuffer + page * 128, data, 128);
}

void oled_flush() { state().fb_generation.fetch_add(1); }

// ---------------------------------------------------------------------------
// Audio engine clocking + I2S rings
// ---------------------------------------------------------------------------
extern "C" void xemu_audio_update_all();  // defined via AudioStream (below)

namespace {
// Simple ring of interleaved stereo int16 frames @44.1k. All access happens
// under the ISR mutex (engine side runs inside timers; frontend side locks).
struct AudioRing {
  static constexpr int kCap = 16384;  // frames (~0.37 s)
  int16_t l[kCap];
  int16_t r[kCap];
  int head = 0, tail = 0, count = 0;

  void push(int16_t sl, int16_t sr) {
    if (count == kCap) {  // overrun: drop oldest
      tail = (tail + 1) % kCap;
      --count;
    }
    l[head] = sl;
    r[head] = sr;
    head = (head + 1) % kCap;
    ++count;
  }
  bool pop(int16_t &sl, int16_t &sr) {
    if (!count) return false;
    sl = l[tail];
    sr = r[tail];
    tail = (tail + 1) % kCap;
    --count;
    return true;
  }
};

AudioRing g_out_ring;  // engine -> frontend
AudioRing g_in_ring;   // frontend -> engine
std::atomic<bool> g_audio_running{false};
}  // namespace

bool audio_engine_running() { return g_audio_running.load(); }

void audio_engine_start() {
  if (g_audio_running.exchange(true)) return;
  // 128 samples @44100 Hz = 2902494.33 ns; sub-ppm rounding is absorbed by
  // the rings (the frontend clock is authoritative).
  clock().add_timer_ns([] { xemu_audio_update_all(); }, 2902494ull);
}

void audio_out_push(const int16_t *left, const int16_t *right, int n) {
  std::lock_guard<std::recursive_mutex> lk(clock().isr_mutex);
  for (int i = 0; i < n; ++i)
    g_out_ring.push(left ? left[i] : 0, right ? right[i] : 0);
}

void audio_in_pull(int16_t *left, int16_t *right, int n) {
  std::lock_guard<std::recursive_mutex> lk(clock().isr_mutex);
  for (int i = 0; i < n; ++i) {
    int16_t sl = 0, sr = 0;
    g_in_ring.pop(sl, sr);
    if (left) left[i] = sl;
    if (right) right[i] = sr;
  }
}

void audio_out_read(float *lr, int frames) {
  std::lock_guard<std::recursive_mutex> lk(clock().isr_mutex);
  for (int i = 0; i < frames; ++i) {
    int16_t sl = 0, sr = 0;
    g_out_ring.pop(sl, sr);
    lr[i * 2] = (float)sl / 32768.f;
    lr[i * 2 + 1] = (float)sr / 32768.f;
  }
}

void audio_in_write(const float *lr, int frames) {
  std::lock_guard<std::recursive_mutex> lk(clock().isr_mutex);
  for (int i = 0; i < frames; ++i) {
    float fl = lr[i * 2], fr = lr[i * 2 + 1];
    if (fl > 1.f) fl = 1.f; else if (fl < -1.f) fl = -1.f;
    if (fr > 1.f) fr = 1.f; else if (fr < -1.f) fr = -1.f;
    g_in_ring.push((int16_t)(fl * 32767.f), (int16_t)(fr * 32767.f));
  }
}

int audio_out_available() {
  std::lock_guard<std::recursive_mutex> lk(clock().isr_mutex);
  return g_out_ring.count;
}

}  // namespace xemu
