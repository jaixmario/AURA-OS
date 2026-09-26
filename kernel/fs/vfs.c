#include "vfs.h"
#include "../libc/string.h"
#include "../arch/rtc.h"
#include "../arch/ata.h"

#define ATA_FS_MAGIC         0x41555241 // 'AURA'
#define ATA_FS_VERSION       1
#define ATA_FS_SUPER_LBA     512
#define ATA_FS_SUPER_SECTORS 4
#define ATA_SECTORS_PER_FILE 4
#define ATA_FS_DATA_LBA      516

typedef struct {
    char name[VFS_MAX_FILENAME];
    char folder[VFS_MAX_FOLDER];
    unsigned int size;
    unsigned char attr;
    unsigned int created_year;
    unsigned int created_month;
    unsigned int created_day;
    unsigned int lba_start;
    unsigned int sector_count;
} __attribute__((packed)) ata_disk_entry_t;

typedef struct {
    unsigned int magic;
    unsigned int version;
    unsigned int file_count;
    unsigned int total_capacity;
    ata_disk_entry_t entries[VFS_MAX_FILES];
    unsigned char padding[2048 - 16 - (VFS_MAX_FILES * sizeof(ata_disk_entry_t))];
} __attribute__((packed)) ata_superblock_t;

static vfs_file_t files[VFS_MAX_FILES];
static int file_count = 0;
static int disk_backed = 0;

static const char *default_readme =
    "==================================================\n"
    "       AuraOS Graphical Operating System\n"
    "==================================================\n"
    "Version     : 1.2.0 (32-bit x86 Protected Mode)\n"
    "Graphics    : VESA VBE 2.0+ (1024x768 TrueColor)\n"
    "Storage     : ATA PIO Hard Disk (LBA Sector 512+)\n"
    "Drivers     : CMOS RTC, PS/2 Mouse & Keyboard, PIT\n"
    "Apps        : File Explorer, Terminal, Settings,\n"
    "              Calculator, Paint Canvas, Notes Editor.\n\n"
    "Files are persistently stored on your hard drive!\n";

static const char *default_cfg =
    "# AuraOS Desktop Configuration\n"
    "[STORAGE]\n"
    "DRIVER=ATA_PIO\n"
    "PRIMARY_BUS=0x1F0\n"
    "LBA_OFFSET=512\n"
    "MAX_FILES=24\n\n"
    "[DISPLAY]\n"
    "WIDTH=1024\n"
    "HEIGHT=768\n"
    "BPP=32\n"
    "VBE_MODE=0x4118\n"
    "DOUBLE_BUFFER=ENABLED\n\n"
    "[INPUT]\n"
    "MOUSE_RATE=200\n"
    "KEYBOARD_LAYOUT=US_QWERTY\n";

static const char *default_notes =
    "✦ AuraOS Notes & Documents\n"
    "- ATA PIO Hard Disk Storage Driver active\n"
    "- Files are written directly to disk sectors\n"
    "- Save As dialog allows choosing destination folder\n"
    "- Folders: Documents, Storage, System\n";

static const char *default_startup =
    "# AuraOS Bootup Script\n"
    "echo \"Mounting ATA PIO Hard Drive at LBA 512...\"\n"
    "sync\n"
    "uptime\n"
    "echo \"Storage ready.\"\n";

static const char *default_kernel =
    "[KERNEL STORAGE MAP]\n"
    "Sector 0      : MBR Bootloader (512 B)\n"
    "Sectors 1..128: Kernel Image (64 KB)\n"
    "Sector 512    : AuraOS Superblock & Inode Index\n"
    "Sectors 516+  : File Data Clusters (4 sectors/file)\n";

static const char *default_hardware =
    "[HARDWARE DIAGNOSTICS]\n"
    "CPU: x86 32-bit Protected Mode\n"
    "Storage: ATA Primary Master LBA Mode Enabled\n"
    "RTC: Motherboard CMOS Clock Synchronized\n"
    "Mouse: Auxiliary PS/2 Sample Rate 200/s\n";

