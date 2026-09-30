#include "kernel.h"
#include "arch/idt.h"
#include "arch/pit.h"
#include "arch/kbd.h"
#include "arch/mouse.h"
#include "arch/rtc.h"
#include "arch/io.h"
#include "fs/vfs.h"
#include "gfx/gfx.h"
#include "gfx/wallpaper.h"
#include "wm/wm.h"
#include "arch/display.h"
#include "apps/apps.h"
#include "libc/string.h"

static int start_menu_open = 0;
static boot_info_t *g_boot_info = 0;
static int current_theme = 0; // 0..7

static int g_system_installed = 0;
static int g_logged_in = 1;   // 1 = logged in, 0 = locked / login screen
static char login_input[32] = "";
static int login_error = 0;

static char g_fullname[32] = "Aura User";
static char g_username[32] = "aura";
static char g_hostname[32] = "aura-pc";
static char g_password[32] = "aura";

int sys_is_installed(void) {
    return g_system_installed;
}

void sys_set_installed(int installed) {
    g_system_installed = installed;
}

int sys_is_logged_in(void) {
    return g_logged_in;
}

void sys_set_logged_in(int logged_in) {
    g_logged_in = logged_in;
    if (!logged_in) {
        login_input[0] = '\0';
        login_error = 0;
    }
}

int sys_verify_password(const char *pw) {
    if (!pw) return 0;
    return (strcmp(g_password, pw) == 0);
}

void sys_lock_screen(void) {
    sys_set_logged_in(0);
}

const char *sys_get_fullname(void) {
    return g_fullname;
}

const char *sys_get_username(void) {
    return g_username;
}

const char *sys_get_hostname(void) {
    return g_hostname;
}

void sys_set_user_info(const char *fullname, const char *username, const char *hostname, const char *password) {
    if (fullname && fullname[0]) {
        strncpy(g_fullname, fullname, sizeof(g_fullname) - 1);
        g_fullname[sizeof(g_fullname) - 1] = '\0';
    }
    if (username && username[0]) {
        strncpy(g_username, username, sizeof(g_username) - 1);
        g_username[sizeof(g_username) - 1] = '\0';
    }
    if (hostname && hostname[0]) {
        strncpy(g_hostname, hostname, sizeof(g_hostname) - 1);
        g_hostname[sizeof(g_hostname) - 1] = '\0';
    }
    if (password && password[0]) {
        strncpy(g_password, password, sizeof(g_password) - 1);
        g_password[sizeof(g_password) - 1] = '\0';
    }
}

void sys_shutdown(void) {
    vfs_sync_disk();
    __asm__ volatile ("cli");
    outw(0x604, 0x2000);  // QEMU shutdown
    outw(0xB004, 0x2000); // Bochs shutdown
    outw(0x4004, 0x3400); // VirtualBox shutdown
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

void sys_reboot(void) {
    // 1. Commit and sync all file system buffers
    vfs_sync_disk();

    // 2. Disable CPU interrupts
    __asm__ volatile ("cli");

    // 3. ACPI / PCI Reset via port 0xCF9 (AMD Ryzen / Acer Aspire Lite / modern x86 standard)
    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);

    // 4. PS/2 8042 keyboard controller reset pulse
    for (int i = 0; i < 10000; i++) {
        if ((inb(0x64) & 0x02) == 0) break;
    }
    outb(0x64, 0xFE);

    // 5. Fallback: Triple fault CPU reset
    struct {
        unsigned short limit;
        unsigned int base;
    } __attribute__((packed)) null_idt = { 0, 0 };
    __asm__ volatile ("lidt %0; int $3" : : "m"(null_idt));

    // 6. Halt loop fallback
    while (1) {
        __asm__ volatile ("hlt");
    }
}

static void load_user_profile(void) {
    vfs_file_t *f = vfs_find("USER.CFG");
    if (!f || f->size == 0) return;

    char *inst = strstr(f->data, "INSTALLED=1");
    if (inst) {
        if (g_boot_info && g_boot_info->is_live_media == 0) {
            g_system_installed = 1;
            g_logged_in = 0; // Lock on boot for installed systems!
        }
    }

    char *name_line = strstr(f->data, "NAME=");
    if (name_line) {
        name_line += 5;
        int len = 0;
        while (name_line[len] && name_line[len] != '\r' && name_line[len] != '\n' && len < (int)sizeof(g_fullname) - 1) {
            len++;
        }
        if (len > 0) {
            strncpy(g_fullname, name_line, len);
            g_fullname[len] = '\0';
        }
    }

    char *user_line = strstr(f->data, "USERNAME=");
    if (user_line) {
        user_line += 9;
        int len = 0;
        while (user_line[len] && user_line[len] != '\r' && user_line[len] != '\n' && len < (int)sizeof(g_username) - 1) {
            len++;
        }
        if (len > 0) {
            strncpy(g_username, user_line, len);
            g_username[len] = '\0';
        }
    }

    char *host_line = strstr(f->data, "HOSTNAME=");
    if (host_line) {
        host_line += 9;
        int len = 0;
        while (host_line[len] && host_line[len] != '\r' && host_line[len] != '\n' && len < (int)sizeof(g_hostname) - 1) {
            len++;
        }
        if (len > 0) {
            strncpy(g_hostname, host_line, len);
            g_hostname[len] = '\0';
        }
    }

    char *pass_line = strstr(f->data, "PASSWORD=");
    if (pass_line) {
        pass_line += 9;
        int len = 0;
        while (pass_line[len] && pass_line[len] != '\r' && pass_line[len] != '\n' && len < (int)sizeof(g_password) - 1) {
            len++;
        }
        if (len > 0) {
            strncpy(g_password, pass_line, len);
            g_password[len] = '\0';
        }
    }
}

