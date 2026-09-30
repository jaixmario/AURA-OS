#include "display.h"
#include "io.h"
#include "mouse.h"
#include "ata.h"
#include "../kernel.h"
#include "../fs/vfs.h"
#include "../gfx/gfx.h"
#include "../gfx/wallpaper.h"
#include "../wm/wm.h"
#include "../libc/string.h"

#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF

#define VBE_DISPI_INDEX_ID     0x0
#define VBE_DISPI_INDEX_XRES   0x1
#define VBE_DISPI_INDEX_YRES   0x2
#define VBE_DISPI_INDEX_BPP    0x3
#define VBE_DISPI_INDEX_ENABLE 0x4

#define VBE_DISPI_DISABLED     0x00
#define VBE_DISPI_ENABLED      0x01
#define VBE_DISPI_LFB_ENABLED  0x40
#define VBE_DISPI_NOCLEARMEM   0x80

static const display_mode_t g_modes[] = {
    { 1920, 1080, "1920 x 1080", "16:9",  "Full HD 1080p (Widescreen)" },
    { 1600, 900,  "1600 x 900",  "16:9",  "HD+ High Definition (16:9)" },
    { 1366, 768,  "1366 x 768",  "16:9",  "Standard Laptop Widescreen" },
    { 1280, 1024, "1280 x 1024", "5:4",   "SXGA Traditional Desktop" },
    { 1280, 800,  "1280 x 800",  "16:10", "WXGA 16:10 Widescreen" },
    { 1280, 720,  "1280 x 720",  "16:9",  "720p HD Widescreen" },
    { 1024, 768,  "1024 x 768",  "4:3",   "XGA Standard Desktop" },
    { 800,  600,  "800 x 600",   "4:3",   "SVGA Safe Mode" }
};
#define MODE_COUNT (int)(sizeof(g_modes) / sizeof(g_modes[0]))

static int g_bga_detected = 0;
static int g_vmware_detected = 0;
static unsigned short g_vmware_io_base = 0;

static unsigned int pci_read32(unsigned char bus, unsigned char slot, unsigned char func, unsigned char offset) {
    unsigned int address = (1U << 31) | ((unsigned int)bus << 16) | ((unsigned int)slot << 11) | ((unsigned int)func << 8) | (offset & 0xFC);
    outl(0x0CF8, address);
    return inl(0x0CFC);
}

static void vmware_write(unsigned int index, unsigned int val) {
    outl(g_vmware_io_base + 0, index);
    outl(g_vmware_io_base + 1, val);
}

static unsigned int vmware_read(unsigned int index) {
    outl(g_vmware_io_base + 0, index);
    return inl(g_vmware_io_base + 1);
}

static void bga_write(unsigned short index, unsigned short val) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, val);
}

