#include "ata.h"
#include "io.h"
#include "../libc/string.h"

static int ata_drive_present = 0;
static unsigned short ata_identify_data[256];
static unsigned int ata_total_sectors = 0;
static unsigned int ata_size_mb = 0;
static char ata_model_str[42] = "AuraOS Primary Hard Disk";

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
        ata_total_sectors = 0;
        ata_size_mb = 0;
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
        // Read 256 words of IDENTIFY response
        for (int i = 0; i < 256; i++) {
            ata_identify_data[i] = inw(ATA_DATA_PORT);
        }
        ata_drive_present = 1;

        // Parse Model string (words 27..46)
        int m_idx = 0;
        for (int i = 0; i < 20; i++) {
            unsigned short w = ata_identify_data[27 + i];
            char c1 = (char)((w >> 8) & 0xFF);
            char c2 = (char)(w & 0xFF);
            ata_model_str[m_idx++] = (c1 >= 32 && c1 <= 126) ? c1 : ' ';
            ata_model_str[m_idx++] = (c2 >= 32 && c2 <= 126) ? c2 : ' ';
        }
        ata_model_str[m_idx] = '\0';

        // Trim trailing spaces
        while (m_idx > 0 && ata_model_str[m_idx - 1] == ' ') {
            ata_model_str[--m_idx] = '\0';
        }

        // Parse 28-bit LBA sectors (words 60..61)
        unsigned int sec28 = (unsigned int)ata_identify_data[60] |
                            ((unsigned int)ata_identify_data[61] << 16);

        // Parse 48-bit LBA sectors (words 100..103)
        unsigned int sec48_low = (unsigned int)ata_identify_data[100] |
                                ((unsigned int)ata_identify_data[101] << 16);
        unsigned int sec48_high = (unsigned int)ata_identify_data[102] |
                                 ((unsigned int)ata_identify_data[103] << 16);

        if (sec48_high > 0 || (sec48_low > sec28 && sec48_low > 20480)) {
            // Large capacity hard disk (e.g. 8 GB, 20 GB, 40 GB in VMware)
            ata_total_sectors = sec48_low;
            unsigned int mb_high = sec48_high * 2048;
            unsigned int mb_low = sec48_low / 2048;
            ata_size_mb = mb_high + mb_low;
        } else if (sec28 > 0) {
            ata_total_sectors = sec28;
            ata_size_mb = sec28 / 2048;
        } else {
            // CHS geometry fallback
            unsigned int chs = (unsigned int)ata_identify_data[1] *
                               (unsigned int)ata_identify_data[3] *
                               (unsigned int)ata_identify_data[6];
            ata_total_sectors = (chs > 0) ? chs : 20480;
            ata_size_mb = ata_total_sectors / 2048;
        }

        if (ata_size_mb == 0) ata_size_mb = 10;
        return 0;
    }

    // Drive present fallback
    ata_drive_present = 1;
    ata_total_sectors = 20480;
    ata_size_mb = 10;
    return 0;
}

int ata_is_available(void) {
    return ata_drive_present;
}

unsigned int ata_get_total_sectors(void) {
    return ata_total_sectors;
}

unsigned int ata_get_size_mb(void) {
    return ata_size_mb;
}

const char *ata_get_model(void) {
    return ata_model_str;
}

void ata_get_capacity_string(char *buf, int max_len) {
    if (!buf || max_len < 8) return;
    if (!ata_drive_present) {
        strncpy(buf, "No Disk", max_len - 1);
        buf[max_len - 1] = '\0';
        return;
    }

    if (ata_size_mb >= 1024) {
        int gb_whole = ata_size_mb / 1024;
        int gb_tenth = ((ata_size_mb % 1024) * 10) / 1024;
        snprintf(buf, max_len, "%d.%d GB", gb_whole, gb_tenth);
    } else {
        snprintf(buf, max_len, "%u MB", ata_size_mb);
    }
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