void sys_set_setting(const char *key, const char *value) {
    if (!key || !value) return;

    vfs_file_t *cfg = vfs_find("SYSTEM.CFG");
    char new_buf[VFS_MAX_FILESIZE];
    memset(new_buf, 0, sizeof(new_buf));

    char key_prefix[64];
    snprintf(key_prefix, sizeof(key_prefix), "%s=", key);
    int key_len = strlen(key_prefix);

    if (cfg && cfg->size > 0) {
        const char *src = cfg->data;
        char *dst = new_buf;
        int found = 0;

        while (*src) {
            const char *line_end = src;
            while (*line_end && *line_end != '\n') line_end++;

            int line_len = line_end - src;
            while (line_len > 0 && (src[line_len - 1] == '\r' || src[line_len - 1] == ' ')) {
                line_len--;
            }

            if (strncmp(src, key_prefix, key_len) == 0) {
                dst += snprintf(dst, sizeof(new_buf) - (dst - new_buf), "%s=%s\n", key, value);
                found = 1;
            } else {
                if (line_len > 0 && (dst - new_buf + line_len + 1 < (int)sizeof(new_buf))) {
                    memcpy(dst, src, line_len);
                    dst += line_len;
                    *dst++ = '\n';
                }
            }

            src = (*line_end == '\n') ? line_end + 1 : line_end;
        }

        if (!found) {
            snprintf(dst, sizeof(new_buf) - (dst - new_buf), "%s=%s\n", key, value);
        }

        vfs_write_file("SYSTEM.CFG", new_buf, strlen(new_buf));
    } else {
        snprintf(new_buf, sizeof(new_buf),
                 "# AuraOS Desktop Configuration\n[STORAGE]\nDRIVER=ATA_PIO\n%s=%s\n",
                 key, value);
        vfs_create_file("SYSTEM.CFG", "System", new_buf, strlen(new_buf), FS_ATTR_SYSTEM);
    }

    vfs_sync_disk();
}

void sys_set_setting_int(const char *key, int value) {
    char val_str[16];
    snprintf(val_str, sizeof(val_str), "%d", value);
    sys_set_setting(key, val_str);
}

int sys_get_setting_int(const char *key, int default_val) {
    vfs_file_t *cfg = vfs_find("SYSTEM.CFG");
    if (!cfg || cfg->size == 0) return default_val;

    char key_prefix[64];
    snprintf(key_prefix, sizeof(key_prefix), "%s=", key);
    int key_len = strlen(key_prefix);

    const char *p = cfg->data;
    while (*p) {
        while (*p == '\r' || *p == '\n') p++;
        if (!*p) break;
        if (strncmp(p, key_prefix, key_len) == 0) {
            p += key_len;
            while (*p == ' ' || *p == '\t') p++;
            int val = 0;
            int neg = 0;
            if (*p == '-') { neg = 1; p++; }
            while (*p >= '0' && *p <= '9') {
                val = val * 10 + (*p - '0');
                p++;
            }
            return neg ? -val : val;
        }
        while (*p && *p != '\n') p++;
    }
    return default_val;
}

const char *sys_get_setting(const char *key, char *out_buf, int max_len) {
    if (!out_buf || max_len <= 0) return "";
    out_buf[0] = '\0';

    vfs_file_t *cfg = vfs_find("SYSTEM.CFG");
    if (!cfg || cfg->size == 0) return "";

    char key_prefix[64];
    snprintf(key_prefix, sizeof(key_prefix), "%s=", key);
    int key_len = strlen(key_prefix);

    const char *p = cfg->data;
    while (*p) {
        while (*p == '\r' || *p == '\n') p++;
        if (!*p) break;
        if (strncmp(p, key_prefix, key_len) == 0) {
            p += key_len;
            while (*p == ' ' || *p == '\t') p++;
            int len = 0;
            while (p[len] && p[len] != '\r' && p[len] != '\n' && len < max_len - 1) {
                out_buf[len] = p[len];
                len++;
            }
            out_buf[len] = '\0';
            return out_buf;
        }
        while (*p && *p != '\n') p++;
    }
    return "";
}

static void load_system_settings(void) {
    // 1. Restore Saved Wallpaper
    int saved_wp = sys_get_setting_int("WALLPAPER", -1);
    if (saved_wp < 0) saved_wp = sys_get_setting_int("THEME", 0);
    if (saved_wp < 0 || saved_wp >= wallpaper_get_count()) saved_wp = 0;
    wallpaper_set(saved_wp);

    // 2. Restore Saved Mouse Speed
    int saved_spd = sys_get_setting_int("MOUSE_SPEED", 1);
    if (saved_spd >= 0 && saved_spd <= 2) {
        mouse_set_speed(saved_spd);
    }

    // 3. Restore Saved Display Resolution or Auto-Detect (only on emulators with BGA support)
    if (display_is_bga_supported()) {
        int auto_detect = sys_get_setting_int("RES_AUTO", 0);
        if (auto_detect) {
            display_auto_detect();
        } else {
            int saved_w = sys_get_setting_int("RES_WIDTH", -1);
            int saved_h = sys_get_setting_int("RES_HEIGHT", -1);
            if (saved_w >= 640 && saved_h >= 480) {
                if (saved_w != gfx_get_width() || saved_h != gfx_get_height()) {
                    display_set_resolution(saved_w, saved_h);
                }
            }
        }
    }
}

int get_theme_count(void) {
    return wallpaper_get_count();
}

const char *get_theme_name(int theme) {
    return wallpaper_get_name(theme);
}

const char *get_theme_desc(int theme) {
    return wallpaper_get_desc(theme);
}

void set_desktop_theme(int theme) {
    if (theme >= 0 && theme < wallpaper_get_count()) {
        wallpaper_set(theme);
        sys_set_setting_int("WALLPAPER", theme);
        sys_set_setting_int("THEME", theme);
    }
}

int get_desktop_theme(void) {
    return wallpaper_get_current();
}

boot_info_t *get_boot_info(void) {
    return g_boot_info;
}

static void draw_wallpaper(void) {
    wallpaper_draw_desktop();

    int w = gfx_get_width();
    int h = gfx_get_height();

    // 1. Centered Desktop Brand Watermark with drop shadow
    gfx_draw_string_shadow(w / 2 - 40, h / 2 - 44, "* AURA OS", COLOR_WHITE, COLOR_SHADOW);
    if (!sys_is_installed()) {
        gfx_draw_string_shadow(w / 2 - 76, h / 2 - 24, "Live Installation Media", RGB(220, 230, 255), COLOR_SHADOW);
    } else {
        gfx_draw_string_shadow(w / 2 - 80, h / 2 - 24, "Modern x86 Graphical System", RGB(220, 230, 255), COLOR_SHADOW);
    }

    // 2. Dynamic Own Screen & Display Hardware Information
    char res_info[64];
    const char *ratio = (w * 9 == h * 16) ? "16:9 Full HD" :
                        (w * 10 == h * 16) ? "16:10 Widescreen" :
                        (w * 3 == h * 4) ? "4:3 Standard" : "Custom Ratio";
    snprintf(res_info, sizeof(res_info), "Screen: %d x %d (%s) @ 32bpp", w, h, ratio);
    int res_x = (w - (strlen(res_info) * 8)) / 2;
    gfx_draw_string_shadow(res_x, h / 2 + 4, res_info, RGB(180, 220, 255), COLOR_SHADOW);

    char hw_info[80];
    snprintf(hw_info, sizeof(hw_info), "Display Panel: Acer Aspire Lite 15.6\" | %s", display_get_adapter_name());
    int hw_x = (w - (strlen(hw_info) * 8)) / 2;
    gfx_draw_string_shadow(hw_x, h / 2 + 24, hw_info, RGB(150, 185, 230), COLOR_SHADOW);

    // 3. Navigation shortcuts guide for keyboard & touchpad users
    const char *tip_str = "[Win] Menu | [Home] Center Mouse | [Arrows/WASD] Move | [Enter] Click | [U-U-R] Restart";
    int tip_x = (w - (strlen(tip_str) * 8)) / 2;
    gfx_draw_string_shadow(tip_x, h / 2 + 48, tip_str, RGB(140, 210, 180), COLOR_SHADOW);

    // 4. Compact screen resolution badge in lower right corner
    char corner_buf[32];
    snprintf(corner_buf, sizeof(corner_buf), "%dx%d 32bpp", w, h);
    gfx_draw_string_shadow(w - (strlen(corner_buf) * 8) - 16, h - 66, corner_buf, RGB(160, 190, 225), COLOR_SHADOW);
}

