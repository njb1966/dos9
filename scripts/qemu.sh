#!/usr/bin/env bash
set -e

KERNEL="build/kernel.elf"
USER_ELF="build/user/hello.elf"
DISK_IMG="disk.img"
NET_MODE="user"
DEBUG_MODE=0
APPEND_ARG=""

for arg in "$@"; do
    case "$arg" in
        user|bridge|guestfwd)
            NET_MODE="$arg"
            ;;
        --debug)
            DEBUG_MODE=1
            ;;
        *)
            echo "usage: $0 [user|bridge|guestfwd] [--debug]" >&2
            exit 1
            ;;
    esac
done

if [ ! -f "$KERNEL" ]; then
    echo "error: $KERNEL not found — run 'make' first" >&2
    exit 1
fi

INITRD_ARG=""
[ -f "$USER_ELF" ] && INITRD_ARG="-initrd $USER_ELF"

DISK_ARG=""
[ -f "$DISK_IMG" ] && DISK_ARG="-drive file=$DISK_IMG,format=raw,if=ide,index=0,media=disk,file.locking=off"

case "$NET_MODE" in
    user)
        NET_ARG="-netdev user,id=n0 -device rtl8139,netdev=n0"
        APPEND_ARG="-append netmode=user"
        ;;
    bridge)
        NET_ARG="-netdev bridge,id=n0,br=br0 -device rtl8139,netdev=n0"
        APPEND_ARG="-append netmode=bridge"
        ;;
    guestfwd)
        NET_ARG="-netdev user,id=n0,guestfwd=tcp:10.0.2.100:1234-tcp:127.0.0.1:1234 -device rtl8139,netdev=n0"
        APPEND_ARG="-append netmode=user"
        ;;
    *)
        echo "usage: $0 [user|bridge|guestfwd] [--debug]" >&2
        exit 1
        ;;
esac

if [ "$DEBUG_MODE" -eq 1 ]; then
    exec qemu-system-i386 \
        -nographic \
        -kernel "$KERNEL" \
        $INITRD_ARG \
        $DISK_ARG \
        $APPEND_ARG \
        $NET_ARG \
        -s -S
else
    exec qemu-system-i386 \
        -nographic \
        -kernel "$KERNEL" \
        $INITRD_ARG \
        $DISK_ARG \
        $APPEND_ARG \
        $NET_ARG
fi
