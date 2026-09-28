#ifndef ATA_H
#define ATA_H

#define ATA_SECTOR_SIZE 512

// ATA Commands
#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY    0xEC

// Status Register Bits
#define ATA_STATUS_ERR  0x01
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_DF   0x20
#define ATA_STATUS_DRDY 0x40
#define ATA_STATUS_BSY  0x80

typedef struct {
    int present;
    int is_atapi;
    unsigned short io_base;
    unsigned short ctrl_base;
    unsigned char drive_sel; // 0xA0 (master) or 0xB0 (slave)
    unsigned int total_sectors;
    unsigned int size_mb;
    char model[42];
} ata_device_t;

int  ata_init(void);
int  ata_is_available(void);
int  ata_get_drive_count(void);
int  ata_get_active_drive(void);
int  ata_select_drive(int drive_index);
const ata_device_t *ata_get_device(int drive_index);

unsigned int ata_get_total_sectors(void);
unsigned int ata_get_size_mb(void);
const char  *ata_get_model(void);
void ata_get_capacity_string(char *buf, int max_len);
const char  *ata_get_location_string(void);

int  ata_read_sector(unsigned int lba, void *buf);
int  ata_write_sector(unsigned int lba, const void *buf);
int  ata_read_sectors(unsigned int lba, unsigned int count, void *buf);
int  ata_write_sectors(unsigned int lba, unsigned int count, const void *buf);
int  ata_flush_cache(void);

#endif
