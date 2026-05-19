#include <ata.h>
#include <io.h>
#include <terminal.h>
#include <stdint.h>

/* Primary ATA channel I/O ports */
#define ATA_DATA        0x1F0u   /* 16-bit data register */
#define ATA_FEATURES    0x1F1u   /* write: features; read: error */
#define ATA_SECT_CNT    0x1F2u
#define ATA_LBA_LO      0x1F3u
#define ATA_LBA_MID     0x1F4u
#define ATA_LBA_HI      0x1F5u
#define ATA_DRIVE_HEAD  0x1F6u
#define ATA_STATUS      0x1F7u   /* read */
#define ATA_COMMAND     0x1F7u   /* write */
#define ATA_ALT_STATUS  0x3F6u   /* read; also device control (write) */

#define ATA_SR_BSY   0x80u
#define ATA_SR_DRDY  0x40u
#define ATA_SR_DRQ   0x08u
#define ATA_SR_ERR   0x01u

#define ATA_CMD_READ_SECTORS 0x20u
#define ATA_CMD_IDENTIFY     0xECu

static int disk_present = 0;

/* 400 ns delay: read alternate status 4× (each I/O cycle ≈ 100 ns). */
static void ata_delay(void) {
    inb(ATA_ALT_STATUS); inb(ATA_ALT_STATUS);
    inb(ATA_ALT_STATUS); inb(ATA_ALT_STATUS);
}

/* Poll until BSY clears.  Returns final status byte, or 0xFF on timeout. */
static uint8_t ata_wait_busy(void) {
    for (int i = 0; i < 100000; i++) {
        uint8_t st = inb(ATA_STATUS);
        if (!(st & ATA_SR_BSY)) return st;
    }
    return 0xFF;
}

/* Poll until DRQ or ERR sets.  Returns final status, or 0xFF on timeout. */
static uint8_t ata_wait_drq(void) {
    for (int i = 0; i < 100000; i++) {
        uint8_t st = inb(ATA_STATUS);
        if (st & (ATA_SR_DRQ | ATA_SR_ERR)) return st;
    }
    return 0xFF;
}

void ata_init(void) {
    /* Select master drive on primary channel. */
    outb(ATA_DRIVE_HEAD, 0xA0);
    ata_delay();

    /* Issue IDENTIFY DEVICE. */
    outb(ATA_SECT_CNT, 0);
    outb(ATA_LBA_LO,   0);
    outb(ATA_LBA_MID,  0);
    outb(ATA_LBA_HI,   0);
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);

    /* Status = 0x00 means no drive on this channel. */
    if (inb(ATA_STATUS) == 0x00) {
        terminal_write("[ATA] no drive\n");
        return;
    }

    /* Wait for BSY to clear. */
    uint8_t st = ata_wait_busy();
    if (st == 0xFF) {
        terminal_write("[ATA] timeout on IDENTIFY\n");
        return;
    }

    /* Non-zero LBA mid/hi → ATAPI (CD-ROM), not plain ATA. */
    if (inb(ATA_LBA_MID) != 0 || inb(ATA_LBA_HI) != 0) {
        terminal_write("[ATA] ATAPI device — skipping\n");
        return;
    }

    /* Wait for DRQ (identify data ready). */
    st = ata_wait_drq();
    if (st & ATA_SR_ERR) {
        terminal_write("[ATA] IDENTIFY error\n");
        return;
    }

    /* Drain the 256-word identify block (we don't parse it yet). */
    for (int i = 0; i < 256; i++) inw(ATA_DATA);

    disk_present = 1;
    terminal_write("[ATA] drive ready\n");
}

int ata_present(void) { return disk_present; }

int ata_read_sector(uint32_t lba, void *buf) {
    if (!disk_present) return -1;

    /* Wait for drive to be idle before issuing a new command. */
    uint8_t st = ata_wait_busy();
    if (st == 0xFF || (st & ATA_SR_ERR)) return -1;

    /* LBA28: select master drive, LBA bits 24–27 in low nibble. */
    outb(ATA_DRIVE_HEAD, (uint8_t)(0xE0 | ((lba >> 24) & 0x0Fu)));
    ata_delay();

    outb(ATA_SECT_CNT, 1);
    outb(ATA_LBA_LO,   (uint8_t)(lba));
    outb(ATA_LBA_MID,  (uint8_t)(lba >>  8));
    outb(ATA_LBA_HI,   (uint8_t)(lba >> 16));
    outb(ATA_COMMAND, ATA_CMD_READ_SECTORS);

    /* Wait for data ready. */
    st = ata_wait_drq();
    if (st == 0xFF || (st & ATA_SR_ERR)) return -1;

    /* Read 256 × 16-bit words = 512 bytes. */
    uint16_t *p = (uint16_t *)buf;
    for (int i = 0; i < 256; i++) p[i] = inw(ATA_DATA);

    return 0;
}
