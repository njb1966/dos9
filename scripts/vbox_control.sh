#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VM_NAME="${VM_NAME:-DOS9-control}"
OSTYPE="${OSTYPE:-Debian_64}"
ISO_PATH="$(readlink -f "$ROOT_DIR/build/vbox/dos9-control.iso")"
DISK_IMG="$(readlink -f "$ROOT_DIR/disk.img")"
RAW_VMDK="$(readlink -f "$ROOT_DIR/build/vbox/disk.vmdk")"

if [ ! -f "$ISO_PATH" ]; then
    echo "error: missing $ISO_PATH; run scripts/build_vbox_iso.sh first" >&2
    exit 1
fi

if [ ! -f "$DISK_IMG" ]; then
    echo "error: missing $DISK_IMG" >&2
    exit 1
fi

mkdir -p "$ROOT_DIR/build/vbox"

if [ ! -f "$RAW_VMDK" ]; then
    VBoxManage convertfromraw "$DISK_IMG" "$RAW_VMDK" --format VMDK >/dev/null
fi

if ! VBoxManage showvminfo "$VM_NAME" >/dev/null 2>&1; then
    VBoxManage createvm --name "$VM_NAME" --ostype "$OSTYPE" --register >/dev/null
fi

VBoxManage modifyvm "$VM_NAME" \
    --ostype "$OSTYPE" \
    --memory 128 \
    --vram 16 \
    --firmware efi \
    --ioapic on \
    --boot1 dvd \
    --boot2 disk \
    --boot3 none \
    --boot4 none \
    --nic1 nat \
    --audio none \
    --usb off \
    --usbehci off >/dev/null

if ! VBoxManage showvminfo "$VM_NAME" --machinereadable 2>/dev/null | grep -q '^storagecontrollername0='; then
    VBoxManage storagectl "$VM_NAME" --name "IDE" --add ide >/dev/null
fi
if ! VBoxManage showvminfo "$VM_NAME" --machinereadable 2>/dev/null | grep -q '^storagecontrollername1='; then
    VBoxManage storagectl "$VM_NAME" --name "SATA" --add sata --controller IntelAhci >/dev/null
fi

# Detach any stale media first, then attach the known-good ISO/VMDK paths.
VBoxManage storageattach "$VM_NAME" --storagectl "IDE" --port 0 --device 0 --type dvddrive --medium none >/dev/null 2>&1 || true
VBoxManage storageattach "$VM_NAME" --storagectl "SATA" --port 0 --device 0 --type hdd --medium none >/dev/null 2>&1 || true
VBoxManage storageattach "$VM_NAME" --storagectl "SATA" --port 1 --device 0 --type dvddrive --medium none >/dev/null 2>&1 || true
VBoxManage storageattach "$VM_NAME" --storagectl "SATA" --port 0 --device 0 --type hdd --medium "$RAW_VMDK" >/dev/null 2>&1 || true
VBoxManage storageattach "$VM_NAME" --storagectl "SATA" --port 1 --device 0 --type dvddrive --medium "$ISO_PATH" >/dev/null 2>&1 || true

if [ "${1:-}" = "--start" ]; then
    VBoxManage startvm "$VM_NAME" --type headless
else
    VBoxManage showvminfo "$VM_NAME"
    echo
    echo "To start: scripts/vbox_control.sh --start"
fi
