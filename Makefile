CROSS   := $(HOME)/tools/cross/bin/i686-elf
CC      := $(CROSS)-gcc
AR      := $(CROSS)-ar
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
USER_GEMINI  := $(BUILD)/user/gemini.elf
USER_RSS     := $(BUILD)/user/rss.elf
USER_FINGER  := $(BUILD)/user/finger.elf
USER_IRC     := $(BUILD)/user/irc.elf
USER_NNTP    := $(BUILD)/user/nntp.elf
USER_TIME    := $(BUILD)/user/time.elf
USER_ARGV    := $(BUILD)/user/argv.elf
USER_FMT     := $(BUILD)/user/fmt.elf
USER_ALLOC   := $(BUILD)/user/alloc.elf
USER_STRESS  := $(BUILD)/user/stress.elf
SHELLREG_TXT := tests/shellreg.txt

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

# BearSSL — minimal TLS 1.2 client library (cross-compiled for i686-elf user space)
BEARSSL_SRC := docs/references/bearssl-0.6/src
BEARSSL_INC := docs/references/bearssl-0.6/inc
BEARSSL_LIB := $(BUILD)/user/libbearssl.a
BEARSSL_CFLAGS := -std=gnu99 -ffreestanding -O2 \
    -Iuser/bearssl/include -I$(BEARSSL_INC) -I$(BEARSSL_SRC) \
    -DBR_USE_URANDOM=0 -DBR_USE_WIN32_RAND=0 \
    -DBR_USE_UNIX_TIME=0 -DBR_AES_X86NI=0 -DBR_SSE2=0 -DBR_POWER8=0

BEARSSL_SRCS := \
    ssl/ssl_engine ssl/ssl_client ssl/ssl_hs_client \
    ssl/ssl_io ssl/ssl_lru ssl/ssl_hashes ssl/ssl_rec_gcm \
    ssl/ssl_engine_default_aesgcm \
    ssl/prf ssl/prf_sha256 ssl/prf_sha384 \
    symcipher/aes_ct symcipher/aes_ct_enc symcipher/aes_ct_ctr symcipher/aes_common \
    hash/sha2small hash/sha2big hash/sha1 hash/multihash hash/ghash_ctmul \
    mac/hmac rand/hmac_drbg \
    int/i31_add int/i31_bitlen int/i31_decmod int/i31_decode int/i31_decred \
    int/i31_encode int/i31_fmont int/i31_iszero int/i31_moddiv int/i31_modpow \
    int/i31_modpow2 int/i31_montmul int/i31_mulacc int/i31_muladd int/i31_ninv31 \
    int/i31_reduce int/i31_rshift int/i31_sub int/i31_tmont \
    int/i32_div32 \
    codec/ccopy codec/dec32be codec/enc32be codec/dec64be codec/enc64be \
    rsa/rsa_i31_pkcs1_vrfy rsa/rsa_i31_pub rsa/rsa_pkcs1_sig_unpad \
    ec/ec_prime_i31 ec/ec_secp256r1 ec/ec_secp384r1 ec/ec_secp521r1 \
    ec/ec_default ec/ec_pubkey ec/ec_keygen \
    ec/ecdsa_i31_vrfy_asn1 ec/ecdsa_i31_vrfy_raw ec/ecdsa_i31_bits ec/ecdsa_atr \
    x509/x509_minimal

BEARSSL_OBJS := $(patsubst %, $(BUILD)/bearssl/%.o, $(BEARSSL_SRCS))
BEARSSL_OBJS += $(BUILD)/bearssl/sysrng_stub.o

$(BUILD)/bearssl/%.o: $(BEARSSL_SRC)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(BEARSSL_CFLAGS) -c $< -o $@

$(BUILD)/bearssl/sysrng_stub.o: user/bearssl/sysrng_stub.c
	@mkdir -p $(dir $@)
	$(CC) $(BEARSSL_CFLAGS) -c $< -o $@

