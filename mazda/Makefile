# Cross-compiles the CarPlay -> HUD bridge, an LD_PRELOAD library for the
# jciCARPLAY process on Mazda Connect (CMU150, firmware 74.00.324).
#
#   make                 build/release/libpatch-blmjcicarplay.so
#   make diag            build/diag/...     the release build with a debug log
#                        on the persistent partition, for finding problems in
#                        the car (see main.cpp)
#   make debug           build/debug/...    unoptimised, logs everything
#   make clean
#
# Options, given on the command line (make UNITS=imperial):
#   UNITS=metric         distances in metres and kilometres (default)
#   UNITS=imperial       distances in feet and miles
#   VIETNAMESE=1         replace the Vietnamese letters the HUD font cannot
#                        draw (see hud/hud_send.cpp); default 0
#   M3TOOLCHAIN=<dir>    a checkout of lmagder/m3-toolchain, the ARM
#                        cross-compiler (an independent, third-party
#                        project); default ./m3-toolchain
#
# The library contains no licensing, DRM or network code. It decodes Apple's
# turn-by-turn messages inside jciCARPLAY and passes them to the OEM HUD
# D-Bus service (com.jci.vbs.navi).

M3TOOLCHAIN ?= ./m3-toolchain
UNITS       ?= metric
VIETNAMESE  ?= 0

TRIPLE  := arm-cortexa9_neon-linux-gnueabi
CXX     := $(M3TOOLCHAIN)/bin/$(TRIPLE)-g++
SYSROOT := $(M3TOOLCHAIN)/$(TRIPLE)/sysroot

PATCH := blmjcicarplay

ifeq ($(UNITS),imperial)
  UNIT_FLAGS := -DHUDLINK_IMPERIAL=1
else ifeq ($(UNITS),metric)
  UNIT_FLAGS :=
else
  $(error UNITS must be metric or imperial, not "$(UNITS)")
endif

FEATURE_FLAGS := $(UNIT_FLAGS) -DCARPLAY_VN_NORMALIZE=$(VIETNAMESE)

# The flags match what the head unit runs: an ARMv7 Cortex-A9 with NEON, and
# the libstdc++ of GCC 4.6, which only has the pre-C++11 (copy-on-write)
# std::string ABI, hence -D_GLIBCXX_USE_CXX11_ABI=0. -fvisibility=hidden keeps
# everything but the hooks out of the dynamic symbol table.
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
DIAG_CXXFLAGS    := $(RELEASE_CXXFLAGS) -DHUDLINK_DIAG -DLOG_LEVEL=1
DIAG_LDFLAGS     := $(RELEASE_LDFLAGS)
DEBUG_CXXFLAGS   := $(COMMON_CXXFLAGS) -O0 -g3 -DDEBUG
DEBUG_LDFLAGS    := $(COMMON_LDFLAGS)

BUILD_DIR   := build
RELEASE_DIR := $(BUILD_DIR)/release
DIAG_DIR    := $(BUILD_DIR)/diag
DEBUG_DIR   := $(BUILD_DIR)/debug

# The sources, in the order the tested builds link them. The link order
# shapes the library, so a fixed list (rather than a directory listing, whose
# order depends on the file system) gives the same bytes on every machine.
SRCS         := $(addprefix patches/$(PATCH)/, \
                  nav.cpp \
                  hud/hud_send.cpp \
                  main.cpp \
                  oem/libjcivbsnaviclient.cpp \
                  oem/libjcidbus.cpp \
                  devmgr_shim.cpp)
RELEASE_OBJS := $(patsubst patches/$(PATCH)/%.cpp,$(RELEASE_DIR)/$(PATCH)/%.o,$(SRCS))
DIAG_OBJS    := $(patsubst patches/$(PATCH)/%.cpp,$(DIAG_DIR)/$(PATCH)/%.o,$(SRCS))
DEBUG_OBJS   := $(patsubst patches/$(PATCH)/%.cpp,$(DEBUG_DIR)/$(PATCH)/%.o,$(SRCS))

.PHONY: all release diag debug clean FORCE
.DEFAULT_GOAL := release

all:     release diag debug
release: $(RELEASE_DIR)/libpatch-$(PATCH).so
diag:    $(DIAG_DIR)/libpatch-$(PATCH).so
debug:   $(DEBUG_DIR)/libpatch-$(PATCH).so

# Each build directory keeps the options it was last built with in a file
# that is rewritten only when they change, so changing UNITS or VIETNAMESE
# rebuilds the objects without a "make clean".
$(RELEASE_DIR)/options $(DIAG_DIR)/options $(DEBUG_DIR)/options: FORCE
	@mkdir -p $(@D)
	@echo '$(FEATURE_FLAGS)' | cmp -s - $@ || echo '$(FEATURE_FLAGS)' > $@

$(RELEASE_DIR)/$(PATCH)/%.o: patches/$(PATCH)/%.cpp $(RELEASE_DIR)/options
	@mkdir -p $(@D)
	$(CXX) -MD $(RELEASE_CXXFLAGS) -c $< -o $@

$(DIAG_DIR)/$(PATCH)/%.o: patches/$(PATCH)/%.cpp $(DIAG_DIR)/options
	@mkdir -p $(@D)
	$(CXX) -MD $(DIAG_CXXFLAGS) -c $< -o $@

$(DEBUG_DIR)/$(PATCH)/%.o: patches/$(PATCH)/%.cpp $(DEBUG_DIR)/options
	@mkdir -p $(@D)
	$(CXX) -MD $(DEBUG_CXXFLAGS) -c $< -o $@

$(RELEASE_DIR)/libpatch-$(PATCH).so: $(RELEASE_OBJS)
	$(CXX) $^ $(RELEASE_LDFLAGS) -Wl,-soname,libpatch-$(PATCH).so -o $@
	@echo "built -> $@"

$(DIAG_DIR)/libpatch-$(PATCH).so: $(DIAG_OBJS)
	$(CXX) $^ $(DIAG_LDFLAGS) -Wl,-soname,libpatch-$(PATCH).so -o $@
	@echo "built -> $@"

$(DEBUG_DIR)/libpatch-$(PATCH).so: $(DEBUG_OBJS)
	$(CXX) $^ $(DEBUG_LDFLAGS) -Wl,-soname,libpatch-$(PATCH).so -o $@
	@echo "built -> $@"

clean:
	rm -rf $(BUILD_DIR)

-include $(RELEASE_OBJS:.o=.d)
-include $(DIAG_OBJS:.o=.d)
-include $(DEBUG_OBJS:.o=.d)
