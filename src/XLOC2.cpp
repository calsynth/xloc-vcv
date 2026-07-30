// XLOC2 module for VCV Rack — runs the real Phazerville firmware through the
// xemu shim layer. Only ONE instance can exist per Rack process (the firmware
// is a global singleton, just like the hardware).
#include "plugin.hpp"

#include "../emu/xloc_emu.h"

#include <atomic>

using namespace rack;

struct XLOC2Module : Module {
  enum ParamIds { NUM_PARAMS };
  enum InputIds {
    CV1_INPUT, CV2_INPUT, CV3_INPUT, CV4_INPUT,
    CV5_INPUT, CV6_INPUT, CV7_INPUT, CV8_INPUT,
    TR1_INPUT, TR2_INPUT, TR3_INPUT, TR4_INPUT,
    AUDIO_L_INPUT, AUDIO_R_INPUT,
    NUM_INPUTS
  };
  enum OutputIds {
    OUTA_OUTPUT, OUTB_OUTPUT, OUTC_OUTPUT, OUTD_OUTPUT,
    OUTE_OUTPUT, OUTF_OUTPUT, OUTG_OUTPUT, OUTH_OUTPUT,
    AUDIO_L_OUTPUT, AUDIO_R_OUTPUT,
    NUM_OUTPUTS
  };
  enum LightIds { NUM_LIGHTS };

  static std::atomic<XLOC2Module *> owner;
  bool isOwner = false;

  // Encoder click scheduling (virtual-time press/release), fed by widgets.
  std::atomic<uint64_t> encClickUntil[2] = {{0}, {0}};
  std::atomic<uint64_t> encClickStart[2] = {{0}, {0}};
  bool encClickActive[2] = {false, false};
  // Latch auto-release bookkeeping (virtual ns):
  // last encoder use (turn/push) — latches expire after idle timeout
  std::atomic<uint64_t> lastEncActivityNs{0};
  // set past the end of a timed encoder click — latches armed before this
  // release once it passes ("hold button + click encoder" completes combo)
  std::atomic<uint64_t> latchReleaseAtNs{0};

  void noteEncoderActivity() {
    lastEncActivityNs.store(xemu::clock().now_ns.load());
  }

  bool trigHigh[4] = {false, false, false, false};

  // ---- AUDIO jacks <-> 44.1 kHz engine rings (linear-interp SRC) ----
  // input side: Rack rate -> 44.1k
  float inPrevL = 0.f, inPrevR = 0.f;
  double inPhase = 0.0;   // position within current Rack sample, in 44.1k steps
  // output side: 44.1k -> Rack rate
  float outA[2] = {0.f, 0.f}, outB[2] = {0.f, 0.f};  // consecutive 44.1k frames
  double outPhase = 1.0;  // >=1 means need next frame
  bool outPrimed = false;
  int depthCheck = 0;

  XLOC2Module() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    for (int i = 0; i < 8; ++i)
      configInput(CV1_INPUT + i, string::f("CV %d", i + 1));
    for (int i = 0; i < 4; ++i)
      configInput(TR1_INPUT + i, string::f("Trigger %d", i + 1));
    static const char *outNames = "ABCDEFGH";
    for (int i = 0; i < 8; ++i)
      configOutput(OUTA_OUTPUT + i, string::f("CV %c", outNames[i]));
    configInput(AUDIO_L_INPUT, "Audio L");
    configInput(AUDIO_R_INPUT, "Audio R");
    configOutput(AUDIO_L_OUTPUT, "Audio L");
    configOutput(AUDIO_R_OUTPUT, "Audio R");

