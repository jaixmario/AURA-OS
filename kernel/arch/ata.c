#include "ata.h"
#include "io.h"

static int ata_drive_present = 0;

static void ata_delay_400ns(void) {
    inb(ATA_CONTROL_PORT);
    inb(ATA_CONTROL_PORT);
    inb(ATA_CONTROL_PORT);
    inb(ATA_CONTROL_PORT);
}

static int ata_wait_bsy(void) {
    for (int i = 0; i < 100000; i++) {
        if (!(inb(ATA_STATUS_PORT) & ATA_STATUS_BSY)) {
            return 0;
        }
    }
    return -1; // Timeout
}

static int ata_wait_drq(void) {
    for (int i = 0; i < 100000; i++) {
        unsigned char st = inb(ATA_STATUS_PORT);
        if (st & ATA_STATUS_ERR) return -1;
        if (st & ATA_STATUS_DRQ) return 0;
    }
    return -1; // Timeout
}

int ata_init(void) {
    // Select master drive on primary bus
    outb(ATA_DRIVE_PORT, 0xA0);
    ata_delay_400ns();

    // Reset ATA bus via control port
    outb(ATA_CONTROL_PORT, 0x02); // nIEN=1 (disable interrupts)
    ata_delay_400ns();

    unsigned char status = inb(ATA_STATUS_PORT);
    if (status == 0xFF) {
        // Floating bus, no drive attached
        ata_drive_present = 0;
        return -1;
    }

    // Send IDENTIFY command
    outb(ATA_SEC_COUNT_PORT, 0);
    outb(ATA_LBA_LOW_PORT, 0);
    outb(ATA_LBA_MID_PORT, 0);
    outb(ATA_LBA_HIGH_PORT, 0);
    outb(ATA_COMMAND_PORT, ATA_CMD_IDENTIFY);
    ata_delay_400ns();

    status = inb(ATA_STATUS_PORT);
    if (status == 0) {
        // Drive does not exist
        ata_drive_present = 0;
        return -1;
    }

    if (ata_wait_bsy() != 0) {
        ata_drive_present = 0;
        return -2;
    }

    // Check if device is ATAPI
    unsigned char mid = inb(ATA_LBA_MID_PORT);
    unsigned char high = inb(ATA_LBA_HIGH_PORT);
    if (mid == 0x14 && high == 0xEB) {
        // ATAPI device (CD-ROM)
        ata_drive_present = 0;
        return -3;
    }

    // Wait until DRQ or ERR
    if (ata_wait_drq() == 0) {
        // Read 256 words of IDENTIFY response to clear the buffer
        for (int i = 0; i < 256; i++) {
            inw(ATA_DATA_PORT);
        }
        ata_drive_present = 1;
        return 0;
    }

    // Drive responded but not in IDENTIFY DRQ; assume present if status valid
    ata_drive_present = 1;
    return 0;
}

int ata_is_available(void) {
    return ata_drive_present;
}

int ata_read_sector(unsigned int lba, void *buf) {
    if (!buf) return -1;

    if (ata_wait_bsy() != 0) return -2;

    // Send LBA and drive select (0xE0 = LBA mode, Master)
    outb(ATA_DRIVE_PORT, 0xE0 | ((lba >> 24) & 0x0F));
    ata_delay_400ns();

    outb(ATA_FEATURES_PORT, 0x00);
    outb(ATA_SEC_COUNT_PORT, 1);
    outb(ATA_LBA_LOW_PORT, (unsigned char)(lba & 0xFF));
    outb(ATA_LBA_MID_PORT, (unsigned char)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH_PORT, (unsigned char)((lba >> 16) & 0xFF));
    outb(ATA_COMMAND_PORT, ATA_CMD_READ_PIO);

    if (ata_wait_drq() != 0) return -3;

    unsigned short *ptr = (unsigned short *)buf;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(ATA_DATA_PORT);
    }

    return 0;
}

int ata_write_sector(unsigned int lba, const void *buf) {
    if (!buf) return -1;

    if (ata_wait_bsy() != 0) return -2;

    outb(ATA_DRIVE_PORT, 0xE0 | ((lba >> 24) & 0x0F));
    ata_delay_400ns();

    outb(ATA_FEATURES_PORT, 0x00);
    outb(ATA_SEC_COUNT_PORT, 1);
    outb(ATA_LBA_LOW_PORT, (unsigned char)(lba & 0xFF));
    outb(ATA_LBA_MID_PORT, (unsigned char)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH_PORT, (unsigned char)((lba >> 16) & 0xFF));
    outb(ATA_COMMAND_PORT, ATA_CMD_WRITE_PIO);

    if (ata_wait_drq() != 0) return -3;

    const unsigned short *ptr = (const unsigned short *)buf;
    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA_PORT, ptr[i]);
    }

    // Flush cache to ensure physical drive write
    outb(ATA_COMMAND_PORT, ATA_CMD_CACHE_FLUSH);
    ata_wait_bsy();

    return 0;
}

int ata_read_sectors(unsigned int lba, unsigned int count, void *buf) {
    unsigned char *ptr = (unsigned char *)buf;
    for (unsigned int i = 0; i < count; i++) {
        int res = ata_read_sector(lba + i, ptr + (i * ATA_SECTOR_SIZE));
        if (res != 0) return res;
    }
    return 0;
}

int ata_write_sectors(unsigned int lba, unsigned int count, const void *buf) {
    const unsigned char *ptr = (const unsigned char *)buf;
    for (unsigned int i = 0; i < count; i++) {
        int res = ata_write_sector(lba + i, ptr + (i * ATA_SECTOR_SIZE));
        if (res != 0) return res;
    }
    return 0;
}
