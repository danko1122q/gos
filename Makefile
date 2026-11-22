.DEFAULT_GOAL := all
ARCH   ?= x86
BUILD  ?= $(if $(filter arm,$(ARCH)),build/arm,build)

ifeq ($(ARCH),x86)
CC      := gcc
AS      := as
LD      := ld
AR      := ar
OBJCOPY := objcopy
STRIP   := strip
NM      := nm
QEMU    ?= qemu-system-i386
IMG     ?= os.img
ARCHFLAGS := -m32 -mgeneral-regs-only -fno-tree-loop-distribute-patterns
ASFLAGS := --32
LDEMU   := -m elf_i386
KLDS    := boot/x86/linker.ld
ENTRY_SRC := boot/x86/entry.s
ASM     = $(AS) $(ASFLAGS) -MD $(@:.o=.d) $< -o $@
ASM_NODEP = $(AS) $(ASFLAGS) $< -o $@
UASM    = $(AS) $(ASFLAGS) $< -o $@
QFLAGS  := -drive format=raw,file=$(IMG) -vga std -serial stdio
else ifeq ($(ARCH),arm)
TOOLCHAIN ?= $(if $(shell command -v arm-none-eabi-gcc 2>/dev/null),gnu,llvm)
ifeq ($(TOOLCHAIN),gnu)
CC      := arm-none-eabi-gcc
LD      := arm-none-eabi-ld
AR      := arm-none-eabi-ar
OBJCOPY := arm-none-eabi-objcopy
STRIP   := arm-none-eabi-strip
NM      := arm-none-eabi-nm
else
CC      := clang --target=arm-none-eabi
LD      := ld.lld
AR      := llvm-ar
OBJCOPY := llvm-objcopy
STRIP   := llvm-strip
NM      := llvm-nm
endif
QEMU    ?= qemu-system-arm
IMG     ?= disk.img
ARCHFLAGS := -marm -mcpu=arm926ej-s -mfloat-abi=soft -mno-unaligned-access -fsigned-char -fno-short-enums \
             -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-pie
ASMFLAGS := $(ARCHFLAGS) -DGOS_ARM -Ikernel/arch/arm/include -Iabi -Ikernel
LDEMU   := -m armelf
KLDS    := boot/arm/linker.ld
ENTRY_SRC := boot/arm/entry.S
ASM     = $(CC) $(ASMFLAGS) -MMD -MP -c $< -o $@
ASM_NODEP = $(CC) $(ASMFLAGS) -c $< -o $@
UASM    = $(CC) $(ASMFLAGS) -c $< -o $@
QMEM    ?= 128
QFLAGS  := -M versatilepb -cpu arm926 -m $(QMEM) -kernel $(BUILD)/kernel.bin -drive file=$(IMG),if=sd,format=raw -serial stdio -no-reboot
else
$(error ARCH must be x86 or arm)
endif

COMMON := $(ARCHFLAGS) -D$(if $(filter arm,$(ARCH)),GOS_ARM,GOS_X86) -ffreestanding -fno-pic -fno-stack-protector -fno-builtin \
          -Wall -Werror -nostdlib -MMD -MP
ifeq ($(ARCH),x86)
COMMON += -fno-pie
endif
AINC    := -Ikernel/arch/$(ARCH)/include
CFLAGS  := $(COMMON) -O2 -Ikernel $(AINC) -Iabi
UCFLAGS := $(COMMON) -Os -ffunction-sections -fdata-sections -DGOS_USERLAND -Iuser/include -Iabi -Ikernel $(AINC)

ifeq ($(BENCH),1)
CFLAGS        += -DGOS_MEMBENCH -DGOS_STACKMARK
endif

USER_PROGS    := terminal files editor sysmon ticker counter burn upper \
                 calc puzzle keycaps clipview notepad scrapbook alarm getinfo control paint
