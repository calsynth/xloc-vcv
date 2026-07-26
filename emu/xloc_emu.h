// XLOC2 emulation core — shared state between the shimmed firmware and any
// frontend (headless test harness or VCV Rack module).
//
// The firmware runs unmodified; all hardware access funnels through the shim
// layer into this state.
#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>

namespace xemu {

// ---------------------------------------------------------------------------
// Constants (XLOC2 / CalSynthXL hardware facts)
// ---------------------------------------------------------------------------
static constexpr int kNumCVIn = 8;
static constexpr int kNumCVOut = 8;
static constexpr int kNumTrig = 4;
static constexpr int kNumPins = 64;   // virtual GPIO pin space
static constexpr int kFBWidth = 128;
static constexpr int kFBHeight = 64;
static constexpr int kFBSize = kFBWidth * kFBHeight / 8;  // 1 KB, page mode

// ID voltage on pin A17: 0.25..0.35 V => CalSynthXL (XLOC2)
static constexpr float kIdVoltage = 0.30f;

// Core ISR rate: 60 us (16.666 kHz); UI timer: 1000 us (1 kHz)
// (These come from the firmware's IntervalTimer.begin() calls; we don't
// hardcode them here — the shim registers whatever the firmware asks for.)

// Physical trigger input pins on ORN8/XLOC2 (TR1..TR4)
static constexpr int kTrigPins[kNumTrig] = {0, 1, 23, 22};

// ---------------------------------------------------------------------------
// Virtual hardware state
// ---------------------------------------------------------------------------
struct State {
  // --- CV inputs, in volts, written by the frontend ---
  std::atomic<float> cv_in_volts[kNumCVIn];

  // --- Trigger inputs: frontend sets logical gate high/low; the shimmed
  //     digitalReadFast() reads pin state (active low on hardware) ---
  // pin_state[pin] == 1 means electrically high.
  std::atomic<uint8_t> pin_state[kNumPins];

  // --- CV outputs: decoded from DAC8568 SPI writes, raw 16-bit codes ---
  std::atomic<uint16_t> dac_raw[kNumCVOut];
  // Converted to volts by the frontend using the firmware's own calibration
  // (ideal XLOC2: ±10 V over 0..65535, inverted handled in decode).
  std::atomic<float> dac_volts[kNumCVOut];

  // --- OLED framebuffer: SH1106 page writes land here ---
  uint8_t framebuffer[kFBSize];
  std::atomic<uint32_t> fb_generation{0};  // bumped on every Flush

  // --- Panel controls, written by frontend, read by shimmed UI poll ---
  // Buttons (electrically: pressed = LOW). Indexed by physical pin number.
  // Encoders: accumulated detent deltas; the shim converts to quadrature
  // pin states as the UI timer samples them.
  std::atomic<int32_t> enc_accum[2];   // L, R accumulated ticks (frontend +=)
  int32_t enc_consumed[2] = {0, 0};    // shim-side consumed count
  uint8_t enc_phase[2] = {0, 0};       // current quadrature phase 0..3

  State() {
    for (auto &v : cv_in_volts) v.store(0.f);
    for (auto &p : pin_state) p.store(1);  // pulled up / idle high
    for (int t : kTrigPins) pin_state[t].store(0);  // triggers active-high, idle low
    for (auto &d : dac_raw) d.store(0x8000);
    for (auto &d : dac_volts) d.store(0.f);
    std::memset(framebuffer, 0, sizeof framebuffer);
    enc_accum[0] = enc_accum[1] = 0;
  }
};

State &state();

// ---------------------------------------------------------------------------
// Virtual time + timers (shim IntervalTimer registers here)
// ---------------------------------------------------------------------------
struct Timer {
  std::function<void()> fn;
  uint64_t period_ns = 0;
  uint64_t next_due_ns = 0;
  bool active = false;
};

struct Clock {
  std::atomic<uint64_t> now_ns{0};
  static constexpr int kMaxTimers = 8;
  Timer timers[kMaxTimers];

  // "interrupts disabled" emulation: a recursive mutex held while any ISR
  // runs; noInterrupts() locks it from the loop() thread.
  std::recursive_mutex isr_mutex;