    XLOC2Module *expected = nullptr;
    isOwner = owner.compare_exchange_strong(expected, this);
    if (isOwner) {
      std::string dir = asset::user("Calsynth-XLOC2");
      system::createDirectories(dir);
      xemu::set_storage_dir(dir);
      xemu::boot_async();
    }
  }

  ~XLOC2Module() override {
    XLOC2Module *self = this;
    owner.compare_exchange_strong(self, nullptr);
    // The firmware keeps running (it cannot be torn down); a new instance
    // simply reattaches to it.
  }

  void clickEncoder(int which, float virtualMs) {
    uint64_t now = xemu::clock().now_ns.load();
    uint64_t until = now + (uint64_t)(virtualMs * 1e6);
    encClickStart[which].store(now);
    encClickUntil[which].store(until);
    lastEncActivityNs.store(until);
    // A SHORT click confirms a button+encoder combo — release any latched
    // button shortly after. Long presses are their own gesture and should
    // never cut a latch out from under active use.
    if (virtualMs < 500.f) latchReleaseAtNs.store(until + 60000000ull);
  }

  void dualClickEncoders() {
    // Press both encoders, STAGGERED ~30 virtual ms: the firmware's combo
    // detector (AudioAppletSubapp::HandleEncoderButtonEvent) arms on a
    // single-button DOWN and fires when the second DOWN sees both held.
    // Perfectly simultaneous presses never arm it.
    INFO("XLOC2 dualClickEncoders");
    uint64_t now = xemu::clock().now_ns.load();
    encClickStart[0].store(now);
    encClickUntil[0].store(now + 170000000ull);           // L: 0..170 ms
    encClickStart[1].store(now + 30000000ull);            // R: 30..170 ms
    encClickUntil[1].store(now + 170000000ull);
    lastEncActivityNs.store(now + 170000000ull);
    latchReleaseAtNs.store(now + 230000000ull);
  }

  void process(const ProcessArgs &args) override {
    if (!isOwner) {
      for (int i = 0; i < 8; ++i) outputs[OUTA_OUTPUT + i].setVoltage(0.f);
      return;
    }

    // Feed inputs
    for (int i = 0; i < 8; ++i)
      xemu::set_cv_volts(i, inputs[CV1_INPUT + i].getVoltage());
    for (int i = 0; i < 4; ++i) {
      float v = inputs[TR1_INPUT + i].getVoltage();
      // Schmitt trigger: high above 1.0 V, low below 0.1 V
      if (!trigHigh[i] && v >= 1.f) trigHigh[i] = true;
      else if (trigHigh[i] && v <= 0.1f) trigHigh[i] = false;
      xemu::set_trigger(i, trigHigh[i]);
    }

    // Timed encoder clicks (short press injected by the widget)
    uint64_t now = xemu::clock().now_ns.load();
    for (int e = 0; e < 2; ++e) {
      uint64_t until = encClickUntil[e].load();
      bool want = now >= encClickStart[e].load() && now < until;
      if (want != encClickActive[e]) {
        encClickActive[e] = want;
        xemu::press_encoder(e, want);
      }
    }

    // Audio input jacks -> engine (resample Rack rate -> 44.1k, linear)
    {
      float curL = inputs[AUDIO_L_INPUT].getVoltage() / 5.f;   // 5 Vpp reference
      float curR = inputs[AUDIO_R_INPUT].getVoltage() / 5.f;
      const double ratio = xemu::kAudioSampleRate * args.sampleTime;  // 44.1k frames per Rack sample
      inPhase += ratio;
      while (inPhase >= 1.0) {
        inPhase -= 1.0;
        // fraction through this Rack sample for the emitted 44.1k frame
        double frac = 1.0 - inPhase / ratio;
        if (frac < 0.0) frac = 0.0;
        if (frac > 1.0) frac = 1.0;
        float lr[2] = {(float)(inPrevL + (curL - inPrevL) * frac),
                       (float)(inPrevR + (curR - inPrevR) * frac)};
        xemu::audio_in_write(lr, 1);
      }
      inPrevL = curL;
      inPrevR = curR;
    }

    // Advance the firmware by one sample of virtual time; this fires the
    // 16.666 kHz core ISR, 1 kHz UI ISR and the 344.5 Hz audio update.
    xemu::clock().step((uint64_t)(args.sampleTime * 1e9));

    // Collect CV outputs
    for (int i = 0; i < 8; ++i)
      outputs[OUTA_OUTPUT + i].setVoltage(xemu::get_cv_out_volts(i));

    // Engine -> audio output jacks (resample 44.1k -> Rack rate, linear)
    {
      // Latency management: prime to a small level; drain if the ring grows.
      if (++depthCheck >= 256) {
        depthCheck = 0;
        int avail = xemu::audio_out_available();
        if (avail > 2048) {
          float junk[2];
          while (avail-- > 512) xemu::audio_out_read(junk, 1);
        }
      }
      if (!outPrimed) {
        if (xemu::audio_out_available() >= 256) {
          xemu::audio_out_read(outA, 1);
          xemu::audio_out_read(outB, 1);
          outPhase = 0.0;
          outPrimed = true;
        }
      }
      float ol = 0.f, orr = 0.f;
      if (outPrimed) {
        const double ratio = xemu::kAudioSampleRate * args.sampleTime;
        outPhase += ratio;
        while (outPhase >= 1.0) {
          outPhase -= 1.0;
          outA[0] = outB[0];
          outA[1] = outB[1];
          if (xemu::audio_out_available() > 0) {
            xemu::audio_out_read(outB, 1);
          } else {
            outPrimed = false;  // underrun: re-prime
            outPhase = 1.0;
            break;
          }
        }
        float t = (float)outPhase;
        ol = outA[0] + (outB[0] - outA[0]) * t;
        orr = outA[1] + (outB[1] - outA[1]) * t;
      }
      outputs[AUDIO_L_OUTPUT].setVoltage(ol * 5.f);
      outputs[AUDIO_R_OUTPUT].setVoltage(orr * 5.f);
    }
  }
};

