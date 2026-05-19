CROSS   := $(HOME)/tools/cross/bin/i686-elf
CC      := $(CROSS)-gcc
LD      := $(CROSS)-ld
OBJDUMP := $(CROSS)-objdump

CFLAGS  := -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Ikernel/include
LDFLAGS := -T kernel/arch/i386/linker.ld -ffreestanding -O2 -nostdlib -lgcc

BUILD    := build
KERNEL   := $(BUILD)/kernel.elf
USER_ELF := $(BUILD)/user/hello.elf

# Collect all C and S sources under boot/ and kernel/
C_SRCS  := $(shell find kernel -name '*.c')
S_SRCS  := boot/boot.S $(shell find kernel -name '*.S')

C_OBJS  := $(patsubst %.c, $(BUILD)/%.o, $(C_SRCS))
S_OBJS  := $(patsubst %.S, $(BUILD)/%.o, $(S_SRCS))
OBJS    := $(C_OBJS) $(S_OBJS)

.PHONY: all clean run debug

all: $(KERNEL) $(USER_ELF)

# Pattern rules — mirror source tree under build/
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# User-space binary — plain ELF, no kernel headers, linked at 0x400000
$(BUILD)/user/hello.o: user/hello.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(USER_ELF): $(BUILD)/user/hello.o user/user.ld
	$(LD) -T user/user.ld -o $@ $<

run: $(KERNEL) $(USER_ELF)
	./scripts/qemu.sh

debug: $(KERNEL) $(USER_ELF)
	./scripts/qemu.sh --debug

clean:
	rm -rf $(BUILD)