static unsigned short bga_read(unsigned short index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

void display_init(void) {
    g_bga_detected = 0;
    g_vmware_detected = 0;
    g_vmware_io_base = 0;

    // 1. Probe VMware SVGA-II device on PCI bus (Vendor 0x15AD, Dev 0x0405 or 0x0710)
    for (unsigned char bus = 0; bus < 5; bus++) {
        for (unsigned char slot = 0; slot < 32; slot++) {
            unsigned int id = pci_read32(bus, slot, 0, 0);
            unsigned short vendor = id & 0xFFFF;
            unsigned short dev = (id >> 16) & 0xFFFF;
            if (vendor == 0x15AD && (dev == 0x0405 || dev == 0x0710)) {
                unsigned int bar0 = pci_read32(bus, slot, 0, 0x10);
                if (bar0 & 1) {
                    g_vmware_io_base = (unsigned short)(bar0 & ~0x3);
                    // Negotiate SVGA version 2 ID: 0x90000002
                    vmware_write(0, 0x90000002);
                    if (vmware_read(0) == 0x90000002) {
                        g_vmware_detected = 1;
                        break;
                    }
                }
            }
        }
        if (g_vmware_detected) break;
    }

    // 2. Probe Bochs/QEMU BGA version register (0x01CE/0x01CF)
    if (!g_vmware_detected) {
        unsigned short id = bga_read(VBE_DISPI_INDEX_ID);
        if (id >= 0xB0C0 && id <= 0xB0C6) {
            bga_write(VBE_DISPI_INDEX_ID, 0xB0C5);
            if (bga_read(VBE_DISPI_INDEX_ID) == 0xB0C5) {
                g_bga_detected = 1;
            }
        }
    }
}

int display_get_mode_count(void) {
    return MODE_COUNT;
}

const display_mode_t *display_get_mode(int index) {
    if (index < 0 || index >= MODE_COUNT) return 0;
    return &g_modes[index];
}

int display_get_current_mode_index(void) {
    int cur_w = gfx_get_width();
    int cur_h = gfx_get_height();
    for (int i = 0; i < MODE_COUNT; i++) {
        if (g_modes[i].width == cur_w && g_modes[i].height == cur_h) {
            return i;
        }
    }
    return -1;
}

int display_is_live_switch_supported(void) {
    return (g_vmware_detected || g_bga_detected);
}

int display_is_bga_supported(void) {
    return display_is_live_switch_supported();
}

const char *display_get_adapter_name(void) {
    if (g_vmware_detected) {
        return "VMware SVGA-II Hardware Accelerator";
    }
    if (g_bga_detected) {
        return "Bochs/QEMU BGA (LFB Direct Mode)";
    }
    return "VESA VBE 2.0+ / UEFI GOP Linear Framebuffer";
}

int display_set_mode_by_index(int index) {
    if (index < 0 || index >= MODE_COUNT) return 0;
    return display_set_resolution(g_modes[index].width, g_modes[index].height);
}

int display_set_resolution(int width, int height) {
    boot_info_t *bi = get_boot_info();
    if (!bi) return 0;

    int applied_live = 0;
    if (g_vmware_detected) {
        vmware_write(1, 0); // SVGA_REG_ENABLE = 0
        vmware_write(2, (unsigned int)width); // SVGA_REG_WIDTH
        vmware_write(3, (unsigned int)height); // SVGA_REG_HEIGHT
        vmware_write(7, 32); // SVGA_REG_BITS_PER_PIXEL
        vmware_write(1, 1); // SVGA_REG_ENABLE = 1
        applied_live = 1;
    } else if (g_bga_detected) {
        bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
        bga_write(VBE_DISPI_INDEX_XRES, width);
        bga_write(VBE_DISPI_INDEX_YRES, height);
        bga_write(VBE_DISPI_INDEX_BPP, 32);
        bga_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED | VBE_DISPI_NOCLEARMEM);
        applied_live = 1;
    }

    if (applied_live) {
        // Only update active engine dimensions if hardware mode actually changed
        bi->width = width;
        bi->height = height;
        bi->pitch = width * 4;
        bi->bpp = 32;

        gfx_set_resolution(width, height, width * 4, 32);
        mouse_set_bounds(width, height);
        wallpaper_invalidate();
        wm_on_resolution_change(width, height);
    } else if (width == bi->width && height == bi->height) {
        applied_live = 1;
    }

    // Save persistent settings to SYSTEM.CFG
    sys_set_setting_int("RES_WIDTH", width);
    sys_set_setting_int("RES_HEIGHT", height);
    char res_str[32];
    snprintf(res_str, sizeof(res_str), "%dx%d", width, height);
    sys_set_setting("RESOLUTION", res_str);

    // Save target resolution to MBR Sector 0
    // so both BIOS and UEFI bootloaders automatically boot directly in this resolution on restart
    if (ata_is_available()) {
        unsigned char mbr[512];
        if (ata_read_sectors(0, 1, mbr) == 0 && mbr[510] == 0x55 && mbr[511] == 0xAA) {
            for (int i = 0; i <= 440; i++) {
                if (mbr[i] == 'A' && mbr[i+1] == 'U' && mbr[i+2] == 'R' && mbr[i+3] == 'A') {
                    *(unsigned short *)(mbr + i + 4) = (unsigned short)width;
                    *(unsigned short *)(mbr + i + 6) = (unsigned short)height;
                    ata_write_sectors(0, 1, mbr);
                    break;
                }
            }
        }
    }

    vfs_sync_disk();

    return applied_live ? 1 : 2; // 1 = live, 2 = saved for next boot
}

