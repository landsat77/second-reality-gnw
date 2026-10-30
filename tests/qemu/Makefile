# The demo on QEMU's Cortex-M7 (mps2-an500), with the PC build's stand-ins
# for the firmware (host/*.c) and no SDL: the console's code (same compiler
# and flags) to find crashes and to estimate the CPU time per part.
#
#   make -C tests/qemu            build sr_qemu.elf
#   make -C tests/qemu run        run it (qemu_env.txt: SR_* variables, see
#                                 host/host_sr.h; SR_CPU=1 for the CPU time)
#
# -icount shift=2: an instruction takes 4 ns (250 MIPS; the console runs at
# 280 MHz).

ROOT := ../..
GEN := $(ROOT)/build/gen
SDK := $(ROOT)/sdk/include
CC := arm-none-eabi-gcc
CXX := arm-none-eabi-g++
MCU := -mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard -mno-unaligned-access
DEFS := -DHOST_BUILD=1 -DSR_QEMU=1 -DPROJECT_KIND_HOMEBREW=1 -DSR_GNW=1 -DSR_AUDIO_RATE=44100 \
	-DSR_VERSION=\"qemu\" -DIS_LITTLE_ENDIAN -DGNW_DISABLE_COMPRESSION -DHOST_SCALE=1
INC := -I$(ROOT)/host -I$(ROOT)/host/stubs -I$(ROOT)/sdk/src -I$(SDK)/Core/Inc -I$(SDK)/Core/Inc/retro-go \
	-I$(SDK)/Core/Inc/porting -I$(SDK)/Core/Src/porting/lib -I$(SDK)/Core/Src/porting/lib/FatFs \
	-I$(ROOT)/src/sr -I$(ROOT)/src/sr/Music -I$(ROOT)/src/port -I$(GEN)
CFLAGS := $(MCU) -O2 -g -ffunction-sections -fdata-sections $(DEFS) $(INC) -include $(ROOT)/host/stubs/config.h -w
CXXFLAGS := $(MCU) -O2 -g -ffunction-sections -fdata-sections $(DEFS) $(INC) -std=gnu++20 -w -fsigned-char \
	-fno-exceptions -fno-rtti -fno-threadsafe-statics

SR := $(wildcard $(ROOT)/src/sr/Parts/*/*.cpp) $(ROOT)/src/sr/Parts/Common.cpp $(ROOT)/src/sr/Graphics/shims.cpp \
	$(ROOT)/src/sr/Music/music.cpp $(ROOT)/src/sr/Music/st3play.cpp $(ROOT)/src/sr/Blob/Blob.cpp \
	$(ROOT)/src/port/sr_glue.cpp $(ROOT)/src/port/sr_pack.cpp $(GEN)/sr_ram_data.cpp $(GEN)/sr_hot.cpp
C := $(ROOT)/src/port/sr_main.c $(ROOT)/src/port/sr_overlay.c $(ROOT)/host/host_sr.c $(ROOT)/host/host_emu.c \
	$(ROOT)/host/host_i18n.c $(ROOT)/host/crc32.c $(ROOT)/host/host_main.c qemu_platform.c qemu_start.c

OBJ := $(addprefix obj/,$(notdir $(SR:.cpp=.o)) $(notdir $(C:.c=.o))) obj/sr_flash_refs.o
vpath %.cpp $(sort $(dir $(SR)))
vpath %.c $(sort $(dir $(C)))

sr_qemu.elf: $(OBJ) qemu.ld
	$(CXX) $(MCU) --specs=rdimon.specs -T qemu.ld -Wl,--gc-sections -o $@ $(OBJ) -lm
	arm-none-eabi-size $@

obj/%.o: %.cpp
	@mkdir -p obj
	$(CXX) $(CXXFLAGS) -c $< -o $@
obj/%.o: %.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@
obj/sr_flash_refs.o: $(GEN)/sr_flash_refs.cpp
	@mkdir -p obj
	$(CXX) $(CXXFLAGS) -c $< -o $@
	arm-none-eabi-objcopy --rename-section .init_array=sr_init_array $@

run: sr_qemu.elf
	qemu-system-arm -M mps2-an500 -nographic -monitor none -serial none -icount shift=2 \
		-semihosting-config enable=on,target=native -kernel sr_qemu.elf

clean:
	rm -rf obj sr_qemu.elf
