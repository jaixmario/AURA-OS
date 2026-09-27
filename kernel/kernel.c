#include "kernel.h"
#include "arch/idt.h"
#include "arch/pit.h"
#include "arch/kbd.h"
#include "arch/mouse.h"
#include "arch/rtc.h"
#include "fs/vfs.h"
#include "gfx/gfx.h"
#include "wm/wm.h"
#include "apps/apps.h"
#include "libc/string.h"

static int start_menu_open = 0;
static boot_info_t *g_boot_info = 0;
static int current_theme = 0; // 0=Nebula, 1=Midnight, 2=Cyberpunk, 3=Emerald

static int g_system_installed = 0;
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

static void load_user_profile(void) {
    vfs_file_t *f = vfs_find("USER.CFG");
    if (!f || f->size == 0) return;

    char *inst = strstr(f->data, "INSTALLED=1");
    if (inst) {
        if (g_boot_info && g_boot_info->is_live_media == 0) {
            g_system_installed = 1;
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
}

void set_desktop_theme(int theme) {
    if (theme >= 0 && theme <= 3) current_theme = theme;
}

int get_desktop_theme(void) {
    return current_theme;
}

boot_info_t *get_boot_info(void) {
    return g_boot_info;
}

static void draw_wallpaper(void) {
    int w = gfx_get_width();
    int h = gfx_get_height();

    unsigned int top_col = RGB(22, 24, 38);
    unsigned int bot_col = RGB(34, 37, 56);
    unsigned int accent_grid = RGB(48, 52, 78);

    if (current_theme == 1) { // Midnight Dark
        top_col = RGB(14, 16, 24);
        bot_col = RGB(22, 26, 38);
        accent_grid = RGB(34, 38, 56);
    } else if (current_theme == 2) { // Cyberpunk
        top_col = RGB(32, 16, 42);
        bot_col = RGB(16, 28, 52);
        accent_grid = RGB(55, 30, 72);
    } else if (current_theme == 3) { // Emerald Forest
        top_col = RGB(14, 28, 24);
        bot_col = RGB(20, 44, 38);
        accent_grid = RGB(28, 55, 45);
    }

    gfx_gradient_v(0, 0, w, h - 48, top_col, bot_col);

    // Subtle geometric horizon accent lines
    for (int y = h - 200; y < h - 48; y += 24) {
        int alpha_y = (y - (h - 200));
        unsigned int line_col = RGB(
            ((top_col >> 16) & 0xFF) + alpha_y / 10,
            ((top_col >> 8) & 0xFF) + alpha_y / 8,
            (top_col & 0xFF) + alpha_y / 6
        );
        gfx_draw_line(0, y, w, y, line_col);
    }

    // Centered Desktop Brand Watermark
    gfx_draw_string(w / 2 - 40, h / 2 - 30, "✦ AURA OS", accent_grid, COLOR_TRANSPARENT);
    if (!sys_is_installed()) {
        gfx_draw_string(w / 2 - 76, h / 2 - 10, "Live Installation Media", accent_grid, COLOR_TRANSPARENT);
    } else {
        gfx_draw_string(w / 2 - 80, h / 2 - 10, "Modern x86 Graphical System", accent_grid, COLOR_TRANSPARENT);
    }
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

        // Truncate title to fit button
        char title_buf[12];
        strncpy(title_buf, win->title, 11);
        title_buf[11] = '\0';
        gfx_draw_string(btn_x + 6, tb_y + 16, title_buf, tab_fg, COLOR_TRANSPARENT);

        btn_x += tab_stride;
    }
}

static void draw_start_menu(void) {
    if (!start_menu_open) return;

    int h = gfx_get_height();
    int menu_x = 8;
    int menu_w = 210;
    int is_inst = sys_is_installed();
    int num_items = is_inst ? 7 : 8;
    int menu_h = is_inst ? 324 : 362;
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

    int mx = mouse_get_x();
    int my = mouse_get_y();

    for (int i = 0; i < num_items; i++) {
        int item_y = menu_y + 42 + (i * 38);
        int is_hover = (mx >= menu_x + 6 && mx <= menu_x + menu_w - 6 &&
                        my >= item_y && my <= item_y + 32);

        if (is_hover) {
            gfx_fillrect(menu_x + 6, item_y, menu_w - 12, 32, RGB(49, 50, 68));
            gfx_drawrect(menu_x + 6, item_y, menu_w - 12, 32, COLOR_ACCENT);
        }

        unsigned int fg = is_hover ? COLOR_WHITE : ((!is_inst && i == 0) ? COLOR_ACCENT : COLOR_TEXT);
        gfx_draw_string(menu_x + 16, item_y + 8, items[i], fg, COLOR_TRANSPARENT);
    }
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
        int num_items = is_inst ? 7 : 8;
        int menu_h = is_inst ? 324 : 362;
        int menu_y = h - 48 - menu_h - 8;

        if (mx >= menu_x && mx <= menu_x + menu_w && my >= menu_y && my <= menu_y + menu_h) {
            for (int i = 0; i < num_items; i++) {
                int item_y = menu_y + 42 + (i * 38);
                if (my >= item_y && my <= item_y + 32) {
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

void kernel_main(boot_info_t *bi) {
    if (bi->magic != 0x41555241) return;
    g_boot_info = bi;

    // Detect Boot Mode: Live CD / ISO vs Installed Hard Disk
    if (bi->is_live_media == 0) {
        g_system_installed = 1; // Booted from hard disk
    } else {
        g_system_installed = 0; // Booted from Live CD / ISO
    }

    // 1. Initialize core architecture & interrupt descriptors
    idt_init();

    // 2. Initialize PIT timer (100 Hz = 10ms per tick)
    pit_init(100);

    // 3. Initialize PS/2 Keyboard
    kbd_init();

    // 4. Initialize PS/2 Mouse
    mouse_init(bi->width, bi->height);

    // 5. Initialize Hardware CMOS Real-Time Clock
    rtc_init();

    // 6. Initialize Virtual File System
    vfs_init();

    // Load installed user profile (if present on disk)
    load_user_profile();

    // Enable CPU interrupts!
    __asm__ volatile ("sti");

    // 7. Initialize Graphics Subsystem
    gfx_init(bi);

    // 8. Initialize Window Manager
    wm_init();

    // Note: Do not automatically open apps on boot (clean desktop, like Ubuntu)

    // Main Desktop Event & Render Loop
    while (1) {
        // 1. Process keyboard events
        while (kbd_has_char()) {
            char key = kbd_get_char();
            window_t *act = wm_get_active_window();
            if (act && act->is_open && !act->is_minimized) {
                wm_handle_key(key);
            } else {
                // Desktop keyboard shortcuts
                if (key == 27 || key == ' ') {
                    start_menu_open = !start_menu_open;
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
                } else if (start_menu_open && key == '7') {
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
