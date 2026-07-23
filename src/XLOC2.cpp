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
    NUM_INPUTS
  };
  enum OutputIds {
    OUTA_OUTPUT, OUTB_OUTPUT, OUTC_OUTPUT, OUTD_OUTPUT,
    OUTE_OUTPUT, OUTF_OUTPUT, OUTG_OUTPUT, OUTH_OUTPUT,
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

  void onButton(const ButtonEvent &e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
      if (e.action == GLFW_PRESS) {
        held = true;
        if (module && module->isOwner) xemu::set_button(pin, true);
        e.consume(this);
      } else if (e.action == GLFW_RELEASE) {
        held = false;
        if (module && module->isOwner) xemu::set_button(pin, false);
      }
    }
    OpaqueWidget::onButton(e);
  }

  void onDragEnd(const DragEndEvent &e) override {
    if (held) {
      held = false;
      if (module && module->isOwner) xemu::set_button(pin, false);
    }
    OpaqueWidget::onDragEnd(e);
  }

  void draw(const DrawArgs &args) override {
    float r = std::min(box.size.x, box.size.y) * 0.5f;
    float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
    nvgBeginPath(args.vg);
    nvgCircle(args.vg, cx, cy, r);
    nvgFillColor(args.vg, nvgRGB(0x22, 0x24, 0x28));
    nvgFill(args.vg);
    nvgBeginPath(args.vg);
    nvgCircle(args.vg, cx, cy, r - 1.2f);
    nvgFillColor(args.vg, held ? nvgRGB(0x9F, 0xD8, 0xFF) : nvgRGB(0xE8, 0xE4, 0xDC));
    nvgFill(args.vg);
  }
};

// ---------------------------------------------------------------------------
// Panel labels — drawn in code because Rack's SVG loader ignores <text>
// ---------------------------------------------------------------------------
struct PanelLabels : TransparentWidget {
  void draw(const DrawArgs &args) override {
    std::shared_ptr<window::Font> font =
        APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf"));
    if (!font) return;
    nvgFontFaceId(args.vg, font->handle);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);

    auto label = [&](float xmm, float ymm, const char *s, float sizemm,
                     NVGcolor color, int align = NVG_ALIGN_CENTER,
                     float spacing = 0.4f) {
      nvgFontSize(args.vg, mm2px(sizemm));
      nvgTextLetterSpacing(args.vg, spacing);
      nvgFillColor(args.vg, color);
      nvgTextAlign(args.vg, align | NVG_ALIGN_BASELINE);
      nvgText(args.vg, mm2px(xmm), mm2px(ymm), s, nullptr);
    };

    NVGcolor cream = nvgRGB(0xE8, 0xE4, 0xDC);
    NVGcolor steel = nvgRGB(0x8F, 0xA1, 0xB3);
    NVGcolor faint = nvgRGB(0x4A, 0x55, 0x63);
    NVGcolor ice = nvgRGB(0xAF, 0xC2, 0xD4);

    label(8.f, 6.4f, "XLOC2", 4.6f, nvgRGB(0xF2, 0xEF, 0xE9), NVG_ALIGN_LEFT, 1.2f);
    label(144.4f, 6.4f, "CALSYNTH", 3.0f, steel, NVG_ALIGN_RIGHT, 1.6f);
    label(16.f, 37.8f, "NAV", 2.4f, steel);
    label(136.4f, 37.8f, "EDIT", 2.4f, steel);

    static const char *btn[5] = {"A", "X", "Z", "Y", "B"};
    for (int i = 0; i < 5; ++i) label(48.2f + i * 14.f, 59.f, btn[i], 2.6f, cream);