std::atomic<XLOC2Module *> XLOC2Module::owner{nullptr};

// ---------------------------------------------------------------------------
// OLED widget — renders the live SH1106 framebuffer
// ---------------------------------------------------------------------------
struct OledWidget : TransparentWidget {
  XLOC2Module *module = nullptr;

  void drawLayer(const DrawArgs &args, int layer) override {
    if (layer != 1) return;  // self-illuminating layer (glows in dark rooms)

    uint8_t fb[xemu::kFBSize];
    bool live = module && module->isOwner;
    if (live) xemu::get_framebuffer(fb);

    // Panel background
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2.f);
    nvgFillColor(args.vg, nvgRGB(0x0A, 0x0E, 0x14));
    nvgFill(args.vg);

    // Lit pixels as vector rects: crisp at every zoom (a scaled NEAREST
    // texture gets unevenly-sized pixels at fractional zooms — issue #1),
    // with a hint of pixel grid like the real OLED.
    if (live) {
      const float cw = box.size.x / (float)xemu::kFBWidth;
      const float ch = box.size.y / (float)xemu::kFBHeight;
      const float pw = cw * 0.92f, ph = ch * 0.92f;
      nvgBeginPath(args.vg);
      for (int y = 0; y < xemu::kFBHeight; ++y) {
        int page = y >> 3, bit = y & 7;
        for (int x = 0; x < xemu::kFBWidth; ++x) {
          if ((fb[page * 128 + x] >> bit) & 1)
            nvgRect(args.vg, x * cw, y * ch, pw, ph);
        }
      }
      nvgFillColor(args.vg, nvgRGB(0xCF, 0xEA, 0xFF));  // cool white
      nvgFill(args.vg);
    }

    // subtle glow
    NVGpaint glow = nvgBoxGradient(args.vg, 0, 0, box.size.x, box.size.y, 4.f, 14.f,
                                   nvgRGBA(0x9F, 0xD8, 0xFF, 28), nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(args.vg);
    nvgRect(args.vg, -12, -12, box.size.x + 24, box.size.y + 24);
    nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2.f);
    nvgPathWinding(args.vg, NVG_HOLE);
    nvgFillPaint(args.vg, glow);
    nvgFill(args.vg);
  }
};