int vfs_sync_disk(void) {
    if (!disk_backed) return 0;

    ata_superblock_t sb;
    memset(&sb, 0, sizeof(sb));
    sb.magic = ATA_FS_MAGIC;
    sb.version = ATA_FS_VERSION;
    sb.file_count = file_count;
    sb.total_capacity = VFS_MAX_FILES * VFS_MAX_FILESIZE;

    for (int i = 0; i < file_count; i++) {
        strncpy(sb.entries[i].name, files[i].name, VFS_MAX_FILENAME - 1);
        strncpy(sb.entries[i].folder, files[i].folder, VFS_MAX_FOLDER - 1);
        sb.entries[i].size = files[i].size;
        sb.entries[i].attr = files[i].attr;
        sb.entries[i].created_year = files[i].created_year;
        sb.entries[i].created_month = files[i].created_month;
        sb.entries[i].created_day = files[i].created_day;
        sb.entries[i].lba_start = files[i].disk_lba;
        sb.entries[i].sector_count = ATA_SECTORS_PER_FILE;

        // Write file data sectors (4 sectors = 2048 bytes)
        unsigned char sec_buf[ATA_SECTORS_PER_FILE * ATA_SECTOR_SIZE];
        memset(sec_buf, 0, sizeof(sec_buf));
        if (files[i].size > 0) {
            memcpy(sec_buf, files[i].data, files[i].size);
        }
        ata_write_sectors(files[i].disk_lba, ATA_SECTORS_PER_FILE, sec_buf);
    }

    // Write superblock (4 sectors at LBA 512)
    ata_write_sectors(ATA_FS_SUPER_LBA, ATA_FS_SUPER_SECTORS, &sb);
    return 0;
}

int vfs_create_file(const char *name, const char *folder, const char *content, unsigned int size, unsigned char attr) {
    if (!name || file_count >= VFS_MAX_FILES) return -1;
    if (vfs_find(name)) return -2; // File already exists

    vfs_file_t *f = &files[file_count];
    strncpy(f->name, name, VFS_MAX_FILENAME - 1);
    f->name[VFS_MAX_FILENAME - 1] = '\0';

    if (folder && folder[0] != '\0') {
        strncpy(f->folder, folder, VFS_MAX_FOLDER - 1);
        f->folder[VFS_MAX_FOLDER - 1] = '\0';
    } else {
        strcpy(f->folder, "Documents");
    }

    f->attr = attr;
    if (size >= VFS_MAX_FILESIZE) size = VFS_MAX_FILESIZE - 1;
    f->size = size;

    if (content && size > 0) {
        memcpy(f->data, content, size);
    }
    f->data[size] = '\0';

    rtc_time_t t;
    rtc_get_datetime(&t);
    f->created_year = t.year;
    f->created_month = t.month;
    f->created_day = t.day;

    f->disk_lba = ATA_FS_DATA_LBA + (file_count * ATA_SECTORS_PER_FILE);

    file_count++;

    if (disk_backed) {
        vfs_sync_disk();
    }
    return 0;
}

int vfs_write_file(const char *name, const char *content, unsigned int size) {
    vfs_file_t *f = vfs_find(name);
    if (!f) return -1;
    if (f->attr & FS_ATTR_READONLY) return -2;

    if (size >= VFS_MAX_FILESIZE) size = VFS_MAX_FILESIZE - 1;
    f->size = size;

    if (content && size > 0) {
        memcpy(f->data, content, size);
    }
    f->data[size] = '\0';

    if (disk_backed) {
        // Write file's sectors directly
        unsigned char sec_buf[ATA_SECTORS_PER_FILE * ATA_SECTOR_SIZE];
        memset(sec_buf, 0, sizeof(sec_buf));
        if (size > 0) {
            memcpy(sec_buf, f->data, size);
        }
        ata_write_sectors(f->disk_lba, ATA_SECTORS_PER_FILE, sec_buf);
        vfs_sync_disk();
    }
    return 0;
}

int vfs_move_file(const char *name, const char *new_folder) {
    vfs_file_t *f = vfs_find(name);
    if (!f || !new_folder) return -1;
    strncpy(f->folder, new_folder, VFS_MAX_FOLDER - 1);
    f->folder[VFS_MAX_FOLDER - 1] = '\0';
    if (disk_backed) {
        vfs_sync_disk();
    }
    return 0;
}

