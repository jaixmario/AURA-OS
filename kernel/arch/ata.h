#ifndef ATA_H
#define ATA_H

#define ATA_SECTOR_SIZE 512

// Primary ATA I/O Ports
#define ATA_DATA_PORT       0x1F0
#define ATA_FEATURES_PORT   0x1F1
#define ATA_SEC_COUNT_PORT  0x1F2
#define ATA_LBA_LOW_PORT    0x1F3
#define ATA_LBA_MID_PORT    0x1F4
#define ATA_LBA_HIGH_PORT   0x1F5
#define ATA_DRIVE_PORT      0x1F6
#define ATA_COMMAND_PORT    0x1F7
#define ATA_STATUS_PORT     0x1F7
#define ATA_CONTROL_PORT    0x3F6

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

int  ata_init(void);
int  ata_is_available(void);
unsigned int ata_get_total_sectors(void);
unsigned int ata_get_size_mb(void);
const char  *ata_get_model(void);
void ata_get_capacity_string(char *buf, int max_len);

int  ata_read_sector(unsigned int lba, void *buf);
int  ata_write_sector(unsigned int lba, const void *buf);
int  ata_read_sectors(unsigned int lba, unsigned int count, void *buf);
int  ata_write_sectors(unsigned int lba, unsigned int count, const void *buf);

#endif
