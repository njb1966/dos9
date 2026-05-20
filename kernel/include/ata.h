#pragma once
#include <stdint.h>

/* Initialise the primary ATA channel (PIO polling, master drive).
   Prints a status line to the terminal.  Safe to call even if no
   drive is present — ata_present() will return 0 afterwards. */
void ata_init(void);

/* Returns 1 if a drive was detected, 0 otherwise. */
int ata_present(void);

/* Read one 512-byte sector at LBA `lba` into `buf` (must be ≥ 512 bytes).
   Returns 0 on success, -1 on error. */
int ata_read_sector(uint32_t lba, void *buf);

/* Write one 512-byte sector at LBA `lba` from `buf` (must be ≥ 512 bytes).
   Returns 0 on success, -1 on error. */
int ata_write_sector(uint32_t lba, const void *buf);
