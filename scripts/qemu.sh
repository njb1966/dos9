#!/usr/bin/env bash
set -e

KERNEL="build/kernel.elf"
USER_ELF="build/user/hello.elf"
DISK_IMG="disk.img"

if [ ! -f "$KERNEL" ]; then
    echo "error: $KERNEL not found — run 'make' first" >&2
    exit 1
fi

INITRD_ARG=""
[ -f "$USER_ELF" ] && INITRD_ARG="-initrd $USER_ELF"

DISK_ARG=""
[ -f "$DISK_IMG" ] && DISK_ARG="-drive file=$DISK_IMG,format=raw,if=ide,index=0,media=disk"

if [ "$1" = "--debug" ]; then
    exec qemu-system-i386 \
        -kernel "$KERNEL" \
        $INITRD_ARG \
        $DISK_ARG \
        -serial stdio \
        -s -S
else
    exec qemu-system-i386 \
        -kernel "$KERNEL" \
        $INITRD_ARG \
        $DISK_ARG \
        -serial stdio
fi
