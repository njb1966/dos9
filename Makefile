CROSS   := $(HOME)/tools/cross/bin/i686-elf
CC      := $(CROSS)-gcc
LD      := $(CROSS)-ld
OBJDUMP := $(CROSS)-objdump

CFLAGS  := -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Ikernel/include
LDFLAGS := -T kernel/arch/i386/linker.ld -ffreestanding -O2 -nostdlib -lgcc

BUILD    := build
KERNEL   := $(BUILD)/kernel.elf
USER_ELF := $(BUILD)/user/hello.elf

# Kernel sources
C_SRCS  := $(shell find kernel -name '*.c')
S_SRCS  := boot/boot.S $(shell find kernel -name '*.S')

C_OBJS  := $(patsubst %.c, $(BUILD)/%.o, $(C_SRCS))
S_OBJS  := $(patsubst %.S, $(BUILD)/%.o, $(S_SRCS))
OBJS    := $(C_OBJS) $(S_OBJS)

# User-space libc
LIBC_DIR  := user/libc
LIBC_INC  := $(LIBC_DIR)/include
UCFLAGS   := -std=gnu99 -ffreestanding -O2 -Wall -Wextra -I$(LIBC_INC)

LIBC_C_SRCS := $(wildcard $(LIBC_DIR)/*.c)
LIBC_C_OBJS := $(patsubst $(LIBC_DIR)/%.c, $(BUILD)/user/libc/%.o, $(LIBC_C_SRCS))
LIBC_CRT0   := $(BUILD)/user/libc/crt0.o
LIBC_OBJS   := $(LIBC_CRT0) $(LIBC_C_OBJS)

.PHONY: all clean run debug

all: $(KERNEL) $(USER_ELF)

# ── User-space (libc + programs) — defined before generic kernel rules ────

$(LIBC_CRT0): $(LIBC_DIR)/crt0.S
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(BUILD)/user/libc/%.o: $(LIBC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(BUILD)/user/hello_main.o: user/hello.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(USER_ELF): $(LIBC_OBJS) $(BUILD)/user/hello_main.o user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/hello_main.o $(LIBC_C_OBJS)

# ── Kernel ────────────────────────────────────────────────────────────────

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

run: $(KERNEL) $(USER_ELF)
	./scripts/qemu.sh

debug: $(KERNEL) $(USER_ELF)
	./scripts/qemu.sh --debug

clean:
	rm -rf $(BUILD)