// ---------------------------------------------------------------------------
// Encoder widget — drag to turn, click to push, shift for push+turn
// ---------------------------------------------------------------------------
struct XlocEncoder : OpaqueWidget {
  XLOC2Module *module = nullptr;
  int which = 0;  // 0 = L, 1 = R
  float dragAccum = 0.f;
  float angle = 0.f;
  bool dragged = false;
  bool shiftHeld = false;
  bool altClicked = false;
  // Right-button drag = push+turn (modifier-free; some macOS setups never
  // deliver modifier bits to the plugin, killing shift+drag).
  bool rightHeld = false;
  bool rightTurning = false;
  float rightAccum = 0.f;
  // Rotary knob modes: virtual pointer position (widget px, accumulated
  // from drag deltas, seeded from the click position) and angle accumulator.
  Vec dragPos;
  float angleAccum = 0.f;

  static constexpr float kPxPerDetent = 12.f;
  // Hardware encoders are ~24 detents/revolution: 15 degrees per detent.
  static constexpr float kRadPerDetent = 2.f * (float)M_PI / 24.f;

  bool rotaryMode() const {
    return settings::knobMode == settings::KNOB_MODE_ROTARY_ABSOLUTE ||
           settings::knobMode == settings::KNOB_MODE_ROTARY_RELATIVE;
  }

  // Consume a drag delta (widget px) and return whole detents, honoring the
  // user's knob mode. `accum` is the linear accumulator to use.
  int detentsFromDelta(Vec deltaPx, float &accum) {
    if (rotaryMode()) {
      Vec c = box.size.div(2.f);
      Vec v0 = dragPos.minus(c);
      dragPos = dragPos.plus(deltaPx);
      Vec v1 = dragPos.minus(c);
      // Too close to the hub for a stable angle: ignore until it moves out.
      if (v0.norm() < 4.f || v1.norm() < 4.f) return 0;
      float cross = v0.x * v1.y - v0.y * v1.x;
      float dot = v0.x * v1.x + v0.y * v1.y;
      angleAccum += std::atan2(cross, dot);  // clockwise = positive (y down)
      int detents = (int)(angleAccum / kRadPerDetent);
      angleAccum -= detents * kRadPerDetent;
      return detents;
    }
    // Linear modes: vertical drag; scale with the user's knob sensitivity
    // (0.001 is Rack's default).
    float sens = settings::knobLinearSensitivity / 0.001f;
    if (sens <= 0.f) sens = 1.f;
    accum += -deltaPx.y * sens;
    int detents = (int)(accum / kPxPerDetent);
    accum -= detents * kPxPerDetent;
    return detents;
  }

