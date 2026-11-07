# Cross-compile Makefile for the CarPlay -> HUD bridge LD_PRELOAD shim.
#
# Builds patches/blmjcicarplay/*.cpp into:
#   build/release/libpatch-blmjcicarplay.so   (default: `make` or `make release`)
#   build/debug/libpatch-blmjcicarplay.so     (`make debug`)
#
# Toolchain: the ARM Cortex-A9 NEON / glibc cross-compiler from lmagder/m3-toolchain
# (an independent, third-party project). Point M3TOOLCHAIN at a checkout of it:
#   make M3TOOLCHAIN=/path/to/m3-toolchain
# The default is ./m3-toolchain next to this Makefile.
#
# The shim has NO licensing / DRM / network code — it is a plain LD_PRELOAD bridge
# that decodes Apple iAP2 turn-by-turn from the OEM jciCARPLAY process and forwards
# it to the OEM navigation HUD D-Bus API (com.jci.vbs.navi).

M3TOOLCHAIN ?= ./m3-toolchain
TRIPLE      := arm-cortexa9_neon-linux-gnueabi
CXX         := $(M3TOOLCHAIN)/bin/$(TRIPLE)-g++
SYSROOT     := $(M3TOOLCHAIN)/$(TRIPLE)/sysroot

PATCH := blmjcicarplay

# Build-time options (define to taste):
#   CARPLAY_VN_NORMALIZE  = strip Vietnamese diacritics so road names render on the
#                              HUD's limited font. Set to 0 / remove for other locales.
FEATURE_FLAGS := -DCARPLAY_VN_NORMALIZE=1

# -D_GLIBCXX_USE_CXX11_ABI=0 selects the pre-C++11 (COW) std::string/std::list ABI:
# the device ships libstdc++.so.6.0.14 (GCC 4.6 era) which only knows that ABI.
# -fvisibility=hidden keeps our symbols private; -fPIC is required for a shared object.
COMMON_CXXFLAGS := -std=c++11 \
                   --sysroot=$(SYSROOT) \
                   -march=armv7-a -mtune=cortex-a9 -mfpu=neon \
                   -fPIC \
                   -fvisibility=hidden \
                   -D_GLIBCXX_USE_CXX11_ABI=0 \
                   $(FEATURE_FLAGS)
COMMON_LDFLAGS  := --sysroot=$(SYSROOT) -shared \
                   -Wl,--no-undefined \
                   -lpthread -ldl

RELEASE_CXXFLAGS := $(COMMON_CXXFLAGS) -O3 -DNDEBUG -ffunction-sections -fdata-sections -flto
RELEASE_LDFLAGS  := $(COMMON_LDFLAGS) -O3 -flto -Wl,--gc-sections -Wl,-s
DEBUG_CXXFLAGS   := $(COMMON_CXXFLAGS) -O0 -g3 -DDEBUG
DEBUG_LDFLAGS    := $(COMMON_LDFLAGS)

BUILD_DIR   := build
RELEASE_DIR := $(BUILD_DIR)/release
DEBUG_DIR   := $(BUILD_DIR)/debug

SRCS         := $(shell find patches/$(PATCH) -name '*.cpp' -type f)
RELEASE_OBJS := $(patsubst patches/$(PATCH)/%.cpp,$(RELEASE_DIR)/$(PATCH)/%.o,$(SRCS))
DEBUG_OBJS   := $(patsubst patches/$(PATCH)/%.cpp,$(DEBUG_DIR)/$(PATCH)/%.o,$(SRCS))

.PHONY: all release debug clean
.DEFAULT_GOAL := release

all:     release debug
release: $(RELEASE_DIR)/libpatch-$(PATCH).so
debug:   $(DEBUG_DIR)/libpatch-$(PATCH).so

$(RELEASE_DIR)/$(PATCH)/%.o: patches/$(PATCH)/%.cpp
	@mkdir -p $(@D)
	$(CXX) -MD $(RELEASE_CXXFLAGS) -c $< -o $@

$(DEBUG_DIR)/$(PATCH)/%.o: patches/$(PATCH)/%.cpp
	@mkdir -p $(@D)
	$(CXX) -MD $(DEBUG_CXXFLAGS) -c $< -o $@

$(RELEASE_DIR)/libpatch-$(PATCH).so: $(RELEASE_OBJS)
	$(CXX) $^ $(RELEASE_LDFLAGS) -Wl,-soname,libpatch-$(PATCH).so -o $@
	@echo "built -> $@"

$(DEBUG_DIR)/libpatch-$(PATCH).so: $(DEBUG_OBJS)
	$(CXX) $^ $(DEBUG_LDFLAGS) -Wl,-soname,libpatch-$(PATCH).so -o $@
	@echo "built -> $@"

clean:
	rm -rf $(BUILD_DIR)

-include $(RELEASE_OBJS:.o=.d)
-include $(DEBUG_OBJS:.o=.d)
