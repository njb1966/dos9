#!/usr/bin/env bash
set -e

KERNEL="build/kernel.elf"
USER_ELF="build/user/hello.elf"

if [ ! -f "$KERNEL" ]; then
    echo "error: $KERNEL not found — run 'make' first" >&2
    exit 1
fi

INITRD_ARG=""
[ -f "$USER_ELF" ] && INITRD_ARG="-initrd $USER_ELF"

if [ "$1" = "--debug" ]; then
    exec qemu-system-i386 \
        -kernel "$KERNEL" \
        $INITRD_ARG \
        -serial stdio \
        -s -S
else
    exec qemu-system-i386 \
        -kernel "$KERNEL" \
        $INITRD_ARG \
        -serial stdio
fi