terminal_SRCS := user/apps/terminal.c user/apps/shell.c user/apps/cmds.c
editor_SRCS   := user/apps/editor.c user/apps/edbuf.c
calc_SRCS     := user/apps/calc.c user/apps/calc_engine.c
SHARED := lib/string.c lib/printf.c lib/buf.c lib/timeconv.c gfx/draw.c gfx/font.c gfx/icons.c gui/widget.c
ifeq ($(ARCH),arm)
SHARED += arch/arm/aeabi.c
endif
LIB_C  := $(wildcard user/lib/*.c)
LIB_OBJS := $(LIB_C:%.c=$(BUILD)/%.o) $(SHARED:%.c=$(BUILD)/ushared/%.o)
LIBGOS := $(BUILD)/user/libgos.a
CRT0_SRC := $(firstword $(wildcard user/lib/$(ARCH)/crt0.s user/lib/$(ARCH)/crt0.S))
CRT0   := $(BUILD)/user/crt0.o

prog_srcs = $(or $($(1)_SRCS),user/apps/$(1).c)
prog_objs = $(patsubst %.c,$(BUILD)/%.o,$(call prog_srcs,$(1)))
USER_BINS := $(USER_PROGS:%=$(BUILD)/user/bin/%)

$(BUILD)/user/%.o: user/%.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@
$(BUILD)/ushared/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@
$(CRT0): $(CRT0_SRC)
	@mkdir -p $(dir $@)
	$(UASM)
$(LIBGOS): $(LIB_OBJS)
	@rm -f $@; $(AR) rcs $@ $^

define PROG_RULE
$(BUILD)/user/elf/$(1): $(CRT0) $(call prog_objs,$(1)) $(LIBGOS) user/user.ld
	@mkdir -p $$(dir $$@)
	$(LD) $(LDEMU) -z max-page-size=4096 --gc-sections -T user/user.ld -o $$@ $(CRT0) $(call prog_objs,$(1)) $(LIBGOS)
$(BUILD)/user/bin/$(1): $(BUILD)/user/elf/$(1)
	@mkdir -p $$(dir $$@)
	$(STRIP) -s -o $$@ $$<
endef
$(foreach p,$(USER_PROGS),$(eval $(call PROG_RULE,$(p))))

$(BUILD)/initrd.s: $(USER_BINS) tools/mkinitrd.sh
	sh tools/mkinitrd.sh $(ARCH) $@ $(USER_BINS)

ENTRY_OBJ := $(BUILD)/$(basename $(ENTRY_SRC)).o
C_SRCS    := $(shell find kernel -name '*.c' -not -path 'kernel/arch/*' -not -path 'kernel/drivers/x86/*' -not -path 'kernel/drivers/arm/*') \
             $(shell find kernel/arch/$(ARCH) kernel/drivers/$(ARCH) -name '*.c')
ASM_SRCS  := $(shell find kernel/arch/$(ARCH) \( -name '*.s' -o -name '*.S' \))
OBJS      := $(ENTRY_OBJ) $(BUILD)/initrd.o $(C_SRCS:%.c=$(BUILD)/%.o) $(patsubst %.S,$(BUILD)/%.o,$(ASM_SRCS:%.s=$(BUILD)/%.o))
DEPS      := $(shell find $(BUILD) -name '*.d' 2>/dev/null)

.PHONY: all run debug clean size membench fonts
ifeq ($(ARCH),x86)
all: $(IMG)
else
all: $(BUILD)/kernel.bin $(IMG)
endif

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(ASM)
$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(ASM)
$(BUILD)/initrd.o: $(BUILD)/initrd.s
	$(ASM_NODEP)

$(BUILD)/kernel.elf: $(OBJS) $(KLDS)
	$(LD) $(LDEMU) -T $(KLDS) -o $@ $(OBJS)

$(BUILD)/kernel.bin: $(BUILD)/kernel.elf
	$(OBJCOPY) -O binary $< $@

ifeq ($(ARCH),x86)
$(IMG): boot/x86/boot.s $(BUILD)/kernel.bin
	$(AS) $(ASFLAGS) --defsym KSECT=$$(( ($$(stat -c%s $(BUILD)/kernel.bin)+511)/512 )) $(if $(NO_E820),--defsym NO_E820=1) boot/x86/boot.s -o $(BUILD)/boot.o
	$(LD) $(LDEMU) -e start -Ttext 0x7C00 --oformat binary -o $(BUILD)/boot.bin $(BUILD)/boot.o
	cat $(BUILD)/boot.bin $(BUILD)/kernel.bin > $@
	truncate -s 8M $@
else
$(IMG):
	truncate -s 8M $@
endif

fonts:
	python3 tools/mkfont.py

size: $(BUILD)/kernel.bin $(USER_BINS)
	@echo "Userland:"
	@du -h $(BUILD)/user/bin/* | sort -h
	@echo "Kernel image: $$(stat -c%s $(BUILD)/kernel.bin) byte"
	@used=$$($(NM) -n $(BUILD)/kernel.elf | awk '$$3 == "__bss_end" { print "0x" $$1 }'); \
	 test -n "$$used" || { echo "size: __bss_end symbol not found"; exit 1; }; \
	 bytes=$$(( $$used - 0x10000 )); limit=524288; \
	 if [ $$bytes -gt $$limit ]; then echo "Kernel+BSS: $$bytes / $$limit bytes (OVER LIMIT)"; exit 1; fi; \
	 echo "Kernel + BSS: $$bytes / $$limit bytes, $$((limit - bytes)) bytes left"

run: $(if $(filter x86,$(ARCH)),$(IMG),$(BUILD)/kernel.bin $(IMG))
	$(QEMU) $(QFLAGS)

ifeq ($(ARCH),x86)
debug: $(IMG)
	$(QEMU) $(QFLAGS) -s -S -no-reboot -d int,cpu_reset
else
debug: $(BUILD)/kernel.bin $(IMG)
	$(QEMU) $(QFLAGS) -s -S -no-reboot -d int,cpu_reset
endif

clean:
	rm -rf $(BUILD) os.img

membench:
	$(MAKE) ARCH=$(ARCH) BENCH=1 BUILD=build/bench IMG=build/bench/os.img build/bench/os.img

-include $(DEPS)

armemu: $(BUILD)/armemu
$(BUILD)/armemu: tools/armemu/armemu.c
	mkdir -p $(BUILD)
	cc -O2 -Wall -o $@ $<
emutest: $(BUILD)/armemu $(BUILD)/kernel.bin
	rm -f $(BUILD)/emu-disk.img; truncate -s 8M $(BUILD)/emu-disk.img
	$(BUILD)/armemu $(BUILD)/kernel.bin $(BUILD)/emu-disk.img -n 600 -mouse 20 -91 96 0 -mouse 21 -91 96 0 -mouse 22 0 0 1 -mouse 23 0 0 0 -mouse 24 0 0 1 -mouse 25 0 0 0 \
	  -shot 590 $(BUILD)/emu.ppm 2>&1 | tee $(BUILD)/emu.log | grep -E "init ok|video:|PANIC|emu:"
	grep -q "GOS: init ok" $(BUILD)/emu.log && grep -q "video: CLCD enabled" $(BUILD)/emu.log && ! grep -q PANIC $(BUILD)/emu.log
