# Headless build of the XLOC2 emulator core (firmware + shims) and test
# harness. The VCV Rack plugin has its own Makefile (plugin.mk) but compiles
# the same source lists — keep XLOC_FW_SOURCES / XLOC_SHIM_SOURCES in sync.
cmake_minimum_required(VERSION 3.16)
project(xloc-emu CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED FIRMWARE_DIR)
  set(FIRMWARE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/firmware)
endif()
set(FW ${FIRMWARE_DIR}/software/src)

# Firmware translation units, minus hardware-only ones we replace:
#   OC_ADC.cpp                -> shim/src/emu_OC_ADC.cpp
#   src/drivers/SH1106...cpp  -> shim/src/emu_SH1106.cpp
#   src/drivers/ADC/*         -> T3-era ADC library, not needed
file(GLOB FW_ROOT_SOURCES ${FW}/*.cpp)
list(REMOVE_ITEM FW_ROOT_SOURCES ${FW}/OC_ADC.cpp ${FW}/OC_digital_inputs.cpp)

set(XLOC_FW_SOURCES
  ${FW_ROOT_SOURCES}
  ${FW}/src/Audio/SafeNoteFrequencyAnalyzer.cpp
  ${FW}/src/Audio/effect_dynamics.cpp
  ${FW}/src/Audio/filter_variable2.cpp
  ${FW}/src/Audio/synth_advanced_karplus.cpp
  ${FW}/src/drivers/display.cpp
  ${FW}/src/drivers/weegfx.cpp
  ${FW}/src/extern/bjorklund.cpp
  ${FW}/src/extern/braids_quantizer.cpp
  ${FW}/src/extern/frames_poly_lfo.cpp
  ${FW}/src/extern/frames_resources.cpp
  ${FW}/src/extern/peaks_bytebeat.cpp
  ${FW}/src/extern/peaks_multistage_envelope.cpp
  ${FW}/src/extern/peaks_resources.cpp
  ${FW}/src/extern/stmlib_utils_random.cpp
  ${FW}/src/extern/streams_lorenz_generator.cpp
  ${FW}/src/extern/streams_resources.cpp
  ${FW}/src/util/util_misc.cpp
  ${FW}/src/util/util_settings.cpp
)

set(XLOC_SHIM_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/emu/xloc_emu.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/shim/src/shim_arduino.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/shim/src/shim_eeprom.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/shim/src/shim_fs.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/shim/src/shim_globals.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/shim/src/emu_OC_ADC.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/shim/src/emu_OC_digital_inputs.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/shim/src/emu_OC_FreqMeasure.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/shim/src/emu_SH1106.cpp
)

add_library(xloc_core STATIC ${XLOC_FW_SOURCES} ${XLOC_SHIM_SOURCES})

target_include_directories(xloc_core BEFORE PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/shim/include
)
target_include_directories(xloc_core PUBLIC
  ${FIRMWARE_DIR}/software/teensy-variable-playback/src
  ${FW}
  ${FW}/src/extern
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(xloc_core PUBLIC
  ARDUINO_TEENSY41
  __IMXRT1062__
  ARDUINO=10819
  TEENSYDUINO=159
  USB_MIDI
  DRUMMAP_GRIDS2
  ENABLE_APP_CALIBR8OR
  ENABLE_APP_SCENES
  ENABLE_APP_PONG
  OC_VERSION_EXTRA="_XEMU"
)

target_compile_options(xloc_core PRIVATE
  -fno-exceptions
  -Wall
  -Wno-unused-parameter
  -Wno-unused-variable
  -Wno-sign-compare
)

find_package(Threads REQUIRED)
target_link_libraries(xloc_core PUBLIC Threads::Threads)

add_executable(headless_test test/headless.cpp)
target_link_libraries(headless_test PRIVATE xloc_core)