$(BEARSSL_LIB): $(BEARSSL_OBJS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

MKDISK := tools/mkdisk

.PHONY: all clean run debug disk smoke-net smoke-rss smoke-finger smoke-irc smoke-nntp smoke-shell

all: $(KERNEL) $(USER_ELF) $(USER_SH) $(USER_CAT) $(USER_TUIDEMO) $(USER_FM) $(USER_ED) $(USER_PKG) $(USER_GOPHER) $(USER_GEMINI) $(USER_RSS) $(USER_FINGER) $(USER_IRC) $(USER_NNTP) $(USER_TIME) $(USER_ARGV) $(USER_FMT) $(USER_ALLOC) $(USER_STRESS) $(PACK)

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

$(BUILD)/user/gemini_main.o: user/gemini.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -Iuser/bearssl/include -I$(BEARSSL_INC) -c $< -o $@

$(BUILD)/user/rss_main.o: user/rss.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(BUILD)/user/finger_main.o: user/finger.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(BUILD)/user/irc_main.o: user/irc.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(BUILD)/user/nntp_main.o: user/nntp.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(BUILD)/user/time_main.o: user/time.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(USER_TIME): $(LIBC_OBJS) $(BUILD)/user/time_main.o user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/time_main.o $(LIBC_C_OBJS)

$(BUILD)/user/argv_main.o: user/argv.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(USER_ARGV): $(LIBC_OBJS) $(BUILD)/user/argv_main.o user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/argv_main.o $(LIBC_C_OBJS)

$(BUILD)/user/fmt_main.o: user/fmt.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(USER_FMT): $(LIBC_OBJS) $(BUILD)/user/fmt_main.o user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/fmt_main.o $(LIBC_C_OBJS)

$(BUILD)/user/alloc_main.o: user/alloc.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(USER_ALLOC): $(LIBC_OBJS) $(BUILD)/user/alloc_main.o user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/alloc_main.o $(LIBC_C_OBJS)

$(BUILD)/user/stress_main.o: user/stress.c
	@mkdir -p $(dir $@)
	$(CC) $(UCFLAGS) -c $< -o $@

$(USER_STRESS): $(LIBC_OBJS) $(BUILD)/user/stress_main.o user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/stress_main.o $(LIBC_C_OBJS)

$(USER_GEMINI): $(LIBC_OBJS) $(BUILD)/user/gemini_main.o $(BEARSSL_LIB) user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/gemini_main.o $(LIBC_C_OBJS) $(BEARSSL_LIB)

$(USER_RSS): $(LIBC_OBJS) $(BUILD)/user/rss_main.o user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/rss_main.o $(LIBC_C_OBJS)

$(USER_FINGER): $(LIBC_OBJS) $(BUILD)/user/finger_main.o user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/finger_main.o $(LIBC_C_OBJS)

$(USER_IRC): $(LIBC_OBJS) $(BUILD)/user/irc_main.o user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/irc_main.o $(LIBC_C_OBJS)

$(USER_NNTP): $(LIBC_OBJS) $(BUILD)/user/nntp_main.o user/user.ld
	$(LD) -T user/user.ld -o $@ \
	    $(LIBC_CRT0) $(BUILD)/user/nntp_main.o $(LIBC_C_OBJS)

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

disk: $(MKDISK) $(USER_ELF) $(USER_SH) $(USER_CAT) $(USER_TUIDEMO) $(USER_FM) $(USER_ED) $(USER_PKG) $(USER_GOPHER) $(USER_GEMINI) $(USER_RSS) $(USER_FINGER) $(USER_IRC) $(USER_NNTP) $(USER_TIME) $(USER_ARGV) $(USER_FMT) $(USER_ALLOC) $(USER_STRESS) $(HELLO_D9P)
	@[ -f disk.img ] || { echo "error: disk.img not found — run: dd if=/dev/zero of=disk.img bs=512 count=16384" >&2; exit 1; }
	./$(MKDISK) disk.img hello=$(USER_ELF) sh=$(USER_SH) cat=$(USER_CAT) \
	    tuidemo=$(USER_TUIDEMO) fm=$(USER_FM) ed=$(USER_ED) pkg=$(USER_PKG) \
	    gopher=$(USER_GOPHER) gemini=$(USER_GEMINI) rss=$(USER_RSS) finger=$(USER_FINGER) irc=$(USER_IRC) nntp=$(USER_NNTP) time=$(USER_TIME) \
	    argv=$(USER_ARGV) fmt=$(USER_FMT) alloc=$(USER_ALLOC) stress=$(USER_STRESS) \
	    hello.d9p=$(HELLO_D9P) shellreg=$(SHELLREG_TXT)

run: $(KERNEL) $(USER_ELF) $(USER_SH) $(USER_CAT) $(USER_TUIDEMO) $(USER_FM) $(USER_ED) $(USER_PKG) $(USER_GOPHER) $(USER_GEMINI) $(USER_RSS) $(USER_FINGER) $(USER_IRC) $(USER_NNTP) $(USER_TIME) $(USER_ARGV) $(USER_FMT) $(USER_ALLOC) $(USER_STRESS) $(HELLO_D9P)
	./scripts/qemu.sh

debug: $(KERNEL) $(USER_ELF) $(USER_SH) $(USER_CAT) $(USER_TUIDEMO) $(USER_FM) $(USER_ED) $(USER_PKG) $(USER_GOPHER) $(USER_GEMINI) $(USER_RSS) $(USER_FINGER) $(USER_IRC) $(USER_NNTP) $(USER_TIME) $(USER_ARGV) $(USER_FMT) $(USER_ALLOC) $(USER_STRESS) $(HELLO_D9P)
	./scripts/qemu.sh --debug

smoke-net: $(KERNEL) $(USER_ELF) $(USER_SH) $(USER_CAT) $(USER_TUIDEMO) $(USER_FM) $(USER_ED) $(USER_PKG) $(USER_GOPHER) $(USER_GEMINI) $(USER_RSS) $(USER_FINGER) $(USER_IRC) $(USER_NNTP) $(USER_TIME) $(USER_ARGV) $(USER_FMT) $(USER_ALLOC) $(USER_STRESS) $(HELLO_D9P)
	bash ./scripts/smoke-net.sh

smoke-rss: disk $(KERNEL) $(USER_ELF) $(USER_SH) $(USER_CAT) $(USER_TUIDEMO) $(USER_FM) $(USER_ED) $(USER_PKG) $(USER_GOPHER) $(USER_GEMINI) $(USER_RSS) $(USER_FINGER) $(USER_IRC) $(USER_NNTP) $(USER_TIME) $(USER_ARGV) $(USER_FMT) $(USER_ALLOC) $(USER_STRESS) $(HELLO_D9P)
	bash ./scripts/smoke-rss.sh

smoke-finger: disk $(KERNEL) $(USER_ELF) $(USER_SH) $(USER_CAT) $(USER_TUIDEMO) $(USER_FM) $(USER_ED) $(USER_PKG) $(USER_GOPHER) $(USER_GEMINI) $(USER_RSS) $(USER_FINGER) $(USER_IRC) $(USER_NNTP) $(USER_TIME) $(USER_ARGV) $(USER_FMT) $(USER_ALLOC) $(USER_STRESS) $(HELLO_D9P)
	bash ./scripts/smoke-finger.sh

smoke-irc: disk $(KERNEL) $(USER_ELF) $(USER_SH) $(USER_CAT) $(USER_TUIDEMO) $(USER_FM) $(USER_ED) $(USER_PKG) $(USER_GOPHER) $(USER_GEMINI) $(USER_RSS) $(USER_FINGER) $(USER_IRC) $(USER_NNTP) $(USER_TIME) $(USER_ARGV) $(USER_FMT) $(USER_ALLOC) $(USER_STRESS) $(HELLO_D9P)
	bash ./scripts/smoke-irc.sh

smoke-nntp: disk $(KERNEL) $(USER_ELF) $(USER_SH) $(USER_CAT) $(USER_TUIDEMO) $(USER_FM) $(USER_ED) $(USER_PKG) $(USER_GOPHER) $(USER_GEMINI) $(USER_RSS) $(USER_FINGER) $(USER_IRC) $(USER_NNTP) $(USER_TIME) $(USER_ARGV) $(USER_FMT) $(USER_ALLOC) $(USER_STRESS) $(HELLO_D9P)
	bash ./scripts/smoke-nntp.sh

smoke-shell: $(KERNEL) $(USER_ELF) $(USER_SH) $(USER_CAT) $(USER_TUIDEMO) $(USER_FM) $(USER_ED) $(USER_PKG) $(USER_GOPHER) $(USER_GEMINI) $(USER_RSS) $(USER_FINGER) $(USER_IRC) $(USER_NNTP) $(USER_TIME) $(USER_ARGV) $(USER_FMT) $(USER_ALLOC) $(USER_STRESS) $(HELLO_D9P) $(SHELLREG_TXT)
	bash ./scripts/shell-smoke.sh

clean:
	rm -rf $(BUILD)
	rm -f $(MKDISK) $(PACK)
