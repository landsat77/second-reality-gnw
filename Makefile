# Second Reality (Future Crew) for Game & Watch Retro-Go SD — GWHB homebrew
#
#   make          the homebrew (build/sd/homebrews/SecondReality.bin) and its
#                 data pack (SecondReality.srd, beside it)
#   make host     PC build (SDL2), same code: ./SecondReality_host
#   make pack     only the pack
#
# Verbose compiler lines: make V=

PROJECT_KIND := homebrew
CORE_NAME  := sr
CORE_ENTRY := app_main
HB_NAME    := Second Reality
PACKED_BIN := SecondReality.bin
SD_DIR     := build/sd/homebrews
PACK_SRD   := $(SD_DIR)/SecondReality.srd

SR_AUDIO_RATE ?= 44100

GEN := build/gen

# the demo: Second Reality++ (src/sr), its data in the pack (src/sr_data)
SR_CXX_SOURCES := \
	$(wildcard src/sr/Parts/*/*.cpp) \
	src/sr/Parts/Common.cpp \
	src/sr/Graphics/shims.cpp \
	src/sr/Music/music.cpp \
	src/sr/Music/st3play.cpp \
	src/sr/Blob/Blob.cpp

CORE_CXX_SOURCES := \
	$(SR_CXX_SOURCES) \
	src/port/sr_glue.cpp \
	src/port/sr_pack.cpp \
	src/port/sr_libc.cpp \
	$(GEN)/sr_ram_data.cpp \
	$(GEN)/sr_hot.cpp \
	$(GEN)/sr_flash_refs.cpp

CORE_C_SOURCES := \
	src/port/sr_main.c \
	src/port/sr_overlay.c

CORE_C_INCLUDES := -Isrc/sr -Isrc/sr/Music -Isrc/port -I$(GEN)
CORE_VERSION ?= $(shell git describe --tags --dirty 2>/dev/null || echo NOTAG)
CORE_C_DEFS := -DPROJECT_KIND_HOMEBREW=1 -DSR_GNW=1 -DSR_AUDIO_RATE=$(SR_AUDIO_RATE) -DSR_VERSION=\"$(CORE_VERSION)\"
OPT := -O2
CORE_LDSCRIPT := src/port/sr_core.ld
CORE_ELF_EXTRA_DEPS := $(GEN)/sr_overlays.ld
CORE_LDLIBS := -L$(GEN) -Wl,--start-group -lgcc -Wl,--end-group

GNW_CORE_SDK ?= sdk
BUILD_DIR ?= build/$(PROJECT_KIND)

include $(GNW_CORE_SDK)/Makefile

# C++ of the demo: C++20 (as Second Reality++), its warnings are not ours
SR_OBJECTS := $(addprefix $(BUILD_DIR)/,$(notdir $(SR_CXX_SOURCES:.cpp=.o)))
$(SR_OBJECTS) $(BUILD_DIR)/sr_glue.o $(BUILD_DIR)/sr_ram_data.o $(BUILD_DIR)/sr_hot.o $(BUILD_DIR)/sr_flash_refs.o \
	$(BUILD_DIR)/sr_pack.o $(BUILD_DIR)/sr_libc.o: CXXFLAGS += -std=gnu++20 -w
$(CXX_OBJECTS) $(C_OBJECTS): $(GEN)/.stamp Makefile

# the references to the pack are bound by sr_bind_data(), not at entry
$(BUILD_DIR)/sr_flash_refs.o: $(GEN)/sr_flash_refs.cpp $(GEN)/.stamp | $(BUILD_DIR)
	$(V)$(ECHO) [ CXX ] $(notdir $<)
	$(V)$(CXX) -c $(CXXFLAGS) $< -o $@
	$(V)$(CP) --redefine-syms=$(REDEFINE_SYMS_FILE) --rename-section .init_array=sr_init_array $@

#######################################
# data: generated sources and the pack
#######################################
SR_DATA_FILES := $(shell find src/sr_data -type f)

$(GEN)/.stamp: tools/sr_data.py tools/sr_overlays.py $(SR_DATA_FILES)
	$(V)$(ECHO) [ DATA ] $(GEN)
	$(V)mkdir -p $(GEN)
	$(V)python3 tools/sr_data.py src/sr_data $(GEN)
	$(V)touch $@

$(GEN)/sr_ram_data.cpp $(GEN)/sr_hot.cpp $(GEN)/sr_flash_refs.cpp $(GEN)/sr_data.entries: $(GEN)/.stamp

$(GEN)/sr_music.entries: tools/sr_music.cpp src/sr/Music/st3play.cpp src/sr_data/Music/REALITY.FC.cpp
	$(V)$(ECHO) [ MUSIC ] $@
	$(V)mkdir -p $(GEN)
	$(V)g++ -std=c++20 -O1 -w -DSR_GNW -DSR_ST3_EXPORT -Isrc/sr/Music -Isrc/sr_data/Music -Isrc/port \
		tools/sr_music.cpp src/sr/Music/st3play.cpp src/sr_data/Music/REALITY.FC.cpp -o $(GEN)/sr_music
	$(V)$(GEN)/sr_music $@

$(PACK_SRD): tools/sr_pack.py $(GEN)/.stamp $(GEN)/sr_music.entries
	$(V)mkdir -p $(dir $@)
	$(V)python3 tools/sr_pack.py $@ $(GEN)/sr_data.entries $(GEN)/sr_music.entries

# the parts' memory (src/port/sr_overlay.c): linker script and table
$(GEN)/sr_overlays.ld: tools/sr_overlays.py
	$(V)mkdir -p $(GEN)
	$(V)python3 tools/sr_overlays.py ld $(GEN)
$(BUILD_DIR)/sr_overlay.o: $(GEN)/sr_overlays.ld

#######################################
# packing
#######################################

PACK_HOMEBREW := $(GNW_CORE_SDK)/tools/pack_homebrew.py
MAKE_COVER    := scripts/make_cover.py
COVER_SRC     := src/assets/cover.png
COVER_JPG     := $(BUILD_DIR)/cover.jpg

.PHONY: pack cover
cover: $(COVER_JPG)

$(COVER_JPG): $(MAKE_COVER) $(COVER_SRC) | $(BUILD_DIR)
	$(V)$(ECHO) "[ COVER ] $(COVER_JPG)"
	$(V)python3 $(MAKE_COVER) --in $(COVER_SRC) --out $(COVER_JPG)

$(PACKED_BIN): $(TARGET_BIN) $(COVER_JPG)
	$(V)$(ECHO) [ PACK GWHB ] $(PACKED_BIN) version=$(CORE_VERSION)
	$(V)python3 $(PACK_HOMEBREW) \
		--elf $(TARGET_ELF) --bin $(TARGET_BIN) \
		--name "$(HB_NAME)" --version "$(CORE_VERSION)" \
		--cover $(COVER_JPG) \
		--out $(PACKED_BIN)

# the SD card's files: build/sd/homebrews/SecondReality.bin + .srd
$(SD_DIR)/$(PACKED_BIN): $(PACKED_BIN)
	$(V)mkdir -p $(SD_DIR)
	$(V)cp $(PACKED_BIN) $@
	$(V)$(ECHO) "[ SD ] $(SD_DIR)/$(PACKED_BIN) + SecondReality.srd"

pack: $(PACK_SRD)

all: $(SD_DIR)/$(PACKED_BIN) $(PACK_SRD)

clean::
	$(V)rm -rf build/sd $(GEN) $(PACKED_BIN)

.PHONY: print-PROJECT_KIND print-PACKED_BIN print-CORE_NAME print-TARGET_ELF print-TARGET_MAP print-CORE_VERSION
print-PROJECT_KIND:
	@echo $(PROJECT_KIND)
print-PACKED_BIN:
	@echo $(PACKED_BIN)
print-CORE_NAME:
	@echo $(CORE_NAME)
print-TARGET_ELF:
	@echo $(TARGET_ELF)
print-TARGET_MAP:
	@echo $(BUILD_DIR)/$(CORE_NAME)_core.map
print-CORE_VERSION:
	@echo $(CORE_VERSION)

#######################################
# PC build
#######################################
include host/Makefile.host

#######################################
# test: the pictures of the PC build against Second Reality++'s
#######################################
.PHONY: check
check: host
	$(V)python3 tests/check.py $(HOST_BIN) $(PACK_SRD)
