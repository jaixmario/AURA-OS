#include "apps.h"
#include "../kernel.h"
#include "../gfx/gfx.h"
#include "../wm/wm.h"
#include "../arch/ata.h"
#include "../arch/pit.h"
#include "../arch/rtc.h"
#include "../arch/io.h"
#include "../fs/vfs.h"
#include "../libc/string.h"

static window_t *installer_win = 0;
static int current_step = 0; // 0=Welcome, 1=User Identity, 2=Disk Target, 3=Installing, 4=Complete

// User Identity Input Fields
static char in_fullname[32] = "Aura User";
static char in_username[20] = "aura";
static char in_hostname[24] = "aura-pc";
static char in_password[24] = "aura";

static int focused_field = 0; // 0=Fullname, 1=Username, 2=Hostname, 3=Password
static int install_progress = 0; // 0..100
static int install_stage = 0;
static int install_chunk = 0;
static int install_error = 0; // 0=None, 1=Error occurred
static char install_status_msg[80] = "Preparing target disk...";
static char install_err_msg[80] = "";

#define KERNEL_TOTAL_SECTORS 1024
#define KERNEL_CHUNK_SECTORS 128

int app_installer_is_open(void) {
    return (installer_win && installer_win->is_open);
}

static void installer_reset_fields(void) {
    strcpy(in_fullname, "Aura User");
    strcpy(in_username, "aura");
    strcpy(in_hostname, "aura-pc");
    strcpy(in_password, "aura");
    focused_field = 0;
}

static void installer_draw_sidebar(window_t *win, int wx, int wy, int sb_w, int client_h) {
    (void)win;
    gfx_fillrect(wx, wy, sb_w, client_h, RGB(18, 20, 30));
    gfx_draw_line(wx + sb_w, wy, wx + sb_w, wy + client_h - 1, COLOR_BORDER);

    // Sidebar Branding Banner
    gfx_fillrect(wx + 10, wy + 14, sb_w - 20, 38, RGB(28, 30, 46));
    gfx_drawrect(wx + 10, wy + 14, sb_w - 20, 38, COLOR_ACCENT);
    gfx_draw_string(wx + 16, wy + 20, "* AuraOS", COLOR_WHITE, COLOR_TRANSPARENT);
    gfx_draw_string(wx + 16, wy + 34, "OS Installer", COLOR_ACCENT, COLOR_TRANSPARENT);

    // Step indicators
    const char *steps[5] = {
        "1. Welcome",
        "2. User Setup",
        "3. Disk Target",
        "4. Installing",
        "5. Complete"
    };

    for (int i = 0; i < 5; i++) {
        int sy = wy + 72 + (i * 42);
        int is_cur = (current_step == i);
        int is_done = (current_step > i);

        if (is_cur) {
            gfx_fillrect(wx + 8, sy, sb_w - 16, 32, RGB(42, 46, 70));
            gfx_drawrect(wx + 8, sy, sb_w - 16, 32, COLOR_ACCENT);
            gfx_fillrect(wx + 8, sy + 4, 3, 24, COLOR_ACCENT);
        }

        unsigned int fg_col = is_cur ? COLOR_WHITE : (is_done ? COLOR_GREEN : COLOR_TEXT_MUTED);

        if (is_done) {
            gfx_draw_string(wx + 16, sy + 8, "[v]", COLOR_GREEN, COLOR_TRANSPARENT);
            gfx_draw_string(wx + 44, sy + 8, steps[i] + 3, COLOR_TEXT, COLOR_TRANSPARENT);
        } else {
            gfx_draw_string(wx + 16, sy + 8, steps[i], fg_col, COLOR_TRANSPARENT);
        }
    }

    // Media Badge at Bottom of Sidebar
    int badge_y = wy + client_h - 40;
    gfx_fillrect(wx + 10, badge_y, sb_w - 20, 26, RGB(26, 28, 40));
    gfx_drawrect(wx + 10, badge_y, sb_w - 20, 26, COLOR_BORDER);
    gfx_draw_string(wx + 16, badge_y + 6, "[*] Live Media", COLOR_ACCENT, COLOR_TRANSPARENT);
}