typedef struct {
    const char *title;
    unsigned int color;
    void (*launch)(void);
    int is_installer;
} desktop_item_t;

static int selected_desktop_icon = -1;
static unsigned int last_icon_click_time = 0;

static void draw_desktop_icons(void) {
    int is_inst = sys_is_installed();
    int count = is_inst ? 7 : 8;

    desktop_item_t items[8];
    int idx = 0;
    if (!is_inst) {
        items[idx++] = (desktop_item_t){ "Install", RGB(30, 144, 255), app_installer_launch, 1 };
    }
    items[idx++] = (desktop_item_t){ "Files", RGB(245, 185, 66), app_files_launch, 0 };
    items[idx++] = (desktop_item_t){ "Terminal", RGB(40, 200, 100), app_term_launch, 0 };
    items[idx++] = (desktop_item_t){ "Notes", RGB(220, 220, 240), app_notes_launch, 0 };
    items[idx++] = (desktop_item_t){ "Settings", RGB(180, 190, 220), app_settings_launch, 0 };
    items[idx++] = (desktop_item_t){ "Calc", RGB(180, 140, 240), app_calc_launch, 0 };
    items[idx++] = (desktop_item_t){ "Paint", RGB(240, 120, 180), app_paint_launch, 0 };
    items[idx++] = (desktop_item_t){ "SysInfo", RGB(100, 200, 240), app_sysinfo_launch, 0 };

    int mx = mouse_get_x();
    int my = mouse_get_y();

    int start_x = 24;
    int start_y = 24;
    int icon_w = 84;
    int icon_h = 74;
    int stride_y = 82;

    for (int i = 0; i < count; i++) {
        int ix = start_x;
        int iy = start_y + (i * stride_y);

        int is_hover = (mx >= ix && mx <= ix + icon_w && my >= iy && my <= iy + icon_h);
        int is_sel = (selected_desktop_icon == i);

        if (is_sel || is_hover) {
            gfx_fillrect(ix, iy, icon_w, icon_h, is_sel ? RGB(45, 52, 78) : RGB(32, 36, 54));
            gfx_drawrect(ix, iy, icon_w, icon_h, is_sel ? COLOR_ACCENT : RGB(55, 60, 85));
        }

        if (items[i].is_installer) {
            // Shiny live installer CD/disc icon
            gfx_fill_circle(ix + 42, iy + 22, 15, RGB(30, 144, 255));
            gfx_draw_circle(ix + 42, iy + 22, 15, COLOR_WHITE);
            gfx_fill_circle(ix + 42, iy + 22, 5, RGB(22, 24, 38));
            gfx_draw_circle(ix + 42, iy + 22, 5, COLOR_ACCENT);
            gfx_draw_string(ix + 18, iy + 42, "Install", COLOR_WHITE, COLOR_TRANSPARENT);
            gfx_draw_string(ix + 18, iy + 56, "AuraOS", COLOR_ACCENT, COLOR_TRANSPARENT);
        } else if (strcmp(items[i].title, "Files") == 0) {
            gfx_fillrect(ix + 28, iy + 12, 14, 5, RGB(245, 185, 66));
            gfx_fillrect(ix + 28, iy + 16, 28, 18, RGB(230, 165, 45));
            gfx_drawrect(ix + 28, iy + 16, 28, 18, RGB(255, 215, 100));
            gfx_draw_string(ix + 22, iy + 48, "Files", COLOR_WHITE, COLOR_TRANSPARENT);
        } else if (strcmp(items[i].title, "Terminal") == 0) {
            gfx_fillrect(ix + 26, iy + 12, 32, 22, RGB(18, 20, 30));
            gfx_drawrect(ix + 26, iy + 12, 32, 22, RGB(70, 75, 100));
            gfx_draw_string(ix + 31, iy + 15, ">_", COLOR_GREEN, COLOR_TRANSPARENT);
            gfx_draw_string(ix + 12, iy + 48, "Terminal", COLOR_WHITE, COLOR_TRANSPARENT);
        } else if (strcmp(items[i].title, "Notes") == 0) {
            gfx_fillrect(ix + 28, iy + 12, 28, 24, RGB(235, 240, 245));
            gfx_draw_line(ix + 32, iy + 18, ix + 50, iy + 18, RGB(120, 130, 150));
            gfx_draw_line(ix + 32, iy + 23, ix + 48, iy + 23, RGB(120, 130, 150));
            gfx_draw_line(ix + 32, iy + 28, ix + 44, iy + 28, RGB(120, 130, 150));
            gfx_draw_string(ix + 22, iy + 48, "Notes", COLOR_WHITE, COLOR_TRANSPARENT);
        } else if (strcmp(items[i].title, "Settings") == 0) {
            gfx_fillrect(ix + 27, iy + 12, 30, 24, RGB(42, 46, 68));
            gfx_drawrect(ix + 27, iy + 12, 30, 24, COLOR_ACCENT);
            gfx_draw_string(ix + 35, iy + 16, "*", COLOR_ACCENT, COLOR_TRANSPARENT);
            gfx_draw_string(ix + 10, iy + 48, "Settings", COLOR_WHITE, COLOR_TRANSPARENT);
        } else if (strcmp(items[i].title, "Calc") == 0) {
            gfx_fillrect(ix + 28, iy + 12, 28, 24, RGB(38, 36, 56));
            gfx_drawrect(ix + 28, iy + 12, 28, 24, RGB(90, 85, 120));
            gfx_draw_string(ix + 33, iy + 16, "+-", COLOR_YELLOW, COLOR_TRANSPARENT);
            gfx_draw_string(ix + 26, iy + 48, "Calc", COLOR_WHITE, COLOR_TRANSPARENT);
        } else if (strcmp(items[i].title, "Paint") == 0) {
            gfx_fillrect(ix + 28, iy + 12, 28, 24, RGB(50, 30, 48));
            gfx_drawrect(ix + 28, iy + 12, 28, 24, RGB(120, 70, 100));
            gfx_fill_circle(ix + 35, iy + 20, 3, COLOR_RED);
            gfx_fill_circle(ix + 45, iy + 20, 3, COLOR_GREEN);
            gfx_fill_circle(ix + 40, iy + 28, 3, COLOR_BLUE);
            gfx_draw_string(ix + 22, iy + 48, "Paint", COLOR_WHITE, COLOR_TRANSPARENT);
        } else if (strcmp(items[i].title, "SysInfo") == 0) {
            gfx_fill_circle(ix + 42, iy + 24, 13, RGB(25, 45, 65));
            gfx_draw_circle(ix + 42, iy + 24, 13, COLOR_ACCENT);
            gfx_draw_string(ix + 39, iy + 16, "i", COLOR_ACCENT, COLOR_TRANSPARENT);
            gfx_draw_string(ix + 14, iy + 48, "SysInfo", COLOR_WHITE, COLOR_TRANSPARENT);
        }
    }
}

