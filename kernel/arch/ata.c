#include "ata.h"
#include "io.h"
#include "../libc/string.h"

static ata_device_t ata_drives[4];
static int ata_drive_count = 0;
static int ata_active_drive = -1;

static void ata_delay_400ns(unsigned short ctrl_port) {
    inb(ctrl_port);
    inb(ctrl_port);
    inb(ctrl_port);
    inb(ctrl_port);
}

static int ata_wait_bsy(unsigned short io_base) {
    for (int i = 0; i < 50000; i++) {
        if (!(inb(io_base + 7) & ATA_STATUS_BSY)) {
            return 0;
        }
    }
    return -1; // Timeout
}

static int ata_wait_drq(unsigned short io_base) {
    for (int i = 0; i < 50000; i++) {
        unsigned char st = inb(io_base + 7);
        if (st & ATA_STATUS_ERR) return -1;
        if (st & ATA_STATUS_DRQ) return 0;
    }
    return -1; // Timeout
}

static int ata_probe_single(unsigned short io_base, unsigned short ctrl_base, unsigned char drive_sel, ata_device_t *dev) {
    dev->present = 0;
    dev->is_atapi = 0;
    dev->io_base = io_base;
    dev->ctrl_base = ctrl_base;
    dev->drive_sel = drive_sel;
    dev->total_sectors = 0;
    dev->size_mb = 0;
    dev->model[0] = '\0';

    // 1. Select target drive
    outb(io_base + 6, drive_sel);
    ata_delay_400ns(ctrl_base);

    // 2. Disable interrupts on this controller channel
    outb(ctrl_base, 0x02);
    ata_delay_400ns(ctrl_base);

    // 3. Floating bus check (0xFF = no controller/drive on this port)
    unsigned char status = inb(io_base + 7);
    if (status == 0xFF) {
        return -1;
    }

    // 4. Send IDENTIFY command
    outb(io_base + 2, 0);
    outb(io_base + 3, 0);
    outb(io_base + 4, 0);
    outb(io_base + 5, 0);
    outb(io_base + 7, ATA_CMD_IDENTIFY);
    ata_delay_400ns(ctrl_base);

    // Poll status register up to 1000 iterations for slow hardware or optical drives
    for (int t = 0; t < 1000; t++) {
        status = inb(io_base + 7);
        if (status != 0 && status != 0xFF) break;
        io_wait();
    }
    if (status == 0 || status == 0xFF) {
        // Drive does not exist
        return -1;
    }

    if (ata_wait_bsy(io_base) != 0) {
        return -2;
    }

    // 5. Check signature for ATAPI optical drive
    unsigned char mid = inb(io_base + 4);
    unsigned char high = inb(io_base + 5);
    if ((mid == 0x14 && high == 0xEB) || (mid == 0x69 && high == 0x96)) {
        dev->present = 1;
        dev->is_atapi = 1;
        strcpy(dev->model, "ATAPI CD/DVD Optical Drive");
        // Issue IDENTIFY PACKET DEVICE (0xA1) to read true optical drive model name
        outb(io_base + 7, 0xA1);
        ata_delay_400ns(ctrl_base);
        if (ata_wait_drq(io_base) == 0) {
            unsigned short id_pkt[256];
            for (int i = 0; i < 256; i++) {
                id_pkt[i] = inw(io_base + 0);
            }
            int m_idx = 0;
            for (int i = 0; i < 20; i++) {
                unsigned short w = id_pkt[27 + i];
                char c1 = (char)((w >> 8) & 0xFF);
                char c2 = (char)(w & 0xFF);
                dev->model[m_idx++] = (c1 >= 32 && c1 <= 126) ? c1 : ' ';
                dev->model[m_idx++] = (c2 >= 32 && c2 <= 126) ? c2 : ' ';
            }
            dev->model[m_idx] = '\0';
            while (m_idx > 0 && dev->model[m_idx - 1] == ' ') {
                dev->model[--m_idx] = '\0';
            }
        }
        return 0;
    }

    // 6. Wait for DRQ or ERR
    if (ata_wait_drq(io_base) != 0) {
        return -3;
    }

    // 7. Read 256 words (512 bytes)
    unsigned short id_data[256];
    for (int i = 0; i < 256; i++) {
        id_data[i] = inw(io_base + 0);
    }

    dev->present = 1;
    dev->is_atapi = 0;

    // 8. Extract Model string (words 27..46)
    int m_idx = 0;
    for (int i = 0; i < 20; i++) {
        unsigned short w = id_data[27 + i];
        char c1 = (char)((w >> 8) & 0xFF);
        char c2 = (char)(w & 0xFF);
        dev->model[m_idx++] = (c1 >= 32 && c1 <= 126) ? c1 : ' ';
        dev->model[m_idx++] = (c2 >= 32 && c2 <= 126) ? c2 : ' ';
    }
    dev->model[m_idx] = '\0';
    while (m_idx > 0 && dev->model[m_idx - 1] == ' ') {
        dev->model[--m_idx] = '\0';
    }
    if (m_idx == 0) {
        strcpy(dev->model, (drive_sel == 0xA0) ? "ATA Master Hard Disk" : "ATA Slave Hard Disk");
    }

    // 9. Calculate sectors & MB capacity
    unsigned int sec28 = (unsigned int)id_data[60] | ((unsigned int)id_data[61] << 16);
    unsigned int sec48_low = (unsigned int)id_data[100] | ((unsigned int)id_data[101] << 16);
    unsigned int sec48_high = (unsigned int)id_data[102] | ((unsigned int)id_data[103] << 16);

    if (sec48_high > 0 || (sec48_low > sec28 && sec48_low > 20480)) {
        dev->total_sectors = sec48_low;
        dev->size_mb = (sec48_high * 2048) + (sec48_low / 2048);
    } else if (sec28 > 0) {
        dev->total_sectors = sec28;
        dev->size_mb = sec28 / 2048;
    } else {
        unsigned int chs = (unsigned int)id_data[1] * (unsigned int)id_data[3] * (unsigned int)id_data[6];
        dev->total_sectors = (chs > 0) ? chs : 20480;
        dev->size_mb = dev->total_sectors / 2048;
    }

    if (dev->size_mb == 0) dev->size_mb = 10;
    return 0;
}

