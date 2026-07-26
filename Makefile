# VCV Rack plugin build for the XLOC2 emulator.
# The firmware submodule must have patches/ applied first: make apply-patches
RACK_DIR ?= ../Rack

FW := firmware/software/src

FLAGS += -Ishim/include -I$(FW) -I$(FW)/src/extern \
         -Ifirmware/software/teensy-variable-playback/src -I.
FLAGS += -DARDUINO_TEENSY41 -D__IMXRT1062__ -DARDUINO=10819 -DTEENSYDUINO=159 \
         -DUSB_MIDI -DDRUMMAP_GRIDS2 \
         -DENABLE_APP_CALIBR8OR -DENABLE_APP_SCENES -DENABLE_APP_PONG \
         -DOC_VERSION_EXTRA="\"_VCV\""

# Firmware needs GNU extensions and C++17; Rack's compile.mk sets -std=c++11,
# ours must come later. (No -fpermissive: Apple clang doesn't know it, and the
# host-portability patches fixed the const-correctness issues properly.)
EXTRA_CXXFLAGS += -std=gnu++17 -Wno-unused-parameter \
                  -Wno-unused-variable -Wno-sign-compare

# macOS: Rack targets 10.9, but the firmware uses std::variant/std::get whose
# libc++ availability annotations demand 10.13+. Every Mac that can run Rack 2
# has the needed runtime; disable the annotations. Inert on GCC/libstdc++.
FLAGS += -D_LIBCPP_DISABLE_AVAILABILITY

# Plugin sources
SOURCES += src/plugin.cpp src/XLOC2.cpp
SOURCES += emu/xloc_emu.cpp
SOURCES += shim/src/shim_arduino.cpp shim/src/shim_eeprom.cpp \
           shim/src/shim_fs.cpp shim/src/shim_globals.cpp \
           shim/src/emu_OC_ADC.cpp shim/src/emu_OC_digital_inputs.cpp \
           shim/src/emu_OC_FreqMeasure.cpp shim/src/emu_SH1106.cpp

# Firmware sources (hardware-only TUs excluded; see shim/src replacements)
FW_EXCLUDE := $(FW)/OC_ADC.cpp $(FW)/OC_digital_inputs.cpp
SOURCES += $(filter-out $(FW_EXCLUDE),$(wildcard $(FW)/*.cpp))
SOURCES += $(wildcard $(FW)/src/Audio/*.cpp)
SOURCES += $(FW)/src/drivers/display.cpp $(FW)/src/drivers/weegfx.cpp
SOURCES += $(wildcard $(FW)/src/extern/*.cpp)
SOURCES += $(FW)/src/util/util_misc.cpp $(FW)/src/util/util_settings.cpp

DISTRIBUTABLES += res

include $(RACK_DIR)/plugin.mk

.PHONY: apply-patches
apply-patches:
	cd firmware && git apply --3way ../patches/*.patch || true
