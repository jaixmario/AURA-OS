#include "vfs.h"
#include "../libc/string.h"
#include "../arch/rtc.h"

static vfs_file_t files[VFS_MAX_FILES];
static int file_count = 0;

static const char *default_readme =
    "==================================================\n"
    "       AuraOS Graphical Operating System\n"
    "==================================================\n"
    "Version     : 1.2.0 (32-bit x86 Protected Mode)\n"
    "Graphics    : VESA VBE 2.0+ (1024x768 TrueColor)\n"
    "Architecture: Bare-metal custom microkernel\n"
    "Drivers     : CMOS RTC, PS/2 Mouse & Keyboard, PIT 100Hz\n"
    "Apps        : File Explorer, Terminal, Settings,\n"
    "              Calculator, Paint Canvas, Notes Editor.\n\n"
    "Enjoy your custom bare-metal operating system!\n";

static const char *default_cfg =
    "# AuraOS Desktop Configuration\n"
    "[DISPLAY]\n"
    "WIDTH=1024\n"
    "HEIGHT=768\n"
    "BPP=32\n"
    "VBE_MODE=0x4118\n"
    "DOUBLE_BUFFER=ENABLED\n"
    "REFRESH_HZ=60\n\n"
    "[INPUT]\n"
    "MOUSE_RATE=200\n"
    "MOUSE_SPEED=1.5\n"
    "KEYBOARD_LAYOUT=US_QWERTY\n\n"
    "[KERNEL]\n"
    "STACK_SIZE=1048576\n"
    "HEAP_SIZE=16777216\n"
    "TIMER_HZ=100\n"
    "RTC_SYNC=CMOS\n";

static const char *default_notes =
    "✦ AuraOS Workspace Notes\n"
    "- Implemented CMOS hardware RTC driver (Ports 0x70/0x71)\n"
    "- Added live digital clock & calendar to taskbar tray\n"
    "- Built 5-tab Settings Control Panel\n"
    "- Created Virtual File System (VFS) and File Explorer\n";

static const char *default_startup =
    "# AuraOS Bootup Script\n"
    "echo \"Initializing AuraOS graphical desktop...\"\n"
    "sync\n"
    "uptime\n"
    "echo \"System ready.\"\n";

static const char *default_kernel =
    "[KERNEL IMAGE MAP]\n"
    "Physical Base : 0x00010000\n"
    "Stack Base    : 0x001FFFF0 (1MB Ring 0)\n"
    "VRAM Backbuf  : 0x00200000 (3.2MB Double Buffer)\n"
    "Paint Studio  : 0x00600000 (6MB Canvas Buffer)\n"
    "VFS RamFS     : Kernel Data Segment\n";

static const char *default_hardware =
    "[BOOT DIAGNOSTICS]\n"
    "CPU: x86 32-bit Protected Mode (CR0.PE=1, CR0.PG=0)\n"
    "A20 Gate: Fast A20 Enabled (Port 0x92)\n"
    "PIC: Master 0x20-0x27, Slave 0x28-0x2F Remapped\n"
    "PIT: Channel 0 100Hz Square Wave Generator\n"
    "RTC: Motherboard CMOS Clock Synchronized\n"
    "Mouse: Auxiliary PS/2 Sample Rate 200/s\n";

int vfs_create_file(const char *name, const char *content, unsigned int size, unsigned char attr) {
    if (!name || file_count >= VFS_MAX_FILES) return -1;
    if (vfs_find(name)) return -2; // File already exists

    vfs_file_t *f = &files[file_count];
    strncpy(f->name, name, VFS_MAX_FILENAME - 1);
    f->name[VFS_MAX_FILENAME - 1] = '\0';
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

    file_count++;
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
    return 0;
}

int vfs_delete_file(const char *name) {
    for (int i = 0; i < file_count; i++) {
        if (strcmp(files[i].name, name) == 0) {
            if (files[i].attr & FS_ATTR_SYSTEM) return -2; // Cannot delete system file
            // Shift remaining files down
            for (int j = i; j < file_count - 1; j++) {
                files[j] = files[j + 1];
            }
            file_count--;
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

void vfs_init(void) {
    file_count = 0;
    vfs_create_file("README.TXT",   default_readme,   strlen(default_readme),   FS_ATTR_READONLY);
    vfs_create_file("SYSTEM.CFG",   default_cfg,      strlen(default_cfg),      FS_ATTR_SYSTEM);
    vfs_create_file("NOTES.TXT",    default_notes,    strlen(default_notes),    FS_ATTR_USER);
    vfs_create_file("STARTUP.SH",   default_startup,  strlen(default_startup),  FS_ATTR_USER);
    vfs_create_file("KERNEL.SYS",   default_kernel,   strlen(default_kernel),   FS_ATTR_SYSTEM);
    vfs_create_file("HARDWARE.LOG", default_hardware, strlen(default_hardware), FS_ATTR_SYSTEM);
}