static void installer_draw_step_welcome(int cx, int cy, int cw) {
    gfx_draw_string(cx, cy, "Welcome to AuraOS 1.0 Setup", COLOR_WHITE, COLOR_TRANSPARENT);
    gfx_draw_string(cx, cy + 20, "Fast, lightweight 32-bit graphical OS for x86 architecture.", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // Hardware & Environment Info Card
    int card_y = cy + 50;
    int card_h = 175;
    gfx_fillrect(cx, card_y, cw, card_h, RGB(22, 24, 36));
    gfx_drawrect(cx, card_y, cw, card_h, COLOR_BORDER);

    gfx_draw_string(cx + 16, card_y + 12, "Hardware & Installation Environment:", COLOR_ACCENT, COLOR_TRANSPARENT);

    char cap_str[32];
    ata_get_capacity_string(cap_str, sizeof(cap_str));
    int disk_ok = ata_is_available();

    char line1[64], line2[64], line3[64], line4[80], line5[80];
    if (ata_get_atapi_count() > 0) {
        const ata_device_t *opt = ata_get_atapi_device();
        snprintf(line1, sizeof(line1), "- Boot Media    : %s (Live ISO)", opt ? opt->model : "ATAPI CD-ROM");
    } else {
        snprintf(line1, sizeof(line1), "- Boot Mode     : Live CD / USB Bootable Media");
    }
    snprintf(line2, sizeof(line2), "- Architecture  : i686 32-bit Protected Mode");
    snprintf(line3, sizeof(line3), "- Video Engine  : VESA VBE Multi-Resolution TrueColor");
    snprintf(line4, sizeof(line4), "- Target Storage: %s", ata_get_location_string());
    if (disk_ok) {
        int d_count = ata_get_drive_count();
        if (d_count > 1) {
            snprintf(line5, sizeof(line5), "- Detected Disk : %s (%s) [%d drives available]", ata_get_model(), cap_str, d_count);
        } else {
            snprintf(line5, sizeof(line5), "- Detected Disk : %s (%s)", ata_get_model(), cap_str);
        }
    } else {
        snprintf(line5, sizeof(line5), "- Detected Disk : [!] No ATA/IDE Hard Disk detected");
    }

    gfx_draw_string_clipped(cx + 16, card_y + 36, line1, COLOR_WHITE, COLOR_TRANSPARENT, cw - 32);
    gfx_draw_string_clipped(cx + 16, card_y + 60, line2, COLOR_WHITE, COLOR_TRANSPARENT, cw - 32);
    gfx_draw_string_clipped(cx + 16, card_y + 84, line3, COLOR_WHITE, COLOR_TRANSPARENT, cw - 32);
    gfx_draw_string_clipped(cx + 16, card_y + 108, line4, COLOR_WHITE, COLOR_TRANSPARENT, cw - 32);
    gfx_draw_string_clipped(cx + 16, card_y + 132, line5, disk_ok ? COLOR_GREEN : COLOR_RED, COLOR_TRANSPARENT, cw - 32);

    if (!disk_ok) {
        gfx_draw_string(cx, card_y + card_h + 12, "[!] Please attach an IDE virtual disk or enable IDE mode in BIOS.", COLOR_RED, COLOR_TRANSPARENT);
    } else {
        gfx_draw_string(cx, card_y + card_h + 12, "Click 'Next' to set up your username, computer name, and password.", COLOR_TEXT, COLOR_TRANSPARENT);
    }

    // Bottom Navigation Buttons
    int btn_y = card_y + card_h + 44;

    // Button: [ Quit / Live Desktop ]
    gfx_fillrect(cx, btn_y, 140, 32, RGB(36, 40, 58));
    gfx_drawrect(cx, btn_y, 140, 32, COLOR_BORDER);
    gfx_draw_string(cx + 16, btn_y + 8, "Close / Live", COLOR_TEXT, COLOR_TRANSPARENT);

    // Button: [ Next Step > ]
    if (disk_ok) {
        gfx_fillrect(cx + cw - 130, btn_y, 130, 32, COLOR_ACCENT);
        gfx_drawrect(cx + cw - 130, btn_y, 130, 32, COLOR_WHITE);
        gfx_draw_string(cx + cw - 116, btn_y + 8, "Next Step >", RGB(17, 17, 27), COLOR_TRANSPARENT);
    } else {
        gfx_fillrect(cx + cw - 130, btn_y, 130, 32, RGB(40, 42, 54));
        gfx_drawrect(cx + cw - 130, btn_y, 130, 32, COLOR_BORDER);
        gfx_draw_string(cx + cw - 116, btn_y + 8, "Next Step >", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    }
}

static void installer_draw_step_user(int cx, int cy, int cw) {
    gfx_draw_string(cx, cy, "User Setup & Credentials", COLOR_WHITE, COLOR_TRANSPARENT);
    gfx_draw_string(cx, cy + 18, "Configure your user identity and system computer name:", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    const char *labels[4] = {
        "Your Full Name:",
        "Username (login & shell):",
        "Computer Name (hostname):",
        "Password:"
    };

    const char *values[4] = {
        in_fullname,
        in_username,
        in_hostname,
        in_password
    };

    int input_y = cy + 44;
    int box_w = cw - 30;
    int box_h = 28;

    for (int i = 0; i < 4; i++) {
        int iy = input_y + (i * 48);
        int is_foc = (focused_field == i);

        gfx_draw_string(cx, iy, labels[i], is_foc ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);

        int by = iy + 16;
        gfx_fillrect(cx, by, box_w, box_h, is_foc ? RGB(16, 18, 28) : RGB(24, 26, 38));
        gfx_drawrect(cx, by, box_w, box_h, is_foc ? COLOR_ACCENT : COLOR_BORDER);

        if (i == 3) {
            int plen = strlen(in_password);
            char mask[32];
            for (int p = 0; p < plen && p < 30; p++) mask[p] = '*';
            mask[plen] = '\0';
            gfx_draw_string(cx + 8, by + 6, mask, COLOR_WHITE, COLOR_TRANSPARENT);
            if (is_foc && (pit_get_uptime_seconds() % 2 == 0)) {
                gfx_draw_string(cx + 8 + (plen * 8), by + 6, "|", COLOR_ACCENT, COLOR_TRANSPARENT);
            }
        } else {
            gfx_draw_string(cx + 8, by + 6, values[i], COLOR_WHITE, COLOR_TRANSPARENT);
            int vlen = strlen(values[i]);
            if (is_foc && (pit_get_uptime_seconds() % 2 == 0)) {
                gfx_draw_string(cx + 8 + (vlen * 8), by + 6, "|", COLOR_ACCENT, COLOR_TRANSPARENT);
            }
        }
    }

    // Quick Defaults Button
    int def_y = input_y + 196;
    gfx_fillrect(cx, def_y, 190, 26, RGB(34, 38, 56));
    gfx_drawrect(cx, def_y, 190, 26, COLOR_BORDER);
    gfx_draw_string(cx + 10, def_y + 5, "Use Defaults (aura)", COLOR_TEXT, COLOR_TRANSPARENT);

    // Bottom Navigation Buttons
    int btn_y = def_y + 38;

    // [ < Back ]
    gfx_fillrect(cx, btn_y, 100, 32, RGB(36, 40, 58));
    gfx_drawrect(cx, btn_y, 100, 32, COLOR_BORDER);
    gfx_draw_string(cx + 20, btn_y + 8, "< Back", COLOR_TEXT, COLOR_TRANSPARENT);

    // [ Next Step > ]
    gfx_fillrect(cx + cw - 130, btn_y, 130, 32, COLOR_ACCENT);
    gfx_drawrect(cx + cw - 130, btn_y, 130, 32, COLOR_WHITE);
    gfx_draw_string(cx + cw - 116, btn_y + 8, "Next Step >", RGB(17, 17, 27), COLOR_TRANSPARENT);
}

static void installer_draw_step_disk(int cx, int cy, int cw) {
    gfx_draw_string(cx, cy, "Installation Destination & Partitioning", COLOR_WHITE, COLOR_TRANSPARENT);
    gfx_draw_string(cx, cy + 18, "Review target disk drive and partition scheme:", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    char cap_str[32];
    ata_get_capacity_string(cap_str, sizeof(cap_str));

    int card_y = cy + 44;
    int card_h = 135;
    gfx_fillrect(cx, card_y, cw, card_h, RGB(22, 24, 36));
    gfx_drawrect(cx, card_y, cw, card_h, COLOR_ACCENT);

    if (!ata_is_available()) {
        gfx_draw_string(cx + 14, card_y + 10, "Target Hard Disk: [!] NO ATA/IDE DISK FOUND", COLOR_RED, COLOR_TRANSPARENT);
        gfx_draw_string(cx + 14, card_y + 32, "VMware: In VM Settings, ensure Virtual Disk is set to IDE.", COLOR_YELLOW, COLOR_TRANSPARENT);
        gfx_draw_string(cx + 14, card_y + 54, "VirtualBox / PC: Set SATA Controller Mode to IDE / Legacy.", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
        gfx_draw_string(cx + 14, card_y + 76, "Please attach an IDE hard disk and restart setup.", COLOR_TEXT, COLOR_TRANSPARENT);
        gfx_draw_string(cx + 14, card_y + 98, "Target: All IDE Ports Probed (0x1F0 / 0x170)", COLOR_ACCENT, COLOR_TRANSPARENT);
    } else {
        char l1[80], l2[80], l3[80], l4[80], l5[80];
        snprintf(l1, sizeof(l1), "- Model       : %s", ata_get_model());
        snprintf(l2, sizeof(l2), "- Capacity    : %s (%u sectors)", cap_str, ata_get_total_sectors());
        snprintf(l3, sizeof(l3), "- Bus Location: %s", ata_get_location_string());
        snprintf(l4, sizeof(l4), "- MBR Boot    : Sector 0 (Active FAT16 System Partition)");
        snprintf(l5, sizeof(l5), "- VFS Storage : LBA 1000 (Superblock) & Clusters");

        gfx_draw_string_clipped(cx + 14, card_y + 10, l1, COLOR_WHITE, COLOR_TRANSPARENT, cw - 28);
        gfx_draw_string_clipped(cx + 14, card_y + 32, l2, COLOR_TEXT, COLOR_TRANSPARENT, cw - 28);
        gfx_draw_string_clipped(cx + 14, card_y + 54, l3, COLOR_ACCENT, COLOR_TRANSPARENT, cw - 28);
        gfx_draw_string_clipped(cx + 14, card_y + 76, l4, COLOR_TEXT, COLOR_TRANSPARENT, cw - 28);
        gfx_draw_string_clipped(cx + 14, card_y + 98, l5, COLOR_TEXT, COLOR_TRANSPARENT, cw - 28);

        // If multiple drives found, provide toggle button
        if (ata_get_drive_count() > 1) {
            gfx_fillrect(cx + cw - 160, card_y + 10, 146, 26, RGB(38, 42, 60));
            gfx_drawrect(cx + cw - 160, card_y + 10, 146, 26, COLOR_ACCENT);
            gfx_draw_string(cx + cw - 150, card_y + 15, "Switch Drive >", COLOR_WHITE, COLOR_TRANSPARENT);
        }
    }

    // Selected Installation Mode Radio Box
    int radio_y = card_y + card_h + 14;
    gfx_fillrect(cx, radio_y, cw, 44, RGB(28, 30, 46));
    gfx_drawrect(cx, radio_y, cw, 44, COLOR_BORDER);

    gfx_fill_circle(cx + 18, radio_y + 22, 6, COLOR_ACCENT);
    gfx_fill_circle(cx + 18, radio_y + 22, 2, RGB(17, 17, 27));

    gfx_draw_string(cx + 34, radio_y + 8, "Erase disk and install clean AuraOS", COLOR_WHITE, COLOR_TRANSPARENT);
    gfx_draw_string(cx + 34, radio_y + 24, "Format MBR, write 512KB kernel, and configure persistent files.", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    gfx_draw_string(cx, radio_y + 52, "[!] Target disk will be formatted and installed with AuraOS.", COLOR_RED, COLOR_TRANSPARENT);

    // Navigation buttons
    int btn_y = radio_y + 76;

    // [ < Back ]
    gfx_fillrect(cx, btn_y, 100, 32, RGB(36, 40, 58));
    gfx_drawrect(cx, btn_y, 100, 32, COLOR_BORDER);
    gfx_draw_string(cx + 20, btn_y + 8, "< Back", COLOR_TEXT, COLOR_TRANSPARENT);

    // [ Install Now > ]
    unsigned int btn_col = ata_is_available() ? RGB(40, 167, 69) : RGB(70, 70, 80);
    gfx_fillrect(cx + cw - 160, btn_y, 160, 32, btn_col);
    gfx_drawrect(cx + cw - 160, btn_y, 160, 32, ata_is_available() ? COLOR_WHITE : COLOR_BORDER);
    gfx_draw_string(cx + cw - 146, btn_y + 8, "Install AuraOS >", ata_is_available() ? COLOR_WHITE : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
}

static void installer_draw_step_installing(int cx, int cy, int cw) {
    if (install_error) {
        gfx_draw_string(cx, cy, "Installation Failed! [!]", COLOR_RED, COLOR_TRANSPARENT);
        gfx_draw_string(cx, cy + 18, "An error occurred during hard disk operations:", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

        int err_y = cy + 50;
        int err_h = 130;
        gfx_fillrect(cx, err_y, cw, err_h, RGB(38, 20, 24));
        gfx_drawrect(cx, err_y, cw, err_h, COLOR_RED);

        gfx_draw_string(cx + 16, err_y + 14, "Error Details:", COLOR_WHITE, COLOR_TRANSPARENT);
        gfx_draw_string(cx + 16, err_y + 36, install_err_msg, COLOR_RED, COLOR_TRANSPARENT);
        gfx_draw_string(cx + 16, err_y + 60, "- Ensure your virtual disk controller is configured as IDE.", COLOR_TEXT, COLOR_TRANSPARENT);
        gfx_draw_string(cx + 16, err_y + 80, "- Check that the disk image is writable.", COLOR_TEXT, COLOR_TRANSPARENT);

        int btn_y = err_y + err_h + 30;

        // [ < Back to Selection ]
        gfx_fillrect(cx, btn_y, 180, 32, RGB(36, 40, 58));
        gfx_drawrect(cx, btn_y, 180, 32, COLOR_BORDER);
        gfx_draw_string(cx + 16, btn_y + 8, "< Disk Selection", COLOR_TEXT, COLOR_TRANSPARENT);

        // [ Retry Installation ]
        gfx_fillrect(cx + cw - 150, btn_y, 150, 32, RGB(218, 56, 70));
        gfx_drawrect(cx + cw - 150, btn_y, 150, 32, COLOR_WHITE);
        gfx_draw_string(cx + cw - 130, btn_y + 8, "Retry Setup", COLOR_WHITE, COLOR_TRANSPARENT);
        return;
    }

    gfx_draw_string(cx, cy, "Installing AuraOS 1.0...", COLOR_WHITE, COLOR_TRANSPARENT);
    gfx_draw_string(cx, cy + 18, "Writing system binaries and user environment to hard disk.", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // Progress Bar
    int bar_y = cy + 60;
    int bar_h = 24;
    gfx_fillrect(cx, bar_y, cw, bar_h, RGB(20, 22, 32));
    gfx_drawrect(cx, bar_y, cw, bar_h, COLOR_BORDER);

    int fill_w = (cw * install_progress) / 100;
    if (fill_w > 0) {
        gfx_gradient_h(cx + 1, bar_y + 1, fill_w - 2, bar_h - 2, COLOR_ACTIVE_HEADER, COLOR_ACCENT);
    }

    // Progress percentage
    char pct_buf[16];
    snprintf(pct_buf, sizeof(pct_buf), "%d%%", install_progress);
    gfx_draw_string(cx + cw / 2 - 12, bar_y + 4, pct_buf, COLOR_WHITE, COLOR_TRANSPARENT);

    // Current Action Message Card
    int log_y = bar_y + 40;
    gfx_fillrect(cx, log_y, cw, 60, RGB(24, 26, 38));
    gfx_drawrect(cx, log_y, cw, 60, COLOR_BORDER);

    gfx_draw_string(cx + 12, log_y + 10, "Status:", COLOR_ACCENT, COLOR_TRANSPARENT);
    gfx_draw_string(cx + 12, log_y + 30, install_status_msg, COLOR_WHITE, COLOR_TRANSPARENT);

    // Step bullets
    int bul_y = log_y + 76;
    gfx_draw_string(cx, bul_y + 0,  (install_stage >= 1) ? "[v] MBR Bootloader written (LBA 0)" : "[-] Writing MBR Bootloader...",
                    (install_stage >= 1) ? COLOR_GREEN : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_draw_string(cx, bul_y + 20, (install_stage >= 2) ? "[v] Protected Mode Kernel installed (512 KB)" : "[-] Copying Kernel Image...",
                    (install_stage >= 2) ? COLOR_GREEN : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_draw_string(cx, bul_y + 40, (install_stage >= 3) ? "[v] AuraOS Superblock & VFS ready" : "[-] Initializing File System...",
                    (install_stage >= 3) ? COLOR_GREEN : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_draw_string(cx, bul_y + 60, (install_stage >= 4) ? "[v] User configuration & profile set" : "[-] Creating User Profile...",
                    (install_stage >= 4) ? COLOR_GREEN : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
}

static void installer_draw_step_complete(int cx, int cy, int cw) {
    gfx_draw_string(cx, cy, "Installation Complete! ✦", COLOR_GREEN, COLOR_TRANSPARENT);
    gfx_draw_string(cx, cy + 18, "AuraOS has been successfully installed on your hard drive.", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // Summary Card
    int card_y = cy + 44;
    int card_h = 160;
    gfx_fillrect(cx, card_y, cw, card_h, RGB(22, 26, 38));
    gfx_drawrect(cx, card_y, cw, card_h, COLOR_GREEN);

    gfx_draw_string(cx + 16, card_y + 12, "Installation Details:", COLOR_WHITE, COLOR_TRANSPARENT);

    char l1[64], l2[64], l3[80], l4[64], l5[64];
    snprintf(l1, sizeof(l1), "- User Name     : %s (%s)", in_fullname, in_username);
    snprintf(l2, sizeof(l2), "- Computer Name : %s", in_hostname);
    snprintf(l3, sizeof(l3), "- Boot Target   : %s", ata_get_location_string());
    snprintf(l4, sizeof(l4), "- Storage State : Installed & Persistent (VFS)");
    snprintf(l5, sizeof(l5), "- Next Step     : Eject ISO/USB media and boot from disk");

    gfx_draw_string_clipped(cx + 16, card_y + 36, l1, COLOR_WHITE, COLOR_TRANSPARENT, cw - 32);
    gfx_draw_string_clipped(cx + 16, card_y + 60, l2, COLOR_WHITE, COLOR_TRANSPARENT, cw - 32);
    gfx_draw_string_clipped(cx + 16, card_y + 84, l3, COLOR_WHITE, COLOR_TRANSPARENT, cw - 32);
    gfx_draw_string_clipped(cx + 16, card_y + 108, l4, COLOR_GREEN, COLOR_TRANSPARENT, cw - 32);
    gfx_draw_string_clipped(cx + 16, card_y + 132, l5, COLOR_ACCENT, COLOR_TRANSPARENT, cw - 32);

    // Bottom Action Buttons
    int btn_y = card_y + card_h + 30;

    // [ Continue Testing Live ]
    gfx_fillrect(cx, btn_y, 160, 32, RGB(36, 40, 58));
    gfx_drawrect(cx, btn_y, 160, 32, COLOR_BORDER);
    gfx_draw_string(cx + 12, btn_y + 8, "Close / Desktop", COLOR_TEXT, COLOR_TRANSPARENT);

    // [ Reboot System ]
    gfx_fillrect(cx + cw - 140, btn_y, 140, 32, RGB(218, 56, 70));
    gfx_drawrect(cx + cw - 140, btn_y, 140, 32, COLOR_WHITE);
    gfx_draw_string(cx + cw - 124, btn_y + 8, "Reboot Now", COLOR_WHITE, COLOR_TRANSPARENT);
}

// Staged non-blocking installer step engine: advances one sub-task per render frame
static void installer_process_step(void) {
    if (install_error) return;

    if (!ata_is_available()) {
        install_error = 1;
        strncpy(install_err_msg, "No ATA/IDE hard disk found on system!", sizeof(install_err_msg));
        return;
    }

    if (install_stage == 0) {
        // Stage 0: Initial verification
        install_stage = 1;
        install_progress = 15;
        strncpy(install_status_msg, "Writing MBR bootloader to Sector 0...", sizeof(install_status_msg));
        return;
    }

    if (install_stage == 1) {
        // Stage 1: Write Sector 0 MBR
        unsigned char mbr_buf[512];
        memcpy(mbr_buf, (const void *)0x7C00, 512);

        // Ensure DAP starting LBA is set to 1 for installed hard disk
        for (int i = 0; i < 400; i++) {
            if (mbr_buf[i] == 0x10 && mbr_buf[i+1] == 0x00 && mbr_buf[i+2] == 0x40 && mbr_buf[i+3] == 0x00) {
                unsigned int lba_low = 1;
                unsigned int lba_high = 0;
                memcpy(&mbr_buf[i + 8], &lba_low, 4);
                memcpy(&mbr_buf[i + 12], &lba_high, 4);
                break;
            }
        }
        // Force is_live_default = 0 (offset 391) so hard disk boots as installed
        mbr_buf[391] = 0;
        mbr_buf[510] = 0x55;
        mbr_buf[511] = 0xAA;

        int res = ata_write_sector(0, mbr_buf);
        if (res != 0) {
            install_error = 1;
            snprintf(install_err_msg, sizeof(install_err_msg), "Failed to write Sector 0 MBR (code %d)", res);
            return;
        }
        ata_flush_cache();

        install_stage = 2;
        install_chunk = 0;
        install_progress = 25;
        strncpy(install_status_msg, "Copying Protected Mode Kernel (512 KB)...", sizeof(install_status_msg));
        return;
    }

    if (install_stage == 2) {
        // Stage 2: Stream kernel in chunks of 128 sectors (64 KB) across frames
        int lba = 1 + (install_chunk * KERNEL_CHUNK_SECTORS);
        const void *src = (const void *)(0x10000 + (install_chunk * KERNEL_CHUNK_SECTORS * 512));

        int res = ata_write_sectors(lba, KERNEL_CHUNK_SECTORS, src);
        if (res != 0) {
            install_error = 1;
            snprintf(install_err_msg, sizeof(install_err_msg), "Failed writing Kernel at LBA %d (code %d)", lba, res);
            return;
        }

        install_chunk++;
        install_progress = 25 + (install_chunk * 5); // 30%..65%
        snprintf(install_status_msg, sizeof(install_status_msg), "Writing Kernel chunk %d of 8 (%d KB / 512 KB)...",
                 install_chunk, install_chunk * 64);

        if (install_chunk >= (KERNEL_TOTAL_SECTORS / KERNEL_CHUNK_SECTORS)) {
            install_stage = 3;
            install_progress = 70;
            strncpy(install_status_msg, "Configuring user credentials and system files...", sizeof(install_status_msg));
        }
        return;
    }

    if (install_stage == 3) {
        // Stage 3: Prepare User Configuration & System Profile in VFS
        char user_cfg[300];
        rtc_time_t t;
        rtc_get_datetime(&t);
        snprintf(user_cfg, sizeof(user_cfg),
                 "[USER]\nNAME=%s\nUSERNAME=%s\nHOSTNAME=%s\nPASSWORD=%s\nINSTALLED=1\nINSTALL_DATE=%04u-%02u-%02u\n",
                 in_fullname, in_username, in_hostname, in_password, t.year, t.month, t.day);

        if (vfs_find("USER.CFG")) {
            vfs_write_file("USER.CFG", user_cfg, strlen(user_cfg));
        } else {
            vfs_create_file("USER.CFG", "System", user_cfg, strlen(user_cfg), FS_ATTR_SYSTEM);
        }

        char sys_cfg[400];
        snprintf(sys_cfg, sizeof(sys_cfg),
                 "# AuraOS Desktop Configuration\n[STORAGE]\nDRIVER=ATA_PIO\nPRIMARY_BUS=%s\nLBA_OFFSET=1000\nINSTALLED=TRUE\n[SYSTEM]\nHOSTNAME=%s\nUSER=%s\n[DESKTOP]\nWALLPAPER=0\nTHEME=0\nMOUSE_SPEED=1\n",
                 ata_get_location_string(), in_hostname, in_username);

        if (vfs_find("SYSTEM.CFG")) {
            vfs_write_file("SYSTEM.CFG", sys_cfg, strlen(sys_cfg));
        } else {
            vfs_create_file("SYSTEM.CFG", "System", sys_cfg, strlen(sys_cfg), FS_ATTR_SYSTEM);
        }

        char startup_sh[200];
        snprintf(startup_sh, sizeof(startup_sh),
                 "# AuraOS Bootup Script\necho \"Welcome %s to AuraOS on %s!\"\nsync\nuptime\n",
                 in_username, in_hostname);

        if (vfs_find("STARTUP.SH")) {
            vfs_write_file("STARTUP.SH", startup_sh, strlen(startup_sh));
        } else {
            vfs_create_file("STARTUP.SH", "Documents", startup_sh, strlen(startup_sh), FS_ATTR_USER);
        }

        install_stage = 4;
        install_progress = 85;
        strncpy(install_status_msg, "Writing AuraOS Superblock and syncing VFS...", sizeof(install_status_msg));
        return;
    }

    if (install_stage == 4) {
        // Stage 4: Sync Superblock and Flush disk cache
        vfs_sync_disk();
        ata_flush_cache();

        install_stage = 5;
        install_progress = 100;
        strncpy(install_status_msg, "Installation complete!", sizeof(install_status_msg));
        return;
    }

    if (install_stage == 5) {
        // Stage 5: Commit live system state and transition to Complete screen
        sys_set_user_info(in_fullname, in_username, in_hostname, in_password);
        sys_set_installed(1);
        current_step = 4;
    }
}

static void installer_draw(window_t *win) {
    int wx = win->x;
    int wy = win->y + TITLEBAR_HEIGHT;
    int client_w = win->width;
    int client_h = win->height - TITLEBAR_HEIGHT;

    int sb_w = 160;

    // Sidebar
    installer_draw_sidebar(win, wx, wy, sb_w, client_h);

    // Content area
    int cx = wx + sb_w + 20;
    int cy = wy + 20;
    int cw = client_w - sb_w - 40;

    if (current_step == 3) {
        installer_draw_step_installing(cx, cy, cw);
        installer_process_step();
        return;
    }

    if (current_step == 0) {
        installer_draw_step_welcome(cx, cy, cw);
    } else if (current_step == 1) {
        installer_draw_step_user(cx, cy, cw);
    } else if (current_step == 2) {
        installer_draw_step_disk(cx, cy, cw);
    } else if (current_step == 4) {
        installer_draw_step_complete(cx, cy, cw);
    }
}

static void installer_on_click(window_t *win, int cx, int cy, int btn) {
    (void)btn;
    int sb_w = 160;
    int content_x = sb_w + 20;
    int content_w = win->width - sb_w - 40;

    if (current_step == 0) {
        // Step 0: Welcome
        int btn_y = 20 + 50 + 175 + 44;
        // Close button
        if (cx >= content_x && cx <= content_x + 140 && cy >= btn_y && cy <= btn_y + 32) {
            wm_close_window(win);
            return;
        }
        // Next button
        if (ata_is_available() && cx >= content_x + content_w - 130 && cx <= content_x + content_w &&
            cy >= btn_y && cy <= btn_y + 32) {
            current_step = 1;
            focused_field = 0;
            return;
        }
    } else if (current_step == 1) {
        // Step 1: User Identity
        int input_y = 20 + 44;
        int box_w = content_w - 30;
        int box_h = 28;

        for (int i = 0; i < 4; i++) {
            int iy = input_y + (i * 48);
            int by = iy + 16;
            if (cx >= content_x && cx <= content_x + box_w && cy >= by && cy <= by + box_h) {
                focused_field = i;
                return;
            }
        }

        // Defaults button
        int def_y = input_y + 196;
        if (cx >= content_x && cx <= content_x + 190 && cy >= def_y && cy <= def_y + 26) {
            installer_reset_fields();
            return;
        }

        // Back button
        int btn_y = def_y + 38;
        if (cx >= content_x && cx <= content_x + 100 && cy >= btn_y && cy <= btn_y + 32) {
            current_step = 0;
            return;
        }

        // Next button
        if (cx >= content_x + content_w - 130 && cx <= content_x + content_w &&
            cy >= btn_y && cy <= btn_y + 32) {
            current_step = 2;
            return;
        }
    } else if (current_step == 2) {
        // Step 2: Disk Target
        int card_y = 20 + 44;
        int card_h = 135;

        // Switch drive button (if >1 drive detected)
        if (ata_get_drive_count() > 1) {
            if (cx >= content_x + content_w - 160 && cx <= content_x + content_w - 14 &&
                cy >= card_y + 10 && cy <= card_y + 36) {
                int cur = ata_get_active_drive();
                for (int next = 1; next < 4; next++) {
                    int cand = (cur + next) % 4;
                    const ata_device_t *d = ata_get_device(cand);
                    if (d && d->present && !d->is_atapi) {
                        ata_select_drive(cand);
                        break;
                    }
                }
                return;
            }
        }

        int radio_y = card_y + card_h + 14;
        int btn_y = radio_y + 76;

        // Back button
        if (cx >= content_x && cx <= content_x + 100 && cy >= btn_y && cy <= btn_y + 32) {
            current_step = 1;
            return;
        }

        // Install button
        if (cx >= content_x + content_w - 160 && cx <= content_x + content_w &&
            cy >= btn_y && cy <= btn_y + 32) {
            if (!ata_is_available()) return;
            current_step = 3;
            install_progress = 5;
            install_stage = 0;
            install_chunk = 0;
            install_error = 0;
            install_err_msg[0] = '\0';
            strncpy(install_status_msg, "Initializing target hard disk...", sizeof(install_status_msg));
            return;
        }
    } else if (current_step == 3) {
        // Step 3 error buttons
        if (install_error) {
            int err_y = 20 + 50;
            int err_h = 130;
            int btn_y = err_y + err_h + 30;

            // [ < Disk Selection ]
            if (cx >= content_x && cx <= content_x + 180 && cy >= btn_y && cy <= btn_y + 32) {
                current_step = 2;
                install_error = 0;
                return;
            }

            // [ Retry Setup ]
            if (cx >= content_x + content_w - 150 && cx <= content_x + content_w &&
                cy >= btn_y && cy <= btn_y + 32) {
                install_error = 0;
                install_stage = 0;
                install_chunk = 0;
                install_progress = 5;
                strncpy(install_status_msg, "Retrying installation...", sizeof(install_status_msg));
                return;
            }
        }
    } else if (current_step == 4) {
        // Step 4: Complete
        int card_y = 20 + 44;
        int card_h = 160;
        int btn_y = card_y + card_h + 30;

        // Close / Desktop
        if (cx >= content_x && cx <= content_x + 160 && cy >= btn_y && cy <= btn_y + 32) {
            wm_close_window(win);
            return;
        }

        // Reboot Now
        if (cx >= content_x + content_w - 140 && cx <= content_x + content_w &&
            cy >= btn_y && cy <= btn_y + 32) {
            sys_reboot();
            return;
        }
    }
}

static void installer_on_key(window_t *win, char key) {
    if (key == 27) { // ESC closes installer
        wm_close_window(win);
        return;
    }

    if (current_step == 0) {
        if ((key == '\n' || key == '\r' || key == ' ' || key == 'n' || key == 'N') && ata_is_available()) {
            current_step = 1;
            focused_field = 0;
        }
        return;
    }

    if (current_step == 1) {
        char *target = 0;
        int max_len = 0;

        if (focused_field == 0) {
            target = in_fullname;
            max_len = sizeof(in_fullname) - 1;
        } else if (focused_field == 1) {
            target = in_username;
            max_len = sizeof(in_username) - 1;
        } else if (focused_field == 2) {
            target = in_hostname;
            max_len = sizeof(in_hostname) - 1;
        } else if (focused_field == 3) {
            target = in_password;
            max_len = sizeof(in_password) - 1;
        }

        if (!target) return;
        int len = strlen(target);

        if (key == '\b') {
            if (len > 0) {
                target[len - 1] = '\0';
            }
        } else if (key == '\t') {
            focused_field = (focused_field + 1) % 4;
        } else if (key == '\n' || key == '\r') {
            current_step = 2; // Advance to disk target step
        } else if (key >= 32 && key <= 126) {
            if (len < max_len) {
                target[len] = key;
                target[len + 1] = '\0';
            }
        }
        return;
    }

    if (current_step == 2) {
        if (key == '\n' || key == '\r' || key == 'i' || key == 'I') {
            if (!ata_is_available()) return;
            current_step = 3;
            install_progress = 5;
            install_stage = 0;
            install_chunk = 0;
            install_error = 0;
            install_err_msg[0] = '\0';
            strncpy(install_status_msg, "Starting disk installation...", sizeof(install_status_msg));
        } else if (key == 'b' || key == 'B') {
            current_step = 1;
        } else if ((key == 's' || key == 'S' || key == '\t') && ata_get_drive_count() > 1) {
            int cur = ata_get_active_drive();
            for (int next = 1; next < 4; next++) {
                int cand = (cur + next) % 4;
                const ata_device_t *d = ata_get_device(cand);
                if (d && d->present && !d->is_atapi) {
                    ata_select_drive(cand);
                    break;
                }
            }
        }
        return;
    }

    if (current_step == 3 && install_error) {
        if (key == 'r' || key == 'R' || key == '\n' || key == '\r') {
            install_error = 0;
            install_stage = 0;
            install_chunk = 0;
            install_progress = 5;
            strncpy(install_status_msg, "Retrying installation...", sizeof(install_status_msg));
        } else if (key == 'b' || key == 'B') {
            current_step = 2;
            install_error = 0;
        }
        return;
    }

    if (current_step == 4) {
        if (key == '\n' || key == '\r' || key == 'r' || key == 'R') {
            sys_reboot();
        } else if (key == 'c' || key == 'C') {
            wm_close_window(win);
        }
        return;
    }
}

void app_installer_launch(void) {
    if (!ata_is_available()) {
        ata_init();
    }

    if (installer_win && installer_win->is_open) {
        wm_focus_window(installer_win);
        return;
    }

    current_step = 0;
    install_progress = 0;
    install_stage = 0;
    install_chunk = 0;
    install_error = 0;
    install_err_msg[0] = '\0';

    int win_w = 680;
    int win_h = 460;
    int win_x = (gfx_get_width() - win_w) / 2;
    int win_y = (gfx_get_height() - 48 - win_h) / 2;
    if (win_x < 10) win_x = 10;
    if (win_y < 10) win_y = 10;

    installer_win = wm_create_window("✦ Install AuraOS 1.0 - Setup Wizard", win_x, win_y, win_w, win_h, RGB(24, 26, 38));
    if (installer_win) {
        installer_win->draw_client = installer_draw;
        installer_win->on_click = installer_on_click;
        installer_win->on_key = installer_on_key;
        wm_focus_window(installer_win);
    }
}
