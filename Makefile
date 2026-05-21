CROSS   := $(HOME)/tools/cross/bin/i686-elf
CC      := $(CROSS)-gcc
LD      := $(CROSS)-ld
OBJDUMP := $(CROSS)-objdump

CFLAGS  := -std=gnu99 -ffreestanding -O2 -Wall -Wextra -Ikernel/include
LDFLAGS := -T kernel/arch/i386/linker.ld -ffreestanding -O2 -nostdlib -lgcc

BUILD      := build
KERNEL     := $(BUILD)/kernel.elf
USER_ELF     := $(BUILD)/user/hello.elf
USER_SH      := $(BUILD)/user/sh.elf
USER_CAT     := $(BUILD)/user/cat.elf
USER_TUIDEMO := $(BUILD)/user/tuidemo.elf
USER_FM      := $(BUILD)/user/fm.elf
USER_ED      := $(BUILD)/user/ed.elf
USER_PKG     := $(BUILD)/user/pkg.elf
USER_GOPHER  := $(BUILD)/user/gopher.elf

# Host tools (native gcc — not cross-compiled)
PACK  := tools/pack

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

MKDISK := tools/mkdisk

.PHONY: all clean run debug disk

all: $(KERNEL) $(USER_ELF) $(USER_SH) $(USER_CAT) $(USER_TUIDEMO) $(USER_FM) $(USER_ED) $(USER_PKG) $(USER_GOPHER) $(PACK)

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

$(BUILD)/user/sh_main.o: user/sh.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(USER_SH): $(LIBC_OBJS) $(BUILD)/user/sh_main.o user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/sh_main.o $(LIBC_C_OBJS)

$(BUILD)/user/cat_main.o: user/cat.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(USER_CAT): $(LIBC_OBJS) $(BUILD)/user/cat_main.o user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/cat_main.o $(LIBC_C_OBJS)

$(BUILD)/user/tuidemo_main.o: user/tuidemo.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(USER_TUIDEMO): $(LIBC_OBJS) $(BUILD)/user/tuidemo_main.o user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/tuidemo_main.o $(LIBC_C_OBJS)

$(BUILD)/user/fm_main.o: user/fm.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(USER_FM): $(LIBC_OBJS) $(BUILD)/user/fm_main.o user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/fm_main.o $(LIBC_C_OBJS)

$(BUILD)/user/ed_main.o: user/ed.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(USER_ED): $(LIBC_OBJS) $(BUILD)/user/ed_main.o user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/ed_main.o $(LIBC_C_OBJS)

$(BUILD)/user/pkg_main.o: user/pkg.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(USER_PKG): $(LIBC_OBJS) $(BUILD)/user/pkg_main.o user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/pkg_main.o $(LIBC_C_OBJS)

$(BUILD)/user/gopher_main.o: user/gopher.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(USER_GOPHER): $(LIBC_OBJS) $(BUILD)/user/gopher_main.o user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/gopher_main.o $(LIBC_C_OBJS)

# ── Kernel ────────────────────────────────────────────────────────────────

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# ── Host tools ────────────────────────────────────────────────────────────

$(MKDISK): tools/mkdisk.c
	gcc -O2 -Wall -Wextra -o $@ $<

$(PACK): tools/pack.c
	gcc -O2 -Wall -Wextra -o $@ $<

# Demo package: hello.d9p ships on disk so pkg can be tested from the shell.
HELLO_D9P := $(BUILD)/user/hello.d9p

$(HELLO_D9P): $(PACK) $(USER_ELF)
	$(PACK) hello $(USER_ELF) $@

# ── Disk image ────────────────────────────────────────────────────────────
# Writes DOS9FS to disk.img with the user hello ELF as "hello".
# Run once after initial 'dd' setup, then again whenever user programs change.

disk: $(MKDISK) $(USER_ELF) $(USER_SH) $(USER_CAT) $(USER_TUIDEMO) $(USER_FM) $(USER_ED) $(USER_PKG) $(USER_GOPHER) $(HELLO_D9P)
	@[ -f disk.img ] || { echo "error: disk.img not found — run: dd if=/dev/zero of=disk.img bs=512 count=16384" >&2; exit 1; }
	./$(MKDISK) disk.img hello=$(USER_ELF) sh=$(USER_SH) cat=$(USER_CAT) \
	    tuidemo=$(USER_TUIDEMO) fm=$(USER_FM) ed=$(USER_ED) pkg=$(USER_PKG) \
	    gopher=$(USER_GOPHER) hello.d9p=$(HELLO_D9P)

run: $(KERNEL) $(USER_ELF) $(USER_SH) $(USER_CAT) $(USER_TUIDEMO) $(USER_FM) $(USER_ED) $(USER_PKG) $(USER_GOPHER) $(HELLO_D9P)
	./scripts/qemu.sh

debug: $(KERNEL) $(USER_ELF) $(USER_SH) $(USER_CAT) $(USER_TUIDEMO) $(USER_FM) $(USER_ED) $(USER_PKG) $(USER_GOPHER) $(HELLO_D9P)
	./scripts/qemu.sh --debug

clean:
	rm -rf $(BUILD)
	rm -f $(MKDISK)