  int add_timer(std::function<void()> fn, uint64_t period_us);
  int add_timer_ns(std::function<void()> fn, uint64_t period_ns);
  void remove_timer(int idx);

  // Advance virtual time by dt_ns, firing due timers in order.
  void step(uint64_t dt_ns);
};

Clock &clock();

// ---------------------------------------------------------------------------
// Lifecycle — used by frontends
// ---------------------------------------------------------------------------
// Configure backing paths (EEPROM file, LittleFS dir, SD dir). Empty string =
// in-memory only. Call before boot().
void set_storage_dir(const std::string &dir);
const std::string &storage_dir();

// Non-blocking boot: spawns a thread that runs setup() and then loop().
// The caller must keep advancing virtual time via clock().step() (setup
// busy-waits on millis(), the splash screen, and possibly a first-run
// confirm dialog that needs a right-encoder press). booted() flips true
// once setup() has completed.
void boot_async();

// Blocking boot for headless use: boot_async() + drive the clock until
// setup() completes. Returns after setup() completes.
void boot();

// Stop the loop() thread (cooperative: loop keeps running; we just park it).
void shutdown();
bool booted();

// Firmware-thread parking, used at process exit. Static destructors tear
// down AudioStream-derived globals while the detached loop() thread is still
// dispatching virtual calls on them ("pure virtual method called" aborts).
// boot_async() registers an atexit handler that requests a park and keeps
// stepping virtual time until the loop thread confirms it is parked (it
// checks in the USB Task / delay shims, hit every loop iteration).
void request_park();                  // ask the firmware thread to park
bool firmware_parked();               // has it parked?
void maybe_park_current_thread();     // called from shims on the fw thread

// --- Panel input injection (thread-safe) ---
void set_cv_volts(int ch, float v);
void set_trigger(int idx, bool gate_high);       // idx 0..3
void set_button(int pin, bool pressed);           // physical pin number
void turn_encoder(int which, int detents);        // 0 = L, 1 = R
void press_encoder(int which, bool pressed);

// Buttons by name (XLOC2: A,X,B,Y,Z + encoder pushes L,R)
// A=29, X=20, B=28, Y=14, Z=15, encL=24, encR=25
enum Button { BTN_A = 29, BTN_X = 20, BTN_B = 28, BTN_Y = 14, BTN_Z = 15,
              BTN_ENC_L = 24, BTN_ENC_R = 25 };

// --- Output readback ---
float get_cv_out_volts(int ch);
// Copy of the framebuffer; returns generation counter.
uint32_t get_framebuffer(uint8_t *dst /* kFBSize bytes */);

// --- internal hooks used by the shim (documented in xloc_emu.cpp) ---
void dac8568_spi_word(uint32_t word);           // called on LPSPI4_TDR write
int32_t adc_read_raw(int adc_channel);          // 16-bit-style raw for scan
void oled_page(int page, const uint8_t *data);  // SH1106 page landed
void oled_flush();
void encoder_service();                          // advance quadrature emulation

// ---------------------------------------------------------------------------
// Audio (phase 3): the AudioStream engine runs at 44.1 kHz on virtual time.
// The frontend exchanges samples with the emulated I2S2 codec through two
// ring buffers of interleaved stereo float frames at 44100 Hz.
// ---------------------------------------------------------------------------
static constexpr double kAudioSampleRate = 44100.0;

// Begin firing AudioStream::update_all() every 128 samples of virtual time.
// Idempotent; called by the I2S shim's update_setup().
void audio_engine_start();
bool audio_engine_running();

// Engine side (called from AudioInputI2S2/AudioOutputI2S2 update()):
void audio_out_push(const int16_t *left, const int16_t *right, int n);  // may pass nullptr for silence
void audio_in_pull(int16_t *left, int16_t *right, int n);               // zero-fills on underrun

// Frontend side (Rack module / tests), frames at 44100 Hz:
void audio_out_read(float *lr_interleaved, int frames);  // zero-fills on underrun
void audio_in_write(const float *lr_interleaved, int frames);
int audio_out_available();

}  // namespace xemu
