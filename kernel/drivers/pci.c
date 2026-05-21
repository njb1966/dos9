#include <pci.h>
#include <io.h>
#include <stdint.h>

#define PCI_ADDR 0xCF8
#define PCI_DATA 0xCFC

static uint32_t pci_addr(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off) {
    return 0x80000000u
         | ((uint32_t)bus  << 16)
         | ((uint32_t)dev  << 11)
         | ((uint32_t)func <<  8)
         | ((uint32_t)(off & 0xFC));
}

uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off) {
    outl(PCI_ADDR, pci_addr(bus, dev, func, off));
    return inl(PCI_DATA);
}

uint16_t pci_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off) {
    uint32_t v = pci_read32(bus, dev, func, off);
    return (uint16_t)(v >> ((off & 2) * 8));
}

uint8_t pci_read8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off) {
    uint32_t v = pci_read32(bus, dev, func, off);
    return (uint8_t)(v >> ((off & 3) * 8));
}

void pci_write32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off, uint32_t v) {
    outl(PCI_ADDR, pci_addr(bus, dev, func, off));
    outl(PCI_DATA, v);
}

void pci_write16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off, uint16_t v) {
    uint32_t cur = pci_read32(bus, dev, func, off);
    int shift = (off & 2) * 8;
    cur = (cur & ~(0xFFFFu << shift)) | ((uint32_t)v << shift);
    pci_write32(bus, dev, func, off, cur);
}

void pci_enable_busmaster(uint8_t bus, uint8_t dev, uint8_t func) {
    uint16_t cmd = pci_read16(bus, dev, func, 0x04);
    pci_write16(bus, dev, func, 0x04, cmd | 0x07);  /* I/O + Mem + BusMaster */
}

int pci_find_device(uint16_t vendor, uint16_t device, pci_device_t *out) {
    for (uint8_t bus = 0; bus < 8; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            uint32_t id = pci_read32(bus, dev, 0, 0x00);
            if ((id & 0xFFFF) != vendor) continue;
            if ((id >> 16) != device)   continue;

            out->bus       = bus;
            out->dev       = dev;
            out->func      = 0;
            out->vendor_id = vendor;
            out->device_id = device;
            out->irq       = pci_read8(bus, dev, 0, 0x3C);

            for (int i = 0; i < 6; i++)
                out->bar[i] = pci_read32(bus, dev, 0, (uint8_t)(0x10 + i * 4));

            return 1;
        }
    }
    return 0;
}