static void draw_taskbar(void) {
    int w = gfx_get_width();
    int h = gfx_get_height();
    int tb_y = h - 48;

    // Taskbar bar background
    gfx_fillrect(0, tb_y, w, 48, COLOR_TASKBAR);
    gfx_draw_line(0, tb_y, w - 1, tb_y, COLOR_BORDER);

    // Start Button ("✦ Aura") on the LEFT
    int mx = mouse_get_x();
    int my = mouse_get_y();
    int is_hover_start = (mx >= 8 && mx <= 100 && my >= tb_y + 6 && my <= tb_y + 42);

    unsigned int start_bg = is_hover_start ? COLOR_ACCENT : RGB(40, 43, 62);
    unsigned int start_fg = is_hover_start ? RGB(17, 17, 27) : COLOR_WHITE;

    gfx_fillrect(8, tb_y + 6, 92, 36, start_bg);
    gfx_drawrect(8, tb_y + 6, 92, 36, COLOR_BORDER);
    gfx_draw_string(20, tb_y + 16, "✦ Aura", start_fg, COLOR_TRANSPARENT);

    // System Tray (RIGHT SIDE)
    // RAM badge
    int ram_w = 88;
    int ram_x = w - 96;
    gfx_fillrect(ram_x, tb_y + 10, ram_w, 28, RGB(30, 32, 48));
    gfx_drawrect(ram_x, tb_y + 10, ram_w, 28, COLOR_BORDER);
    gfx_draw_string(ram_x + 6, tb_y + 16, "RAM: 3.2M", COLOR_ACCENT, COLOR_TRANSPARENT);

    // Live Status pulse dot
    gfx_fill_circle(w - 106, tb_y + 24, 4, COLOR_GREEN);

    // Live Date & Time Widget (on the RIGHT, directly next to tray)
    int clock_w = 180;
    int clock_x = w - 116 - clock_w;
    int is_hover_clock = (mx >= clock_x && mx <= clock_x + clock_w && my >= tb_y + 6 && my <= tb_y + 42);

    char dt_buf[32];
    rtc_time_t t;
    rtc_get_datetime(&t);
    snprintf(dt_buf, sizeof(dt_buf), "%04u-%02u-%02u %02u:%02u:%02u",
             t.year, t.month, t.day, t.hour, t.minute, t.second);

    gfx_fillrect(clock_x, tb_y + 6, clock_w, 36, is_hover_clock ? RGB(42, 46, 70) : RGB(30, 32, 48));
    gfx_drawrect(clock_x, tb_y + 6, clock_w, 36, is_hover_clock ? COLOR_ACCENT : COLOR_BORDER);
    gfx_draw_string(clock_x + 14, tb_y + 16, dt_buf, is_hover_clock ? COLOR_ACCENT : COLOR_WHITE, COLOR_TRANSPARENT);

    // Window items on taskbar (starts at x=108 after Start button, ends before Date/Time widget)
    int btn_x = 108;
    int tab_w = 98;
    int tab_stride = 102;
    window_t *active = wm_get_active_window();

    for (int i = 0; i < MAX_WINDOWS; i++) {
        window_t *win = wm_get_window_at_index(i);
        if (!win || !win->is_open) continue;

        if (btn_x + tab_w > clock_x - 6) break;

        int is_act = (win == active && !win->is_minimized);
        unsigned int tab_bg = is_act ? COLOR_ACTIVE_HEADER : RGB(30, 32, 48);
        unsigned int tab_fg = is_act ? COLOR_WHITE : COLOR_TEXT_MUTED;

        gfx_fillrect(btn_x, tb_y + 6, tab_w, 36, tab_bg);
        gfx_drawrect(btn_x, tb_y + 6, tab_w, 36, is_act ? COLOR_ACCENT : COLOR_BORDER);

        // Render title clipped to taskbar button width
        gfx_draw_string_clipped(btn_x + 8, tb_y + 16, win->title, tab_fg, COLOR_TRANSPARENT, tab_w - 14);

        btn_x += tab_stride;
    }
}

