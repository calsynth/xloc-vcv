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
  bool encClickActive[2] = {false, false};

  bool trigHigh[4] = {false, false, false, false};

  XLOC2Module() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    for (int i = 0; i < 8; ++i)
      configInput(CV1_INPUT + i, string::f("CV %d", i + 1));
    for (int i = 0; i < 4; ++i)
      configInput(TR1_INPUT + i, string::f("Trigger %d", i + 1));
    static const char *outNames = "ABCDEFGH";
    for (int i = 0; i < 8; ++i)
      configOutput(OUTA_OUTPUT + i, string::f("CV %c", outNames[i]));
    configInput(AUDIO_L_INPUT, "Audio L (audio applets not yet ported — inert)");
    configInput(AUDIO_R_INPUT, "Audio R (audio applets not yet ported — inert)");
    configOutput(AUDIO_L_OUTPUT, "Audio L (audio applets not yet ported — silent)");
    configOutput(AUDIO_R_OUTPUT, "Audio R (audio applets not yet ported — silent)");

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
    encClickUntil[which].store(now + (uint64_t)(virtualMs * 1e6));
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
      bool want = now < until;
      if (want != encClickActive[e]) {
        encClickActive[e] = want;
        xemu::press_encoder(e, want);
      }
    }

    // Advance the firmware by one sample of virtual time; this fires the
    // 16.666 kHz core ISR and 1 kHz UI ISR at their due times.
    xemu::clock().step((uint64_t)(args.sampleTime * 1e9));

    // Collect outputs
    for (int i = 0; i < 8; ++i)
      outputs[OUTA_OUTPUT + i].setVoltage(xemu::get_cv_out_volts(i));
    outputs[AUDIO_L_OUTPUT].setVoltage(0.f);  // phase 3: audio applet DSP
    outputs[AUDIO_R_OUTPUT].setVoltage(0.f);
  }
};

std::atomic<XLOC2Module *> XLOC2Module::owner{nullptr};

// ---------------------------------------------------------------------------
// OLED widget — renders the live SH1106 framebuffer
// ---------------------------------------------------------------------------
struct OledWidget : TransparentWidget {
  XLOC2Module *module = nullptr;
  int img = -1;
  uint8_t rgba[xemu::kFBWidth * xemu::kFBHeight * 4];

  void drawLayer(const DrawArgs &args, int layer) override {
    if (layer != 1) return;  // self-illuminating layer (glows in dark rooms)

    uint8_t fb[xemu::kFBSize];
    bool live = module && module->isOwner;
    if (live) xemu::get_framebuffer(fb);

    for (int y = 0; y < xemu::kFBHeight; ++y) {
      for (int x = 0; x < xemu::kFBWidth; ++x) {
        int page = y >> 3, bit = y & 7;
        bool on = live && ((fb[page * 128 + x] >> bit) & 1);
        uint8_t *p = &rgba[(y * xemu::kFBWidth + x) * 4];
        if (on) {
          p[0] = 0xCF; p[1] = 0xEA; p[2] = 0xFF; p[3] = 0xFF;  // cool white
        } else {
          p[0] = 0x0A; p[1] = 0x0E; p[2] = 0x14; p[3] = 0xFF;  // near-black
        }
      }
    }

    if (img < 0) {
      img = nvgCreateImageRGBA(args.vg, xemu::kFBWidth, xemu::kFBHeight,
                               NVG_IMAGE_NEAREST, rgba);
    } else {
      nvgUpdateImage(args.vg, img, rgba);
    }

    NVGpaint paint = nvgImagePattern(args.vg, 0, 0, box.size.x, box.size.y, 0, img, 1.f);
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2.f);
    nvgFillPaint(args.vg, paint);
    nvgFill(args.vg);

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

  static constexpr float kPxPerDetent = 12.f;

  void onButton(const ButtonEvent &e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
      if (e.action == GLFW_PRESS) {
        dragged = false;
        shiftHeld = (e.mods & RACK_MOD_MASK) == GLFW_MOD_SHIFT;
        if (shiftHeld && module && module->isOwner)
          xemu::press_encoder(which, true);  // push+turn gesture
        e.consume(this);
      } else if (e.action == GLFW_RELEASE) {
        if (module && module->isOwner) {
          if (shiftHeld) {
            xemu::press_encoder(which, false);
          } else if (!dragged) {
            module->clickEncoder(which, 80.f);  // short press
          }
        }
        shiftHeld = false;
      }
    }
    // Right-click = long press (1.5 s virtual)
    if (e.button == GLFW_MOUSE_BUTTON_RIGHT && e.action == GLFW_PRESS) {
      if (module && module->isOwner) module->clickEncoder(which, 1600.f);
      e.consume(this);
    }
    OpaqueWidget::onButton(e);
  }

  void onDragMove(const DragMoveEvent &e) override {
    float zoom = getAbsoluteZoom();
    dragAccum += -e.mouseDelta.y / zoom;
    int detents = (int)(dragAccum / kPxPerDetent);
    if (detents != 0) {
      dragAccum -= detents * kPxPerDetent;
      dragged = true;
      angle += detents * 0.30f;
      if (module && module->isOwner) xemu::turn_encoder(which, detents);
    }
  }

  void draw(const DrawArgs &args) override {
    float r = std::min(box.size.x, box.size.y) * 0.5f;
    float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;

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

  void setPin(bool down) {
    if (module && module->isOwner) xemu::set_button(pin, down);
  }

  void onButton(const ButtonEvent &e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
      if (e.action == GLFW_PRESS) {
        if (latched) {
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
      latched = !latched;
      setPin(latched);
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
      menu->addChild(createMenuLabel("Encoder: shift+drag = push+turn, right-click = long press"));
      menu->addChild(createMenuLabel("Button: right-click = latch held (for button+encoder combos)"));
    }
  }
};

Model *modelXLOC2 = createModel<XLOC2Module, XLOC2Widget>("XLOC2");