  void onButton(const ButtonEvent &e) override {
    INFO("XLOC2 enc %d btn=%d action=%d mods=0x%x", which, e.button, e.action,
         e.mods);
    // Middle-click: dual encoder press (modifier-free alternative)
    if (e.button == GLFW_MOUSE_BUTTON_MIDDLE && e.action == GLFW_PRESS) {
      if (module && module->isOwner) module->dualClickEncoders();
      e.consume(this);
      return;
    }
    if (e.action == GLFW_PRESS &&
        (e.button == GLFW_MOUSE_BUTTON_LEFT || e.button == GLFW_MOUSE_BUTTON_RIGHT)) {
      dragPos = e.pos;  // seed rotary tracking at the click position
      angleAccum = 0.f;
    }
    if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
      if (e.action == GLFW_PRESS) {
        dragged = false;
        // On macOS (observed on 26.1 / Rack 2.6.6) ButtonEvent::mods
        // arrives empty, while the live window modifier state is correct —
        // stock Rack knobs work because they query the latter. Prefer the
        // live state, OR the event field in case other platforms differ.
        int mods = (e.mods | APP->window->getMods()) & RACK_MOD_MASK;
        INFO("XLOC2 enc %d leftclick emods=0x%x livemods=0x%x", which,
             e.mods, APP->window->getMods());
        shiftHeld = mods == GLFW_MOD_SHIFT;
        // Alt/Option+click or Cmd+click: dual encoder press. (Ctrl+click
        // never arrives on macOS — the OS converts it to a right-click.)
        altClicked = (mods & (GLFW_MOD_ALT | GLFW_MOD_SUPER)) != 0;
        if (module && module->isOwner) {
          if (altClicked) {
            // Alt/Option+click: press BOTH encoders together (the hardware
            // two-thumb gesture, e.g. stereo/mono toggle in audio setup).
            module->dualClickEncoders();
          } else if (shiftHeld) {
            xemu::press_encoder(which, true);  // push+turn gesture
          }
        }
        e.consume(this);
      } else if (e.action == GLFW_RELEASE) {
        if (module && module->isOwner) {
          if (shiftHeld) {
            xemu::press_encoder(which, false);
            module->noteEncoderActivity();
          } else if (!dragged && !altClicked) {
            module->clickEncoder(which, 80.f);  // short press
          }
        }
        shiftHeld = false;
        altClicked = false;
      }
    }
    // Right button: consume so this widget becomes Rack's drag target and
    // no context menu opens. The gesture itself (drag = push+turn, plain
    // click = long press) is decided in onDragMove/onDragEnd, which Rack
    // runs for right-button drags too.
    if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
      if (e.action == GLFW_PRESS) {
        rightHeld = true;
        rightTurning = false;
        rightAccum = 0.f;
      }
      e.consume(this);
    }
    OpaqueWidget::onButton(e);
  }

  // 'D' while hovering an encoder = dual press (keyboard experiment: tells
  // us whether key events reach the plugin even though mouse modifiers
  // don't). Consumed here, so Rack never sees the keystroke.
  void onHoverKey(const HoverKeyEvent &e) override {
    INFO("XLOC2 enc %d hoverKey key=%d action=%d mods=0x%x", which, e.key,
         e.action, e.mods);
    if (e.key == GLFW_KEY_D && e.action == GLFW_PRESS) {
      if (module && module->isOwner) module->dualClickEncoders();
      e.consume(this);
      return;
    }
    OpaqueWidget::onHoverKey(e);
  }

  void onDragMove(const DragMoveEvent &e) override {
    float zoom = getAbsoluteZoom();
    Vec deltaPx = e.mouseDelta.div(zoom);
    // Any encoder handling counts as activity (keeps button latches alive
    // even between detents while the hand is on the encoder).
    if (module && module->isOwner && (deltaPx.x != 0.f || deltaPx.y != 0.f))
      module->noteEncoderActivity();

    if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
      // Right-drag = push+turn: the virtual push engages with the first
      // detent and holds until the button is released.
      int detents = detentsFromDelta(deltaPx, rightAccum);
      if (detents != 0) {
        angle += detents * 0.30f;
        if (module && module->isOwner) {
          if (!rightTurning) {
            rightTurning = true;
            xemu::press_encoder(which, true);
          }
          xemu::turn_encoder(which, detents);
        }
      }
      return;
    }

    int detents = detentsFromDelta(deltaPx, dragAccum);
    if (detents != 0) {
      dragged = true;
      angle += detents * 0.30f;
      if (module && module->isOwner) xemu::turn_encoder(which, detents);
    }
  }

  void onDragEnd(const DragEndEvent &e) override {
    if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
      if (module && module->isOwner) {
        if (rightTurning) {
          xemu::press_encoder(which, false);  // end push+turn
          module->noteEncoderActivity();
        } else if (rightHeld) {
          module->clickEncoder(which, 1600.f);  // plain right-click = long press
        }
      }
      rightHeld = false;
      rightTurning = false;
    }
    OpaqueWidget::onDragEnd(e);
  }

  // Mouse wheel = turn (issue #3), following Rack convention: only when
  // "Scroll wheel knob control" is enabled in the View menu, so scrolling
  // still pans the rack otherwise. One detent per wheel notch (Rack
  // reports 50 units/notch); trackpad deltas accumulate smoothly.
  float scrollAccum = 0.f;
  void onHoverScroll(const HoverScrollEvent &e) override {
    if (!settings::knobScroll) {
      OpaqueWidget::onHoverScroll(e);
      return;
    }
    scrollAccum += e.scrollDelta.y;
    int detents = (int)(scrollAccum / 50.f);
    if (detents != 0) {
      scrollAccum -= detents * 50.f;
      angle += detents * 0.30f;
      if (module && module->isOwner) {
        xemu::turn_encoder(which, detents);
        module->noteEncoderActivity();
      }
    }
    e.consume(this);
  }

  void draw(const DrawArgs &args) override {
    float r = std::min(box.size.x, box.size.y) * 0.5f;
    float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;

    // pressed indicator: amber ring while the emulated push is active
    // (shift+drag, alt+click dual press, or a timed click in flight)
    bool pressed = shiftHeld || rightTurning || (module && module->encClickActive[which]);
    if (pressed) {
      nvgBeginPath(args.vg);
      nvgCircle(args.vg, cx, cy, r + 1.5f);
      nvgStrokeColor(args.vg, nvgRGB(0xFF, 0xC8, 0x66));
      nvgStrokeWidth(args.vg, 2.f);
      nvgStroke(args.vg);
    }

    // knurled body
    nvgBeginPath(args.vg);
    nvgCircle(args.vg, cx, cy, r);
    nvgFillColor(args.vg, nvgRGB(0x2A, 0x2C, 0x30));
    nvgFill(args.vg);
    nvgBeginPath(args.vg);
    nvgCircle(args.vg, cx, cy, r - 1.5f);
    nvgFillColor(args.vg, nvgRGB(0x3A, 0x3D, 0x43));
    nvgFill(args.vg);
    // ticks
    for (int i = 0; i < 18; ++i) {
      float a = angle + i * (2.f * M_PI / 18.f);
      nvgBeginPath(args.vg);
      nvgMoveTo(args.vg, cx + cosf(a) * (r - 2.f), cy + sinf(a) * (r - 2.f));
      nvgLineTo(args.vg, cx + cosf(a) * (r - 5.f), cy + sinf(a) * (r - 5.f));
      nvgStrokeColor(args.vg, nvgRGB(0x23, 0x25, 0x29));
      nvgStrokeWidth(args.vg, 1.2f);
      nvgStroke(args.vg);
    }
    // indicator
    nvgBeginPath(args.vg);
    nvgCircle(args.vg, cx + cosf(angle - (float)M_PI_2) * (r * 0.55f),
              cy + sinf(angle - (float)M_PI_2) * (r * 0.55f), r * 0.10f);
    nvgFillColor(args.vg, nvgRGB(0xC8, 0xD2, 0xDC));
    nvgFill(args.vg);
  }
};