    label(8.f, 67.4f, "CV IN", 2.4f, steel, NVG_ALIGN_LEFT, 0.8f);
    for (int i = 0; i < 8; ++i) {
      char n[4];
      snprintf(n, sizeof n, "%d", i + 1);
      label(16.7f + i * 17.f, 81.4f, n, 2.4f, ice);
    }
    label(8.f, 85.4f, "TRIG", 2.4f, steel, NVG_ALIGN_LEFT, 0.8f);
    for (int i = 0; i < 4; ++i) {
      char n[8];
      snprintf(n, sizeof n, "TR%d", i + 1);
      label(16.7f + i * 17.f, 99.4f, n, 2.4f, ice);
    }
    label(111.9f, 91.f, "PHAZERVILLE / TEENSY 4.1", 2.0f, faint);
    label(8.f, 101.8f, "CV OUT", 2.4f, steel, NVG_ALIGN_LEFT, 0.8f);
    for (int i = 0; i < 8; ++i) {
      char n[2] = {(char)('A' + i), 0};
      label(16.7f + i * 17.f, 125.4f, n, 2.6f, cream);
    }
  }
};

// ---------------------------------------------------------------------------
// Module widget
// ---------------------------------------------------------------------------
struct XLOC2Widget : ModuleWidget {
  explicit XLOC2Widget(XLOC2Module *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/XLOC2.svg")));

    auto *labels = new PanelLabels();
    labels->box.size = box.size;
    addChild(labels);

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // OLED
    auto *oled = new OledWidget();
    oled->module = module;
    oled->box.pos = mm2px(Vec(44.2f, 8.f));
    oled->box.size = mm2px(Vec(64.f, 32.f));
    addChild(oled);

    // Encoders
    auto *encL = new XlocEncoder();
    encL->module = module;
    encL->which = 0;
    encL->box.size = mm2px(Vec(16.f, 16.f));
    encL->box.pos = mm2px(Vec(16.f - 8.f, 24.f - 8.f));
    addChild(encL);

    auto *encR = new XlocEncoder();
    encR->module = module;
    encR->which = 1;
    encR->box.size = mm2px(Vec(16.f, 16.f));
    encR->box.pos = mm2px(Vec(136.4f - 8.f, 24.f - 8.f));
    addChild(encR);

    // Buttons A X Z Y B (Z in the middle like the hardware)
    static const int pins[5] = {xemu::BTN_A, xemu::BTN_X, xemu::BTN_Z, xemu::BTN_Y, xemu::BTN_B};
    for (int i = 0; i < 5; ++i) {
      auto *b = new XlocButton();
      b->module = module;
      b->pin = pins[i];
      b->box.size = mm2px(Vec(8.f, 8.f));
      b->box.pos = mm2px(Vec(48.2f + i * 14.f - 4.f, 49.f - 4.f));
      addChild(b);
    }

    // Jacks
    auto colX = [](int i) { return 16.7f + i * 17.f; };
    for (int i = 0; i < 8; ++i)
      addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX(i), 72.f)), module,
                                               XLOC2Module::CV1_INPUT + i));
    for (int i = 0; i < 4; ++i)
      addInput(createInputCentered<PJ301MPort>(mm2px(Vec(colX(i), 90.f)), module,
                                               XLOC2Module::TR1_INPUT + i));
    for (int i = 0; i < 8; ++i)
      addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(colX(i), 112.f)), module,
                                                 XLOC2Module::OUTA_OUTPUT + i));
  }

  void appendContextMenu(Menu *menu) override {
    auto *m = dynamic_cast<XLOC2Module *>(module);
    if (!m) return;
    menu->addChild(new MenuSeparator);
    if (!m->isOwner) {
      menu->addChild(createMenuLabel("Inactive: another XLOC2 instance owns the firmware"));
    } else {
      menu->addChild(createMenuLabel(xemu::booted() ? "Firmware: running" : "Firmware: booting..."));
      menu->addChild(createMenuLabel("Shift+drag encoder = push+turn, right-click = long press"));
    }
  }
};

Model *modelXLOC2 = createModel<XLOC2Module, XLOC2Widget>("XLOC2");
