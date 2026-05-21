#pragma once
#include <stdint.h>

typedef struct {
    uint8_t  bus, dev, func;
    uint16_t vendor_id, device_id;
    uint8_t  irq;                    /* INTx line from config offset 0x3C */
    uint32_t bar[6];                 /* BARs 0-5, raw values */
} pci_device_t;

/* Read PCI config space — I/O port method (0xCF8 / 0xCFC). */
uint8_t  pci_read8 (uint8_t bus, uint8_t dev, uint8_t func, uint8_t off);
uint16_t pci_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off);
uint32_t pci_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off);

void pci_write16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off, uint16_t v);
void pci_write32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t off, uint32_t v);

/* Enable bus mastering (bit 2 of Command register). */
void pci_enable_busmaster(uint8_t bus, uint8_t dev, uint8_t func);

/* Find first device matching vendor:device IDs.  Returns 1 if found. */
int pci_find_device(uint16_t vendor, uint16_t device, pci_device_t *out);