int vfs_delete_file(const char *name) {
    for (int i = 0; i < file_count; i++) {
        if (strcmp(files[i].name, name) == 0) {
            if (files[i].attr & FS_ATTR_SYSTEM) return -2; // Cannot delete system file
            // Shift remaining files down
            for (int j = i; j < file_count - 1; j++) {
                files[j] = files[j + 1];
                files[j].disk_lba = ATA_FS_DATA_LBA + (j * ATA_SECTORS_PER_FILE);
            }
            file_count--;
            if (disk_backed) {
                vfs_sync_disk();
            }
            return 0;
        }
    }
    return -1;
}

vfs_file_t *vfs_find(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < file_count; i++) {
        if (strcmp(files[i].name, name) == 0) {
            return &files[i];
        }
    }
    return 0;
}

vfs_file_t *vfs_get_at(int index) {
    if (index >= 0 && index < file_count) {
        return &files[index];
    }
    return 0;
}

int vfs_get_count(void) {
    return file_count;
}

unsigned int vfs_get_total_used(void) {
    unsigned int total = 0;
    for (int i = 0; i < file_count; i++) {
        total += files[i].size;
    }
    return total;
}

unsigned int vfs_get_total_capacity(void) {
    return VFS_MAX_FILES * VFS_MAX_FILESIZE;
}

int vfs_is_disk_backed(void) {
    return disk_backed;
}

void vfs_init(void) {
    file_count = 0;
    disk_backed = 0;

    int ata_ok = ata_init();
    if (ata_ok == 0 || ata_is_available()) {
        disk_backed = 1;
        ata_superblock_t sb;
        memset(&sb, 0, sizeof(sb));

        int r = ata_read_sectors(ATA_FS_SUPER_LBA, ATA_FS_SUPER_SECTORS, &sb);
        if (r == 0 && sb.magic == ATA_FS_MAGIC && sb.version == ATA_FS_VERSION && sb.file_count > 0 && sb.file_count <= VFS_MAX_FILES) {
            // Restore file records and data directly from disk!
            file_count = sb.file_count;
            for (int i = 0; i < file_count; i++) {
                strncpy(files[i].name, sb.entries[i].name, VFS_MAX_FILENAME - 1);
                files[i].name[VFS_MAX_FILENAME - 1] = '\0';

                strncpy(files[i].folder, sb.entries[i].folder, VFS_MAX_FOLDER - 1);
                files[i].folder[VFS_MAX_FOLDER - 1] = '\0';

                files[i].size = sb.entries[i].size;
                if (files[i].size >= VFS_MAX_FILESIZE) files[i].size = VFS_MAX_FILESIZE - 1;

                files[i].attr = sb.entries[i].attr;
                files[i].created_year = sb.entries[i].created_year;
                files[i].created_month = sb.entries[i].created_month;
                files[i].created_day = sb.entries[i].created_day;
                files[i].disk_lba = sb.entries[i].lba_start;

                // Read file data sectors from disk
                unsigned char sec_buf[ATA_SECTORS_PER_FILE * ATA_SECTOR_SIZE];
                ata_read_sectors(files[i].disk_lba, ATA_SECTORS_PER_FILE, sec_buf);
                memcpy(files[i].data, sec_buf, files[i].size);
                files[i].data[files[i].size] = '\0';
            }
            return;
        }
    }

    // First boot or unformatted disk: initialize core default files with folders
    vfs_create_file("README.TXT",   "Storage",   default_readme,   strlen(default_readme),   FS_ATTR_READONLY);
    vfs_create_file("SYSTEM.CFG",   "System",    default_cfg,      strlen(default_cfg),      FS_ATTR_SYSTEM);
    vfs_create_file("NOTES.TXT",    "Documents", default_notes,    strlen(default_notes),    FS_ATTR_USER);
    vfs_create_file("STARTUP.SH",   "Documents", default_startup,  strlen(default_startup),  FS_ATTR_USER);
    vfs_create_file("KERNEL.SYS",   "System",    default_kernel,   strlen(default_kernel),   FS_ATTR_SYSTEM);
    vfs_create_file("HARDWARE.LOG", "System",    default_hardware, strlen(default_hardware), FS_ATTR_SYSTEM);

    if (disk_backed) {
        vfs_sync_disk();
    }
}
