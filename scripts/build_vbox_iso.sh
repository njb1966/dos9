#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_DIR="$ROOT_DIR/build/vbox"
ISO_ROOT="$OUT_DIR/iso"
GRUB_DIR="$ISO_ROOT/boot/grub"
ISO_PATH="$OUT_DIR/dos9-control.iso"

if [ ! -f "$ROOT_DIR/build/kernel.elf" ] || [ ! -f "$ROOT_DIR/build/user/hello.elf" ]; then
    echo "error: build artifacts missing; run 'make -j2 all' first" >&2
    exit 1
fi

rm -rf "$ISO_ROOT"
mkdir -p "$GRUB_DIR"

cp "$ROOT_DIR/build/kernel.elf" "$ISO_ROOT/boot/kernel.elf"
cp "$ROOT_DIR/build/user/hello.elf" "$ISO_ROOT/boot/hello.elf"

cat > "$GRUB_DIR/grub.cfg" <<'EOF'
set timeout=0
set default=0

menuentry "DOS/9 control" {
    multiboot /boot/kernel.elf
    module /boot/hello.elf
    boot
}
EOF

grub-mkrescue -o "$ISO_PATH" "$ISO_ROOT" >/dev/null
echo "$ISO_PATH"