static void draw_start_menu(void) {
    if (!start_menu_open) return;

    int h = gfx_get_height();
    int menu_x = 8;
    int menu_w = 210;
    int is_inst = sys_is_installed();
    int num_items = 8;
    int menu_h = 398;
    int menu_y = h - 48 - menu_h - 8;

    // Drop shadow
    gfx_draw_shadow(menu_x, menu_y, menu_w, menu_h, 8);

    // Menu background
    gfx_fillrect(menu_x, menu_y, menu_w, menu_h, RGB(26, 28, 42));
    gfx_drawrect(menu_x, menu_y, menu_w, menu_h, COLOR_ACCENT);

    // Header banner
    gfx_gradient_h(menu_x + 1, menu_y + 1, menu_w - 2, 34, COLOR_ACTIVE_HEADER, COLOR_HEADER);
    gfx_draw_string(menu_x + 16, menu_y + 10, is_inst ? "Applications" : "Live AuraOS", COLOR_WHITE, COLOR_TRANSPARENT);

    // Menu items
    const char *items[8];
    int idx = 0;
    if (!is_inst) {
        items[idx++] = "✦ Install AuraOS 1.0";
    }
    items[idx++] = "> 1. File Explorer";
    items[idx++] = "> 2. Terminal";
    items[idx++] = "> 3. Calculator";
    items[idx++] = "> 4. Canvas Paint";
    items[idx++] = "> 5. Notes Editor";
    items[idx++] = "> 6. Settings Panel";
    items[idx++] = "> 7. System Info";
    if (is_inst) {
        items[idx++] = "* 8. Lock Screen";
    }

    int mx = mouse_get_x();
    int my = mouse_get_y();

    for (int i = 0; i < num_items; i++) {
        int item_y = menu_y + 40 + (i * 36);
        int is_hover = (mx >= menu_x + 6 && mx <= menu_x + menu_w - 6 &&
                        my >= item_y && my <= item_y + 30);

        if (is_hover) {
            gfx_fillrect(menu_x + 6, item_y, menu_w - 12, 30, RGB(49, 50, 68));
            gfx_drawrect(menu_x + 6, item_y, menu_w - 12, 30, COLOR_ACCENT);
        }

        unsigned int fg = is_hover ? COLOR_WHITE : ((!is_inst && i == 0) ? COLOR_ACCENT : (is_inst && i == 7) ? COLOR_YELLOW : COLOR_TEXT);
        gfx_draw_string(menu_x + 16, item_y + 7, items[i], fg, COLOR_TRANSPARENT);
    }

    // Bottom Power / Reboot Bar
    int foot_y = menu_y + menu_h - 40;
    gfx_draw_line(menu_x + 6, foot_y - 4, menu_x + menu_w - 6, foot_y - 4, COLOR_BORDER);

    // [ ⟳ Restart ] button
    int reb_x = menu_x + 8;
    int reb_w = 92;
    int reb_h = 30;
    int is_hover_reb = (mx >= reb_x && mx <= reb_x + reb_w && my >= foot_y && my <= foot_y + reb_h);
    gfx_fillrect(reb_x, foot_y, reb_w, reb_h, is_hover_reb ? RGB(45, 52, 78) : RGB(30, 32, 48));
    gfx_drawrect(reb_x, foot_y, reb_w, reb_h, is_hover_reb ? COLOR_ACCENT : COLOR_BORDER);
    gfx_draw_string(reb_x + 12, foot_y + 7, "Restart", is_hover_reb ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);

    // [ ⏻ Power ] button
    int sht_x = menu_x + 110;
    int sht_w = 92;
    int sht_h = 30;
    int is_hover_sht = (mx >= sht_x && mx <= sht_x + sht_w && my >= foot_y && my <= foot_y + sht_h);
    gfx_fillrect(sht_x, foot_y, sht_w, sht_h, is_hover_sht ? RGB(60, 25, 30) : RGB(38, 20, 24));
    gfx_drawrect(sht_x, foot_y, sht_w, sht_h, is_hover_sht ? COLOR_RED : COLOR_BORDER);
    gfx_draw_string(sht_x + 12, foot_y + 7, "Power Off", is_hover_sht ? COLOR_WHITE : COLOR_RED, COLOR_TRANSPARENT);
}

static void handle_desktop_click(int mx, int my) {
    int w = gfx_get_width();
    int h = gfx_get_height();
    int tb_y = h - 48;

    // Start Button clicked
    if (mx >= 8 && mx <= 100 && my >= tb_y + 6 && my <= tb_y + 42) {
        start_menu_open = !start_menu_open;
        return;
    }

    // Start Menu item clicked
    if (start_menu_open) {
        int menu_x = 8;
        int menu_w = 210;
        int is_inst = sys_is_installed();
        int num_items = 8;
        int menu_h = 398;
        int menu_y = h - 48 - menu_h - 8;

        if (mx >= menu_x && mx <= menu_x + menu_w && my >= menu_y && my <= menu_y + menu_h) {
            int foot_y = menu_y + menu_h - 40;
            // Check Restart button in Start Menu
            if (mx >= menu_x + 8 && mx <= menu_x + 100 && my >= foot_y && my <= foot_y + 30) {
                sys_reboot();
                return;
            }
            // Check Power Off button in Start Menu
            if (mx >= menu_x + 110 && mx <= menu_x + 202 && my >= foot_y && my <= foot_y + 30) {
                sys_shutdown();
                return;
            }

            for (int i = 0; i < num_items; i++) {
                int item_y = menu_y + 40 + (i * 36);
                if (my >= item_y && my <= item_y + 30) {
                    if (!is_inst) {
                        if (i == 0) app_installer_launch();
                        else if (i == 1) app_files_launch();
                        else if (i == 2) app_term_launch();
                        else if (i == 3) app_calc_launch();
                        else if (i == 4) app_paint_launch();
                        else if (i == 5) app_notes_launch();
                        else if (i == 6) app_settings_launch();
                        else if (i == 7) app_sysinfo_launch();
                    } else {
                        if (i == 0) app_files_launch();
                        else if (i == 1) app_term_launch();
                        else if (i == 2) app_calc_launch();
                        else if (i == 3) app_paint_launch();
                        else if (i == 4) app_notes_launch();
                        else if (i == 5) app_settings_launch();
                        else if (i == 6) app_sysinfo_launch();
                        else if (i == 7) sys_lock_screen();
                    }
                    start_menu_open = 0;
                    return;
                }
            }
        } else {
            // Click outside closes start menu
            start_menu_open = 0;
        }
    }

    // Check click on desktop icons
    int clicked_on_window = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        window_t *win = wm_get_window_at_index(i);
        if (win && win->is_open && !win->is_minimized) {
            if (mx >= win->x && mx < win->x + win->width &&
                my >= win->y && my < win->y + win->height) {
                clicked_on_window = 1;
                break;
            }
        }
    }

    if (!clicked_on_window && my < tb_y) {
        int is_inst = sys_is_installed();
        int count = is_inst ? 7 : 8;
        desktop_item_t items[8];
        int idx = 0;
        if (!is_inst) {
            items[idx++] = (desktop_item_t){ "Install", RGB(30, 144, 255), app_installer_launch, 1 };
        }
        items[idx++] = (desktop_item_t){ "Files", RGB(245, 185, 66), app_files_launch, 0 };
        items[idx++] = (desktop_item_t){ "Terminal", RGB(40, 200, 100), app_term_launch, 0 };
        items[idx++] = (desktop_item_t){ "Notes", RGB(220, 220, 240), app_notes_launch, 0 };
        items[idx++] = (desktop_item_t){ "Settings", RGB(180, 190, 220), app_settings_launch, 0 };
        items[idx++] = (desktop_item_t){ "Calc", RGB(180, 140, 240), app_calc_launch, 0 };
        items[idx++] = (desktop_item_t){ "Paint", RGB(240, 120, 180), app_paint_launch, 0 };
        items[idx++] = (desktop_item_t){ "SysInfo", RGB(100, 200, 240), app_sysinfo_launch, 0 };

        int icon_clicked = -1;
        for (int i = 0; i < count; i++) {
            int ix = 24;
            int iy = 24 + (i * 82);
            if (mx >= ix && mx <= ix + 84 && my >= iy && my <= iy + 74) {
                icon_clicked = i;
                break;
            }
        }

        if (icon_clicked >= 0) {
            selected_desktop_icon = icon_clicked;
            items[icon_clicked].launch();
            return;
        } else {
            selected_desktop_icon = -1;
        }
    }

    int clock_w = 180;
    int clock_x = w - 116 - clock_w;

    // Date & Time widget on the right clicked -> Open Date & Time settings!
    if (mx >= clock_x && mx <= clock_x + clock_w && my >= tb_y + 6 && my <= tb_y + 42) {
        app_settings_open_tab(1);
        return;
    }

    // Taskbar window item clicked (starts after Start button at x=108)
    int tab_w = 98;
    int tab_stride = 102;
    if (my >= tb_y + 6 && my <= tb_y + 42 && mx >= 108 && mx < clock_x - 6) {
        int btn_x = 108;
        for (int i = 0; i < MAX_WINDOWS; i++) {
            window_t *win = wm_get_window_at_index(i);
            if (!win || !win->is_open) continue;

            if (btn_x + tab_w > clock_x - 6) break;

            if (mx >= btn_x && mx <= btn_x + tab_w) {
                if (win->is_minimized) {
                    wm_restore_window(win);
                } else {
                    window_t *act = wm_get_active_window();
                    if (act == win) {
                        wm_minimize_window(win);
                    } else {
                        wm_focus_window(win);
                    }
                }
                return;
            }
            btn_x += tab_stride;
        }
    }
}