int ata_init(void) {
    ata_drive_count = 0;
    ata_active_drive = -1;

    struct {
        unsigned short io;
        unsigned short ctrl;
        unsigned char dev;
    } ports[4] = {
        { 0x1F0, 0x3F6, 0xA0 }, // Primary Master
        { 0x1F0, 0x3F6, 0xB0 }, // Primary Slave
        { 0x170, 0x376, 0xA0 }, // Secondary Master
        { 0x170, 0x376, 0xB0 }  // Secondary Slave
    };

    for (int i = 0; i < 4; i++) {
        ata_probe_single(ports[i].io, ports[i].ctrl, ports[i].dev, &ata_drives[i]);
        if (ata_drives[i].present && !ata_drives[i].is_atapi) {
            ata_drive_count++;
            if (ata_active_drive == -1) {
                ata_active_drive = i;
            }
        }
    }

    return (ata_active_drive != -1) ? 0 : -1;
}

int ata_is_available(void) {
    return (ata_active_drive >= 0 && ata_drives[ata_active_drive].present && !ata_drives[ata_active_drive].is_atapi);
}

int ata_get_drive_count(void) {
    return ata_drive_count;
}

int ata_get_atapi_count(void) {
    int count = 0;
    for (int i = 0; i < 4; i++) {
        if (ata_drives[i].present && ata_drives[i].is_atapi) {
            count++;
        }
    }
    return count;
}

int ata_get_active_drive(void) {
    return ata_active_drive;
}

int ata_select_drive(int drive_index) {
    if (drive_index >= 0 && drive_index < 4 && ata_drives[drive_index].present && !ata_drives[drive_index].is_atapi) {
        ata_active_drive = drive_index;
        return 0;
    }
    return -1;
}

const ata_device_t *ata_get_device(int drive_index) {
    if (drive_index >= 0 && drive_index < 4) {
        return &ata_drives[drive_index];
    }
    return 0;
}

const ata_device_t *ata_get_atapi_device(void) {
    for (int i = 0; i < 4; i++) {
        if (ata_drives[i].present && ata_drives[i].is_atapi) {
            return &ata_drives[i];
        }
    }
    return 0;
}

unsigned int ata_get_total_sectors(void) {
    if (!ata_is_available()) return 0;
    return ata_drives[ata_active_drive].total_sectors;
}

unsigned int ata_get_size_mb(void) {
    if (!ata_is_available()) return 0;
    return ata_drives[ata_active_drive].size_mb;
}