// ---------------------------------------------------------------------------
// Panel button — momentary, real-time press
// ---------------------------------------------------------------------------
struct XlocButton : OpaqueWidget {
  XLOC2Module *module = nullptr;
  int pin = 0;
  bool held = false;
  bool latched = false;  // right-click: hold the button down so an encoder
                         // can be turned while it's "pressed" (applet select)
  uint64_t latchArmNs = 0;  // virtual time the latch was engaged

  // Latch auto-release: 5 s (virtual) of encoder inactivity
  static constexpr uint64_t kLatchIdleNs = 5000000000ull;

  void setPin(bool down) {
    if (module && module->isOwner) xemu::set_button(pin, down);
  }

  void step() override {
    if (latched && module && module->isOwner) {
      uint64_t now = xemu::clock().now_ns.load();
      // 1. An encoder click that started after this latch was engaged has
      //    completed — the button+encoder combo is done, let go.
      uint64_t rel = module->latchReleaseAtNs.load();
      bool combo_done = rel > latchArmNs && now >= rel;
      // 2. Encoders idle too long — latch forgotten, let go.
      uint64_t act = module->lastEncActivityNs.load();
      if (act < latchArmNs) act = latchArmNs;
      bool idle = now >= act + kLatchIdleNs;
      if (combo_done || idle) {
        latched = false;
        setPin(false);
      }
    }
    OpaqueWidget::step();
  }