int display_get_edid_info(edid_info_t *out_info) {
    if (!out_info) return 0;
    memset(out_info, 0, sizeof(edid_info_t));

    const unsigned char *edid = (const unsigned char *)0x5400;
    // Check EDID magic header: 00 FF FF FF FF FF FF 00
    if (edid[0] == 0x00 && edid[1] == 0xFF && edid[2] == 0xFF && edid[3] == 0xFF &&
        edid[4] == 0xFF && edid[5] == 0xFF && edid[6] == 0xFF && edid[7] == 0x00) {
        out_info->valid = 1;

        // Detailed Timing Descriptor #1 (bytes 54..71)
        unsigned short pclk = edid[54] | (edid[55] << 8);
        if (pclk != 0) {
            out_info->native_w = ((edid[58] >> 4) << 8) | edid[56];
            out_info->native_h = ((edid[61] >> 4) << 8) | edid[59];
        }

        // Search for ASCII monitor name descriptor (tag 0xFC at offsets 72, 90, 108)
        for (int d = 0; d < 3; d++) {
            const unsigned char *desc = &edid[72 + (d * 18)];
            if (desc[0] == 0x00 && desc[1] == 0x00 && desc[2] == 0x00 && desc[3] == 0xFC) {
                int len = 0;
                for (int k = 0; k < 13; k++) {
                    char c = (char)desc[5 + k];
                    if (c == '\n' || c == '\r' || c == 0) break;
                    if (c >= 32 && c <= 126) {
                        out_info->monitor_name[len++] = c;
                    }
                }
                out_info->monitor_name[len] = '\0';
                break;
            }
        }

        if (out_info->monitor_name[0] == '\0') {
            strcpy(out_info->monitor_name, "DDC Generic PnP Display");
        }
    } else {
        out_info->valid = 0;
        if (g_bga_detected) {
            out_info->native_w = 1920;
            out_info->native_h = 1080;
            strcpy(out_info->monitor_name, "Bochs/QEMU Display (Auto)");
        } else {
            boot_info_t *bi = get_boot_info();
            out_info->native_w = bi ? bi->width : 1024;
            out_info->native_h = bi ? bi->height : 768;
            strcpy(out_info->monitor_name, "Standard VBE Monitor");
        }
    }

    // Determine Aspect Ratio string
    int nw = out_info->native_w;
    int nh = out_info->native_h;
    if (nw * 9 == nh * 16) {
        strcpy(out_info->aspect, "16:9");
    } else if (nw * 10 == nh * 16) {
        strcpy(out_info->aspect, "16:10");
    } else if (nw * 3 == nh * 4) {
        strcpy(out_info->aspect, "4:3");
    } else if (nw * 4 == nh * 5) {
        strcpy(out_info->aspect, "5:4");
    } else {
        strcpy(out_info->aspect, "Wide");
    }

    return 1;
}

int display_auto_detect(void) {
    edid_info_t info;
    display_get_edid_info(&info);

    int target_w = info.native_w;
    int target_h = info.native_h;

    // Safety clamp to minimum usable graphical resolution
    if (target_w < 800 || target_h < 600) {
        boot_info_t *bi = get_boot_info();
        if (bi && bi->width >= 800 && bi->height >= 600) {
            target_w = bi->width;
            target_h = bi->height;
        } else {
            target_w = 1024;
            target_h = 768;
        }
    }

    sys_set_setting("RES_AUTO", "1");
    return display_set_resolution(target_w, target_h);
}