const char *ata_get_model(void) {
    if (!ata_is_available()) return "No Disk";
    return ata_drives[ata_active_drive].model;
}

void ata_get_capacity_string(char *buf, int max_len) {
    if (!buf || max_len < 8) return;
    if (!ata_is_available()) {
        strncpy(buf, "No Disk", max_len - 1);
        buf[max_len - 1] = '\0';
        return;
    }

    unsigned int mb = ata_drives[ata_active_drive].size_mb;
    if (mb >= 1024) {
        int gb_whole = mb / 1024;
        int gb_tenth = ((mb % 1024) * 10) / 1024;
        snprintf(buf, max_len, "%d.%d GB", gb_whole, gb_tenth);
    } else {
        snprintf(buf, max_len, "%u MB", mb);
    }
}

const char *ata_get_location_string(void) {
    if (ata_active_drive < 0 || ata_active_drive >= 4) return "No ATA Disk";
    switch (ata_active_drive) {
        case 0: return "Primary Master (0x1F0, IDE 0:0)";
        case 1: return "Primary Slave (0x1F0, IDE 0:1)";
        case 2: return "Secondary Master (0x170, IDE 1:0)";
        case 3: return "Secondary Slave (0x170, IDE 1:1)";
        default: return "Unknown";
    }
}

int ata_read_sector(unsigned int lba, void *buf) {
    if (!buf || !ata_is_available()) return -1;
    ata_device_t *d = &ata_drives[ata_active_drive];
    unsigned short io = d->io_base;
    unsigned short ctrl = d->ctrl_base;
    unsigned char dev_flag = (d->drive_sel == 0xB0) ? 0xF0 : 0xE0;

    if (ata_wait_bsy(io) != 0) return -2;

    outb(io + 6, dev_flag | ((lba >> 24) & 0x0F));
    ata_delay_400ns(ctrl);

    outb(io + 1, 0x00);
    outb(io + 2, 1);
    outb(io + 3, (unsigned char)(lba & 0xFF));
    outb(io + 4, (unsigned char)((lba >> 8) & 0xFF));
    outb(io + 5, (unsigned char)((lba >> 16) & 0xFF));
    outb(io + 7, ATA_CMD_READ_PIO);

    if (ata_wait_drq(io) != 0) return -3;

    unsigned short *ptr = (unsigned short *)buf;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(io + 0);
    }
    return 0;
}

int ata_write_sector(unsigned int lba, const void *buf) {
    if (!buf || !ata_is_available()) return -1;
    ata_device_t *d = &ata_drives[ata_active_drive];
    unsigned short io = d->io_base;
    unsigned short ctrl = d->ctrl_base;
    unsigned char dev_flag = (d->drive_sel == 0xB0) ? 0xF0 : 0xE0;

    if (ata_wait_bsy(io) != 0) return -2;

    outb(io + 6, dev_flag | ((lba >> 24) & 0x0F));
    ata_delay_400ns(ctrl);

    outb(io + 1, 0x00);
    outb(io + 2, 1);
    outb(io + 3, (unsigned char)(lba & 0xFF));
    outb(io + 4, (unsigned char)((lba >> 8) & 0xFF));
    outb(io + 5, (unsigned char)((lba >> 16) & 0xFF));
    outb(io + 7, ATA_CMD_WRITE_PIO);

    if (ata_wait_drq(io) != 0) return -3;

    const unsigned short *ptr = (const unsigned short *)buf;
    for (int i = 0; i < 256; i++) {
        outw(io + 0, ptr[i]);
    }

    // Do NOT flush cache per single sector to prevent multi-second freeze.
    // Explicitly call ata_flush_cache() when multi-sector block completes.
    return 0;
}

int ata_flush_cache(void) {
    if (!ata_is_available()) return -1;
    ata_device_t *d = &ata_drives[ata_active_drive];
    unsigned short io = d->io_base;
    unsigned short ctrl = d->ctrl_base;
    unsigned char dev_flag = (d->drive_sel == 0xB0) ? 0xF0 : 0xE0;

    if (ata_wait_bsy(io) != 0) return -2;
    outb(io + 6, dev_flag);
    ata_delay_400ns(ctrl);
    outb(io + 7, ATA_CMD_CACHE_FLUSH);
    return ata_wait_bsy(io);
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
    ata_flush_cache();
    return 0;
}