static void draw_login_screen(void) {
    int w = gfx_get_width();
    int h = gfx_get_height();

    // 1. Tinted photographic wallpaper backdrop
    wallpaper_draw_tinted();

    // 3. Top Clock & Date
    char time_str[32];
    rtc_get_time_string(time_str, sizeof(time_str));
    char date_str[48];
    rtc_get_date_string(date_str, sizeof(date_str));

    int time_x = (w - (strlen(time_str) * 8)) / 2;
    int date_x = (w - (strlen(date_str) * 8)) / 2;
    gfx_draw_string(time_x, 80, time_str, COLOR_WHITE, COLOR_TRANSPARENT);
    gfx_draw_string(date_x, 102, date_str, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // 4. Central Acrylic Login Card
    int card_w = 400;
    int card_h = 320;
    int card_x = (w - card_w) / 2;
    int card_y = (h - card_h) / 2 + 25;

    gfx_draw_shadow(card_x, card_y, card_w, card_h, 8);
    gfx_fillrect(card_x, card_y, card_w, card_h, RGB(24, 26, 38));
    gfx_drawrect(card_x, card_y, card_w, card_h, login_error ? COLOR_RED : COLOR_ACCENT);

    // User Avatar Badge
    int av_cx = card_x + card_w / 2;
    int av_cy = card_y + 46;
    gfx_fill_circle(av_cx, av_cy, 30, COLOR_ACCENT);
    gfx_fill_circle(av_cx, av_cy, 26, RGB(32, 36, 54));

    // User initial
    char init_c = g_username[0];
    if (init_c >= 'a' && init_c <= 'z') init_c -= 32;
    char init_s[2] = { init_c ? init_c : 'U', '\0' };
    gfx_draw_string(av_cx - 4, av_cy - 7, init_s, COLOR_WHITE, COLOR_TRANSPARENT);

    // User identity text
    int name_x = (card_w - (strlen(g_fullname) * 8)) / 2;
    gfx_draw_string(card_x + name_x, card_y + 88, g_fullname, COLOR_WHITE, COLOR_TRANSPARENT);

    char host_sub[64];
    snprintf(host_sub, sizeof(host_sub), "@%s on %s", g_username, g_hostname);
    int host_x = (card_w - (strlen(host_sub) * 8)) / 2;
    gfx_draw_string(card_x + host_x, card_y + 108, host_sub, COLOR_ACCENT, COLOR_TRANSPARENT);

    // Password input box
    int box_w = 280;
    int box_h = 32;
    int box_x = card_x + (card_w - box_w) / 2;
    int box_y = card_y + 138;

    gfx_fillrect(box_x, box_y, box_w, box_h, RGB(16, 18, 26));
    gfx_drawrect(box_x, box_y, box_w, box_h, login_error ? COLOR_RED : COLOR_BORDER);

    int plen = strlen(login_input);
    if (plen == 0) {
        gfx_draw_string(box_x + 10, box_y + 8, "Enter password...", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    } else {
        char mask[32];
        for (int p = 0; p < plen && p < 30; p++) mask[p] = '*';
        mask[plen] = '\0';
        gfx_draw_string(box_x + 10, box_y + 8, mask, COLOR_WHITE, COLOR_TRANSPARENT);
    }
    // Blinking cursor
    if (pit_get_uptime_seconds() % 2 == 0) {
        gfx_draw_string(box_x + 10 + (plen * 8), box_y + 8, "|", COLOR_ACCENT, COLOR_TRANSPARENT);
    }

    // [ Log In -> ] Button
    int btn_w = 280;
    int btn_h = 34;
    int btn_x = card_x + (card_w - btn_w) / 2;
    int btn_y = card_y + 184;

    gfx_fillrect(btn_x, btn_y, btn_w, btn_h, COLOR_ACCENT);
    gfx_drawrect(btn_x, btn_y, btn_w, btn_h, COLOR_WHITE);
    gfx_draw_string(btn_x + (btn_w - 72) / 2, btn_y + 9, "Log In  ->", RGB(17, 17, 27), COLOR_TRANSPARENT);

    // Status or Error Message
    if (login_error) {
        const char *err_s = "[!] Incorrect password. Try again.";
        int ex = (card_w - (strlen(err_s) * 8)) / 2;
        if (ex < 10) ex = 10;
        gfx_draw_string_clipped(card_x + ex, card_y + 236, err_s, COLOR_RED, COLOR_TRANSPARENT, card_w - 20);
    } else {
        const char *inf_s = "Press Enter or click Log In to unlock";
        int ix = (card_w - (strlen(inf_s) * 8)) / 2;
        if (ix < 10) ix = 10;
        gfx_draw_string_clipped(card_x + ix, card_y + 236, inf_s, COLOR_TEXT_MUTED, COLOR_TRANSPARENT, card_w - 20);
    }

    // Hint
    const char *hint_s = "(Password configured during installation)";
    int hx = (card_w - (strlen(hint_s) * 8)) / 2;
    if (hx < 10) hx = 10;
    gfx_draw_string_clipped(card_x + hx, card_y + 262, hint_s, COLOR_TEXT_MUTED, COLOR_TRANSPARENT, card_w - 20);

    // 5. Bottom System Bar
    gfx_fillrect(0, h - 40, w, 40, RGB(14, 16, 24));
    gfx_draw_line(0, h - 40, w, h - 40, COLOR_BORDER);
    gfx_draw_string(16, h - 26, "✦ AuraOS 1.0 (Installed on Hard Disk)", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // [ Restart ] button
    int reb_x = w - 210;
    int reb_y = h - 33;
    gfx_fillrect(reb_x, reb_y, 90, 26, RGB(28, 30, 44));
    gfx_drawrect(reb_x, reb_y, 90, 26, COLOR_BORDER);
    gfx_draw_string(reb_x + 14, reb_y + 5, "Restart", COLOR_TEXT, COLOR_TRANSPARENT);

    // [ Shut Down ] button
    int sht_x = w - 105;
    int sht_y = h - 33;
    gfx_fillrect(sht_x, sht_y, 90, 26, RGB(38, 20, 24));
    gfx_drawrect(sht_x, sht_y, 90, 26, COLOR_RED);
    gfx_draw_string(sht_x + 10, sht_y + 5, "Shut Down", COLOR_RED, COLOR_TRANSPARENT);
}

static void handle_login_click(int mx, int my) {
    int w = gfx_get_width();
    int h = gfx_get_height();

    int card_w = 400;
    int card_h = 320;
    int card_x = (w - card_w) / 2;
    int card_y = (h - card_h) / 2 + 25;

    int btn_w = 280;
    int btn_h = 34;
    int btn_x = card_x + (card_w - btn_w) / 2;
    int btn_y = card_y + 184;

    // Check Log In button click
    if (mx >= btn_x && mx <= btn_x + btn_w && my >= btn_y && my <= btn_y + btn_h) {
        if (sys_verify_password(login_input)) {
            g_logged_in = 1;
            login_error = 0;
            login_input[0] = '\0';
        } else {
            login_error = 1;
            login_input[0] = '\0';
        }
        return;
    }

    // Check Restart button
    int reb_x = w - 210;
    int reb_y = h - 33;
    if (mx >= reb_x && mx <= reb_x + 90 && my >= reb_y && my <= reb_y + 26) {
        sys_reboot();
        return;
    }

    // Check Shut Down button
    int sht_x = w - 105;
    int sht_y = h - 33;
    if (mx >= sht_x && mx <= sht_x + 90 && my >= sht_y && my <= sht_y + 26) {
        sys_shutdown();
        return;
    }
}

static void handle_login_key(char key) {
    if (key == '\r' || key == '\n') {
        if (sys_verify_password(login_input)) {
            g_logged_in = 1;
            login_error = 0;
            login_input[0] = '\0';
        } else {
            login_error = 1;
            login_input[0] = '\0';
        }
    } else if (key == 8 || key == 127) { // Backspace
        int len = strlen(login_input);
        if (len > 0) {
            login_input[len - 1] = '\0';
        }
    } else if (key >= 32 && key <= 126) {
        int len = strlen(login_input);
        if (len < 30) {
            login_input[len] = key;
            login_input[len + 1] = '\0';
            login_error = 0;
        }
    }
}

void kernel_main(boot_info_t *bi) {
    if (bi->magic != 0x41555241) return;
    g_boot_info = bi;

    // 0. Initialize Graphics Subsystem IMMEDIATELY so the display is lit & active
    gfx_init(bi);
    gfx_clear(RGB(24, 25, 38));
    int cx = (bi->width > 300) ? (bi->width / 2 - 130) : 20;
    int cy = (bi->height > 200) ? (bi->height / 2 - 80) : 20;

    gfx_draw_string(cx, cy,      "AuraOS Starting...", COLOR_WHITE, COLOR_TRANSPARENT);
    gfx_draw_string(cx, cy + 25, "[1/6] Core IDT & Exceptions", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_swap_buffers();

    // Detect Boot Mode: Live CD / ISO vs Installed Hard Disk
    if (bi->is_live_media == 0) {
        g_system_installed = 1; // Booted from hard disk
        g_logged_in = 0;        // Locked! Show Login Screen on boot
    } else {
        g_system_installed = 0; // Booted from Live CD / ISO
        g_logged_in = 1;        // Live CD desktop
    }

    // 1. Initialize core architecture & interrupt descriptors
    idt_init();

    gfx_draw_string(cx, cy + 45, "[2/6] PIT Timer & System Clock", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_swap_buffers();

    // 2. Initialize PIT timer (100 Hz = 10ms per tick) & CMOS RTC
    pit_init(100);
    rtc_init();

    gfx_draw_string(cx, cy + 65, "[3/6] Keyboard & Touchpad Input", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_swap_buffers();

    // 3. Initialize PS/2 Keyboard & Mouse (safely probes & keeps IRQ12 masked if no PS/2 mouse)
    kbd_init();
    mouse_init(bi->width, bi->height);

    gfx_draw_string(cx, cy + 85, "[4/6] File System & Profile", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_swap_buffers();

    // 4. Initialize Virtual File System
    vfs_init();
    load_user_profile();

    gfx_draw_string(cx, cy + 105, "[5/6] Desktop Engine & Settings", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_swap_buffers();

    // 5. Initialize Display Driver & Wallpaper Engine
    display_init();
    wallpaper_init();

    // Load persistent system settings (saved wallpaper, mouse speed, etc.)
    load_system_settings();

    // 6. Initialize Window Manager
    wm_init();

    gfx_draw_string(cx, cy + 125, "[6/6] Launching Graphical Desktop...", COLOR_GREEN, COLOR_TRANSPARENT);
    gfx_swap_buffers();

    // Pre-render the initial desktop frame so the transition from boot splash to desktop is instantaneous
    int init_mx = mouse_get_x();
    int init_my = mouse_get_y();
    if (!g_logged_in) {
        draw_login_screen();
        gfx_draw_cursor(init_mx, init_my);
    } else {
        draw_wallpaper();
        draw_desktop_icons();
        wm_render();
        draw_taskbar();
        draw_start_menu();
        gfx_draw_cursor(init_mx, init_my);
    }
    gfx_swap_buffers();

    // Drain any residual scancodes from firmware / boot selection before activating input loop
    while (kbd_has_char()) {
        kbd_get_char();
    }

    // Enable CPU interrupts now that desktop is fully rendered and active!
    __asm__ volatile ("sti");

    static int uur_state = 0;

    // Main Desktop Event & Render Loop
    while (1) {
        // If system is locked, render and handle Login Screen
        if (!g_logged_in) {
            while (kbd_has_char()) {
                char key = kbd_get_char();
                int step = kbd_is_shift_down() ? 6 : 18;
                if ((unsigned char)key == KEY_UP) {
                    mouse_move_relative(0, -step);
                } else if ((unsigned char)key == KEY_DOWN) {
                    mouse_move_relative(0, step);
                } else if ((unsigned char)key == KEY_LEFT) {
                    mouse_move_relative(-step, 0);
                } else if ((unsigned char)key == KEY_RIGHT) {
                    mouse_move_relative(step, 0);
                } else if ((unsigned char)key == KEY_HOME) {
                    mouse_center();
                } else {
                    // Check 'u' then 'u' then 'r' reboot sequence on lock screen
                    if (key == 'u' || key == 'U') {
                        if (uur_state == 0) uur_state = 1;
                        else if (uur_state == 1) uur_state = 2;
                    } else if (key == 'r' || key == 'R') {
                        if (uur_state == 2) {
                            sys_reboot();
                        }
                        uur_state = 0;
                    } else {
                        uur_state = 0;
                    }
                    handle_login_key(key);
                }
            }

            int mx = mouse_get_x();
            int my = mouse_get_y();
            int clicked = mouse_clicked(0);
            if (clicked) {
                handle_login_click(mx, my);
            }

            draw_login_screen();
            gfx_draw_cursor(mx, my);
            gfx_swap_buffers();
            __asm__ volatile ("hlt");
            continue;
        }

        // 1. Process keyboard events
        while (kbd_has_char()) {
            char key = kbd_get_char();

            // Keyboard Mousekeys: Arrow keys move the mouse cursor! (Hold Shift for precision mode)
            int step = kbd_is_shift_down() ? 6 : 18;
            if ((unsigned char)key == KEY_UP) {
                mouse_move_relative(0, -step);
                continue;
            } else if ((unsigned char)key == KEY_DOWN) {
                mouse_move_relative(0, step);
                continue;
            } else if ((unsigned char)key == KEY_LEFT) {
                mouse_move_relative(-step, 0);
                continue;
            } else if ((unsigned char)key == KEY_RIGHT) {
                mouse_move_relative(step, 0);
                continue;
            } else if ((unsigned char)key == KEY_HOME) {
                mouse_center();
                continue;
            }

            // Windows / Super key: Toggle Start Menu anywhere!
            if ((unsigned char)key == KEY_SUPER) {
                start_menu_open = !start_menu_open;
                continue;
            }

            // Escape key closes Start Menu if open
            if (key == 27 && start_menu_open) {
                start_menu_open = 0;
                continue;
            }

            // Global tracking for 'u' then 'u' then 'r' reboot sequence
            if (key == 'u' || key == 'U') {
                if (uur_state == 0) uur_state = 1;
                else if (uur_state == 1) uur_state = 2;
            } else if (key == 'r' || key == 'R') {
                if (uur_state == 2) {
                    sys_reboot();
                }
                uur_state = 0;
            } else if ((unsigned char)key != KEY_UP && (unsigned char)key != KEY_DOWN &&
                       (unsigned char)key != KEY_LEFT && (unsigned char)key != KEY_RIGHT &&
                       (unsigned char)key != KEY_HOME && (unsigned char)key != KEY_SUPER) {
                uur_state = 0;
            }

            window_t *act = wm_get_active_window();
            if (act && act->is_open && !act->is_minimized) {
                wm_handle_key(key);
            } else {
                // Desktop keyboard shortcuts
                if (key == '\r' || key == '\n') {
                    // Enter key triggers click at current cursor location
                    mouse_inject_click(1, 0);
                } else if (key == ' ') {
                    start_menu_open = !start_menu_open;
                } else if (key == 'w' || key == 'W') {
                    // WASD mouse movement on desktop
                    mouse_move_relative(0, -step);
                } else if (key == 's' || key == 'S') {
                    mouse_move_relative(0, step);
                } else if (key == 'a' || key == 'A') {
                    mouse_move_relative(-step, 0);
                } else if (key == 'd' || key == 'D') {
                    mouse_move_relative(step, 0);
                } else if (!sys_is_installed() && (key == 'i' || key == 'I')) {
                    app_installer_launch();
                } else if (key == 'f' || key == 'F' || (start_menu_open && key == '1')) {
                    app_files_launch();
                    start_menu_open = 0;
                } else if (key == 't' || key == 'T' || (start_menu_open && key == '2')) {
                    app_term_launch();
                    start_menu_open = 0;
                } else if (key == 'c' || key == 'C' || (start_menu_open && key == '3')) {
                    app_calc_launch();
                    start_menu_open = 0;
                } else if (key == 'p' || key == 'P' || (start_menu_open && key == '4')) {
                    app_paint_launch();
                    start_menu_open = 0;
                } else if (key == 'n' || key == 'N' || (start_menu_open && key == '5')) {
                    app_notes_launch();
                    start_menu_open = 0;
                } else if (key == 's' || key == 'S' || (start_menu_open && key == '6')) {
                    app_settings_launch();
                    start_menu_open = 0;
                } else if (key == 'y' || key == 'Y' || (start_menu_open && key == '7')) {
                    app_sysinfo_launch();
                    start_menu_open = 0;
                }
            }
        }

        // 2. Process mouse events
        int mx = mouse_get_x();
        int my = mouse_get_y();
        int btn_left = mouse_is_left_down();
        int clicked = mouse_clicked(0);

        if (clicked) {
            handle_desktop_click(mx, my);
        }

        wm_handle_mouse(mx, my, btn_left, clicked);

        // 3. Render Desktop Scene
        draw_wallpaper();
        draw_desktop_icons();
        wm_render();
        draw_taskbar();
        draw_start_menu();

        // 4. Render Mouse Cursor on top
        gfx_draw_cursor(mx, my);

        // 5. Blit backbuffer to VRAM
        gfx_swap_buffers();

        // 6. Halt CPU until next interrupt (power efficient & smooth timing)
        __asm__ volatile ("hlt");
    }
}