  void toggleLatch() {
    latched = !latched;
    if (latched) latchArmNs = xemu::clock().now_ns.load();
    setPin(latched);
  }

  void onButton(const ButtonEvent &e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
      if (e.action == GLFW_PRESS) {
        // Ctrl/Cmd+click = latch toggle on every platform (on macOS the OS
        // converts ctrl+click into a right-click before we see it, so this
        // matches that behavior explicitly on Linux/Windows).
        int mods = (e.mods | APP->window->getMods()) & RACK_MOD_MASK;
        if (mods & (GLFW_MOD_CONTROL | GLFW_MOD_SUPER)) {
          toggleLatch();
        } else if (latched) {
          latched = false;  // click releases a latch
          setPin(false);
        } else {
          held = true;
          setPin(true);
        }
        e.consume(this);
      } else if (e.action == GLFW_RELEASE) {
        if (held) {
          held = false;
          setPin(false);
        }
      }
    }
    if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
      toggleLatch();
      e.consume(this);
    }
    OpaqueWidget::onButton(e);
  }

  void onDragEnd(const DragEndEvent &e) override {
    if (held) {
      held = false;
      setPin(false);
    }
    OpaqueWidget::onDragEnd(e);
  }

  void draw(const DrawArgs &args) override {
    float r = std::min(box.size.x, box.size.y) * 0.5f;
    float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
    bool down = held || latched;
    // dark rim
    nvgBeginPath(args.vg);
    nvgCircle(args.vg, cx, cy, r);
    nvgFillColor(args.vg, nvgRGB(0x0A, 0x0A, 0x0A));
    nvgFill(args.vg);
    // button cap: black, lit cool-white when held; amber when latched
    nvgBeginPath(args.vg);
    nvgCircle(args.vg, cx, cy, r - 1.0f);
    nvgFillColor(args.vg, latched ? nvgRGB(0xFF, 0xC8, 0x66)
                          : down  ? nvgRGB(0x9F, 0xD8, 0xFF)
                                  : nvgRGB(0x1B, 0x1B, 0x1D));
    nvgFill(args.vg);
    // faint top highlight for a moulded look
    NVGpaint hl = nvgRadialGradient(args.vg, cx, cy - r * 0.35f, 0.5f, r,
                                    nvgRGBA(0xFF, 0xFF, 0xFF, down ? 30 : 40),
                                    nvgRGBA(0xFF, 0xFF, 0xFF, 0));
    nvgBeginPath(args.vg);
    nvgCircle(args.vg, cx, cy, r - 1.0f);
    nvgFillPaint(args.vg, hl);
    nvgFill(args.vg);
  }
};

// ---------------------------------------------------------------------------
// Panel labels — drawn in code because Rack's SVG loader ignores <text>
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Module widget — geometry matches res/XLOC2.svg (traced from the original
// XLOC2 aluminium panel artwork). Panel is 22HP; the OLED aperture is
// maximised while keeping the 128x64 2:1 aspect.
// ---------------------------------------------------------------------------
struct XLOC2Widget : ModuleWidget {
  // Jack columns / rows (mm), shared with scripts/gen_panel.py.
  static constexpr float COL_TRIG = 14.5f;
  static constexpr float COL_CV1 = 32.0f, COL_CV2 = 45.0f;
  static constexpr float COL_CO1 = 66.76f, COL_CO2 = 79.76f;
  static constexpr float COL_AUD = 97.26f;
  static constexpr float ROW[4] = {77.5f, 89.5f, 101.5f, 113.5f};

  explicit XLOC2Widget(XLOC2Module *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/XLOC2.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // OLED (aperture 17.88,11 -> 93.88,49)
    auto *oled = new OledWidget();
    oled->module = module;
    oled->box.pos = mm2px(Vec(17.88f, 11.f));
    oled->box.size = mm2px(Vec(76.f, 38.f));
    addChild(oled);

    // Encoders below the screen, flanking Z
    auto addEnc = [&](float xmm, int which) {
      auto *e = new XlocEncoder();
      e->module = module;
      e->which = which;
      e->box.size = mm2px(Vec(11.2f, 11.2f));
      e->box.pos = mm2px(Vec(xmm - 5.6f, 58.5f - 5.6f));
      addChild(e);
    };
    addEnc(22.0f, 0);
    addEnc(89.76f, 1);

    // Buttons: A/X flank screen left, B/Y right, Z between the encoders.
    auto addBtn = [&](float xmm, float ymm, int pin) {
      auto *b = new XlocButton();
      b->module = module;
      b->pin = pin;
      b->box.size = mm2px(Vec(7.6f, 7.6f));
      b->box.pos = mm2px(Vec(xmm - 3.8f, ymm - 3.8f));
      addChild(b);
    };
    addBtn(8.5f, 19.0f, xemu::BTN_A);
    addBtn(8.5f, 41.0f, xemu::BTN_X);
    addBtn(103.26f, 19.0f, xemu::BTN_B);
    addBtn(103.26f, 41.0f, xemu::BTN_Y);
    addBtn(55.88f, 57.5f, xemu::BTN_Z);

    // Jacks
    auto in = [&](float xmm, int row, int id) {
      addInput(createInputCentered<PJ301MPort>(mm2px(Vec(xmm, ROW[row])), module, id));
    };
    auto out = [&](float xmm, int row, int id) {
      addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(xmm, ROW[row])), module, id));
    };
    for (int r = 0; r < 4; ++r) in(COL_TRIG, r, XLOC2Module::TR1_INPUT + r);
    for (int r = 0; r < 4; ++r) {
      in(COL_CV1, r, XLOC2Module::CV1_INPUT + r);
      in(COL_CV2, r, XLOC2Module::CV5_INPUT + r);
    }
    for (int r = 0; r < 4; ++r) {
      out(COL_CO1, r, XLOC2Module::OUTA_OUTPUT + r);
      out(COL_CO2, r, XLOC2Module::OUTE_OUTPUT + r);
    }
    // AUDIO column: L-in, R-in, L-out, R-out (inert until phase 3)
    in(COL_AUD, 0, XLOC2Module::AUDIO_L_INPUT);
    in(COL_AUD, 1, XLOC2Module::AUDIO_R_INPUT);
    out(COL_AUD, 2, XLOC2Module::AUDIO_L_OUTPUT);
    out(COL_AUD, 3, XLOC2Module::AUDIO_R_OUTPUT);
  }

  void appendContextMenu(Menu *menu) override {
    auto *m = dynamic_cast<XLOC2Module *>(module);
    if (!m) return;
    menu->addChild(new MenuSeparator);
    if (!m->isOwner) {
      menu->addChild(createMenuLabel("Inactive: another XLOC2 instance owns the firmware"));
    } else {
      menu->addChild(createMenuLabel(xemu::booted() ? "Firmware: running" : "Firmware: booting..."));
      menu->addChild(createMenuItem("Press both encoders", "",
                                    [m]() { m->dualClickEncoders(); }));
      menu->addChild(createMenuLabel("Encoder: right-DRAG = push+turn, right-click = long press"));
      menu->addChild(createMenuLabel("Encoder: hover + D key = press both encoders"));
      menu->addChild(createMenuLabel("Button: right-click or ctrl+click = latch held (for combos)"));
    }
  }
};

Model *modelXLOC2 = createModel<XLOC2Module, XLOC2Widget>("XLOC2");
