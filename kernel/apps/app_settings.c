#include "apps.h"
#include "../kernel.h"
#include "../gfx/gfx.h"
#include "../gfx/wallpaper.h"
#include "../arch/mouse.h"
#include "../arch/pit.h"
#include "../arch/rtc.h"
#include "../arch/io.h"
#include "../arch/display.h"
#include "../libc/string.h"

static int active_tab = 3; // 0=Themes, 1=Date & Time, 2=Mouse, 3=Display & Res, 4=About

static const char *tab_names[5] = {
    "Personalize",
    "Date & Time",
    "Mouse & Speed",
    "Display & Res",
    "About System"
};

static const char *speed_names[3] = {
    "Slow (1.0x)",
    "Normal (1.5x)",
    "Fast (2.0x)"
};

static const char *speed_descs[3] = {
    "Pixel-precision cursor movement",
    "Smooth adaptive acceleration curve (Default)",
    "High-speed rapid cursor navigation"
};

static char display_status_msg[64] = "Click any resolution above to switch!";

static void settings_draw(window_t *win) {
    int wx = win->x;
    int wy = win->y + TITLEBAR_HEIGHT;

    // 1. Sidebar Background
    int sb_w = 140;
    int client_h = win->height - TITLEBAR_HEIGHT;
    gfx_fillrect(wx, wy, sb_w, client_h, RGB(24, 26, 38));
    gfx_draw_line(wx + sb_w, wy, wx + sb_w, wy + client_h - 1, COLOR_BORDER);

    // Sidebar Title
    gfx_draw_string(wx + 16, wy + 16, "Settings", COLOR_WHITE, COLOR_TRANSPARENT);

    // Sidebar Tabs
    for (int i = 0; i < 5; i++) {
        int ty = wy + 48 + (i * 36);
        int is_act = (active_tab == i);

        if (is_act) {
            gfx_fillrect(wx + 6, ty, sb_w - 12, 30, RGB(42, 46, 68));
            gfx_drawrect(wx + 6, ty, sb_w - 12, 30, COLOR_ACCENT);
            // Left indicator bar
            gfx_fillrect(wx + 6, ty + 3, 3, 24, COLOR_ACCENT);
        }

        gfx_draw_string(wx + 16, ty + 7, tab_names[i], is_act ? COLOR_WHITE : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    }

    // 2. Content Panel
    int cx = wx + sb_w + 16;
    int cy = wy + 16;
    int cw = win->width - sb_w - 32;

    if (active_tab == 0) {
        // Tab 0: Themes & Personalization
        gfx_draw_string(cx, cy, "Photographic Wallpapers", COLOR_WHITE, COLOR_TRANSPARENT);
        gfx_draw_string(cx, cy + 18, "Select a native wallpaper for your desktop:", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

        int cur_theme = get_desktop_theme();
        int count = wallpaper_get_count();

        for (int i = 0; i < count; i++) {
            int bx = cx;
            int by = cy + 44 + (i * 54);
            int is_sel = (cur_theme == i);

            gfx_fillrect(bx, by, cw, 48, is_sel ? RGB(36, 44, 68) : RGB(26, 28, 40));
            gfx_drawrect(bx, by, cw, 48, is_sel ? COLOR_ACCENT : COLOR_BORDER);

            unsigned int sw_col = wallpaper_get_color(i);
            gfx_fill_circle(bx + 20, by + 24, 8, sw_col);
            if (is_sel) {
                gfx_fill_circle(bx + 20, by + 24, 3, RGB(17, 17, 27));
            }

            gfx_draw_string(bx + 38, by + 8, get_theme_name(i), is_sel ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);
            gfx_draw_string_clipped(bx + 38, by + 26, get_theme_desc(i), COLOR_TEXT_MUTED, COLOR_TRANSPARENT, cw - 120);

            if (is_sel) {
                gfx_fillrect(bx + cw - 74, by + 13, 62, 22, COLOR_ACCENT);
                gfx_draw_string(bx + cw - 67, by + 16, "Active", RGB(17, 17, 27), COLOR_TRANSPARENT);
            } else {
                gfx_fillrect(bx + cw - 74, by + 13, 62, 22, RGB(36, 40, 58));
                gfx_drawrect(bx + cw - 74, by + 13, 62, 22, COLOR_BORDER);
                gfx_draw_string(bx + cw - 65, by + 16, "Apply", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
            }
        }
    } else if (active_tab == 1) {
        // Tab 1: Date & Time Configuration
        gfx_draw_string(cx, cy, "Date & Time Configuration", COLOR_WHITE, COLOR_TRANSPARENT);
        gfx_draw_string(cx, cy + 18, "Hardware CMOS Real-Time Clock Synchronization:", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

        rtc_time_t t;
        rtc_get_datetime(&t);

        int card_y = cy + 40;
        gfx_fillrect(cx, card_y, cw, 54, RGB(22, 24, 34));
        gfx_drawrect(cx, card_y, cw, 54, COLOR_ACCENT);

        char dt_big[64];
        snprintf(dt_big, sizeof(dt_big), "%04u-%02u-%02u   %02u:%02u:%02u",
                 t.year, t.month, t.day, t.hour, t.minute, t.second);
        gfx_draw_string(cx + 20, card_y + 18, dt_big, COLOR_WHITE, COLOR_TRANSPARENT);

        // Adjust Time Controls
        int time_y = card_y + 64;
        gfx_draw_string(cx, time_y, "Adjust Time:", COLOR_TEXT, COLOR_TRANSPARENT);

        int tby = time_y + 20;
        int btn_h = 28;

        gfx_fillrect(cx, tby, 64, btn_h, RGB(36, 40, 58));
        gfx_drawrect(cx, tby, 64, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 8, tby + 6, "-1 Hr", COLOR_TEXT, COLOR_TRANSPARENT);

        gfx_fillrect(cx + 70, tby, 64, btn_h, RGB(36, 40, 58));
        gfx_drawrect(cx + 70, tby, 64, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 78, tby + 6, "+1 Hr", COLOR_TEXT, COLOR_TRANSPARENT);

        gfx_fillrect(cx + 144, tby, 68, btn_h, RGB(36, 40, 58));
        gfx_drawrect(cx + 144, tby, 68, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 152, tby + 6, "-1 Min", COLOR_TEXT, COLOR_TRANSPARENT);

        gfx_fillrect(cx + 218, tby, 68, btn_h, RGB(36, 40, 58));
        gfx_drawrect(cx + 218, tby, 68, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 226, tby + 6, "+1 Min", COLOR_TEXT, COLOR_TRANSPARENT);

        gfx_fillrect(cx + 292, tby, 72, btn_h, RGB(36, 40, 58));
        gfx_drawrect(cx + 292, tby, 72, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 298, tby + 6, "+10 Min", COLOR_TEXT, COLOR_TRANSPARENT);

        // Adjust Date Controls
        int date_y = time_y + 56;
        gfx_draw_string(cx, date_y, "Adjust Date:", COLOR_TEXT, COLOR_TRANSPARENT);

        int dby = date_y + 20;
        gfx_fillrect(cx, dby, 64, btn_h, RGB(36, 40, 58));
        gfx_drawrect(cx, dby, 64, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 8, dby + 6, "-1 Day", COLOR_TEXT, COLOR_TRANSPARENT);

        gfx_fillrect(cx + 70, dby, 64, btn_h, RGB(36, 40, 58));
        gfx_drawrect(cx + 70, dby, 64, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 78, dby + 6, "+1 Day", COLOR_TEXT, COLOR_TRANSPARENT);

        gfx_fillrect(cx + 144, dby, 68, btn_h, RGB(36, 40, 58));
        gfx_drawrect(cx + 144, dby, 68, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 150, dby + 6, "-1 Mon", COLOR_TEXT, COLOR_TRANSPARENT);

        gfx_fillrect(cx + 218, dby, 68, btn_h, RGB(36, 40, 58));
        gfx_drawrect(cx + 218, dby, 68, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 224, dby + 6, "+1 Mon", COLOR_TEXT, COLOR_TRANSPARENT);

        gfx_fillrect(cx + 292, dby, 72, btn_h, RGB(36, 40, 58));
        gfx_drawrect(cx + 292, dby, 72, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 300, dby + 6, "+1 Yr", COLOR_TEXT, COLOR_TRANSPARENT);

        // Hardware Actions
        int act_y = date_y + 56;
        gfx_draw_string(cx, act_y, "Hardware CMOS RTC Controls:", COLOR_TEXT, COLOR_TRANSPARENT);

        int aby = act_y + 20;
        gfx_fillrect(cx, aby, 176, 32, RGB(42, 46, 68));
        gfx_drawrect(cx, aby, 176, 32, COLOR_BORDER);
        gfx_draw_string(cx + 12, aby + 8, "Read Live CMOS RTC", COLOR_WHITE, COLOR_TRANSPARENT);

        gfx_fillrect(cx + 188, aby, 176, 32, RGB(40, 167, 69));
        gfx_drawrect(cx + 188, aby, 176, 32, COLOR_WHITE);
        gfx_draw_string(cx + 202, aby + 8, "Write & Save to CMOS", COLOR_WHITE, COLOR_TRANSPARENT);

    } else if (active_tab == 2) {
        // Tab 2: Mouse & Speed Configuration
        gfx_draw_string(cx, cy, "Mouse Cursor & Sensitivity", COLOR_WHITE, COLOR_TRANSPARENT);
        gfx_draw_string(cx, cy + 18, "Hardware PS/2 Mouse Controller (IRQ12, 200 Hz):", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

        int cur_speed = mouse_get_speed();

        for (int i = 0; i < 3; i++) {
            int bx = cx;
            int by = cy + 44 + (i * 58);
            int is_sel = (cur_speed == i);

            gfx_fillrect(bx, by, cw, 48, is_sel ? RGB(36, 44, 68) : RGB(26, 28, 40));
            gfx_drawrect(bx, by, cw, 48, is_sel ? COLOR_ACCENT : COLOR_BORDER);

            gfx_fill_circle(bx + 20, by + 24, 7, is_sel ? COLOR_ACCENT : RGB(40, 42, 54));
            if (is_sel) {
                gfx_fill_circle(bx + 20, by + 24, 2, RGB(17, 17, 27));
            }

            gfx_draw_string(bx + 38, by + 8, speed_names[i], is_sel ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);
            gfx_draw_string_clipped(bx + 38, by + 26, speed_descs[i], COLOR_TEXT_MUTED, COLOR_TRANSPARENT, cw - 120);

            if (is_sel) {
                gfx_draw_string(cx + cw - 70, by + 16, "[Active]", COLOR_ACCENT, COLOR_TRANSPARENT);
            }
        }

        // Live Cursor Coordinate Readout box
        int box_y = cy + 240;
        gfx_fillrect(cx, box_y, cw, 44, RGB(22, 24, 34));
        gfx_drawrect(cx, box_y, cw, 44, COLOR_BORDER);

        char pos_str[64];
        snprintf(pos_str, sizeof(pos_str), "Live Cursor Pos: (%d, %d)  |  %s",
                 mouse_get_x(), mouse_get_y(),
                 mouse_is_detected() ? "PS/2 Mouse Ready" : "Keyboard Navigation Active");
        gfx_draw_string(cx + 16, box_y + 14, pos_str, COLOR_WHITE, COLOR_TRANSPARENT);

    } else if (active_tab == 3) {
        // Tab 3: Interactive Display Resolution Switcher & Auto-Detector
        gfx_draw_string(cx, cy, "Display & Screen Resolution Switcher", COLOR_WHITE, COLOR_TRANSPARENT);
        gfx_draw_string(cx, cy + 18, "Auto-detect native monitor size or pick a standard mode:", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

        // 1. Auto-Detect Display Size Hero Card
        int auto_y = cy + 36;
        int auto_h = 38;
        gfx_fillrect(cx, auto_y, cw, auto_h, RGB(22, 34, 52));
        gfx_drawrect(cx, auto_y, cw, auto_h, COLOR_ACCENT);

        edid_info_t edid;
        display_get_edid_info(&edid);

        char auto_label[64];
        snprintf(auto_label, sizeof(auto_label), "✦ Monitor: %s (%dx%d %s)",
                 edid.monitor_name, edid.native_w, edid.native_h, edid.aspect);
        gfx_draw_string_clipped(cx + 12, auto_y + 11, auto_label, COLOR_WHITE, COLOR_TRANSPARENT, cw - 180);

        // [ ✦ Auto-Detect Size ] Button
        int abtn_w = 160;
        int abtn_x = cx + cw - abtn_w - 8;
        int abtn_y = auto_y + 6;
        gfx_fillrect(abtn_x, abtn_y, abtn_w, 26, COLOR_ACCENT);
        gfx_drawrect(abtn_x, abtn_y, abtn_w, 26, COLOR_WHITE);
        gfx_draw_string(abtn_x + 8, abtn_y + 5, "✦ Auto-Detect Size", RGB(17, 17, 27), COLOR_TRANSPARENT);

        // 2. Standard Resolutions List
        int cur_w = gfx_get_width();
        int cur_h = gfx_get_height();
        int mode_count = display_get_mode_count();

        for (int i = 0; i < mode_count && i < 8; i++) {
            const display_mode_t *m = display_get_mode(i);
            int bx = cx;
            int by = cy + 80 + (i * 30);
            int is_cur = (m->width == cur_w && m->height == cur_h);

            gfx_fillrect(bx, by, cw, 28, is_cur ? RGB(32, 46, 72) : RGB(26, 28, 40));
            gfx_drawrect(bx, by, cw, 28, is_cur ? COLOR_ACCENT : COLOR_BORDER);

            // Aspect ratio tag
            gfx_fillrect(bx + 6, by + 4, 46, 20, is_cur ? COLOR_ACCENT : RGB(36, 40, 58));
            gfx_draw_string(bx + 10, by + 6, m->aspect, is_cur ? RGB(17, 17, 27) : COLOR_WHITE, COLOR_TRANSPARENT);

            // Resolution title
            gfx_draw_string(bx + 58, by + 6, m->label, is_cur ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);

            // Description
            gfx_draw_string_clipped(bx + 165, by + 6, m->desc, COLOR_TEXT_MUTED, COLOR_TRANSPARENT, cw - 245);

            // Active badge or Apply button
            if (is_cur) {
                gfx_fillrect(bx + cw - 78, by + 3, 70, 22, RGB(40, 167, 69));
                gfx_draw_string(bx + cw - 72, by + 6, "[Active]", COLOR_WHITE, COLOR_TRANSPARENT);
            } else {
                gfx_fillrect(bx + cw - 78, by + 3, 70, 22, RGB(36, 42, 64));
                gfx_drawrect(bx + cw - 78, by + 3, 70, 22, COLOR_BORDER);
                gfx_draw_string(bx + cw - 66, by + 6, "Apply", COLOR_ACCENT, COLOR_TRANSPARENT);
            }
        }

        // 3. Hardware Specs and Status Card
        int info_y = cy + 80 + (mode_count > 8 ? 8 : mode_count) * 30 + 4;
        int info_h = 58;
        gfx_fillrect(cx, info_y, cw, info_h, RGB(20, 22, 32));
        gfx_drawrect(cx, info_y, cw, info_h, COLOR_BORDER);

        boot_info_t *bi = get_boot_info();
        char line1[80], line2[80], line3[80];
        snprintf(line1, sizeof(line1), "Adapter: %s", display_get_adapter_name());
        snprintf(line2, sizeof(line2), "Active : %dx%d (32bpp) | VRAM: 0x%08X",
                 cur_w, cur_h, bi ? bi->fb_base : 0);
        snprintf(line3, sizeof(line3), "%s", display_status_msg);

        gfx_draw_string_clipped(cx + 10, info_y + 4,  line1, COLOR_ACCENT, COLOR_TRANSPARENT, cw - 120);
        gfx_draw_string_clipped(cx + 10, info_y + 20, line2, COLOR_TEXT_MUTED, COLOR_TRANSPARENT, cw - 120);
        gfx_draw_string_clipped(cx + 10, info_y + 38, line3, COLOR_GREEN, COLOR_TRANSPARENT, cw - 120);

        // Restart button on bottom right of info card
        int reb_x = cx + cw - 104;
        int reb_y = info_y + 16;
        gfx_fillrect(reb_x, reb_y, 96, 26, RGB(42, 46, 68));
        gfx_drawrect(reb_x, reb_y, 96, 26, COLOR_BORDER);
        gfx_draw_string(reb_x + 10, reb_y + 5, "Restart PC", COLOR_WHITE, COLOR_TRANSPARENT);

    } else if (active_tab == 4) {
        // Tab 4: About System
        gfx_fillrect(cx, cy + 4, 60, 60, RGB(42, 46, 68));
        gfx_drawrect(cx, cy + 4, 60, 60, COLOR_ACCENT);
        gfx_draw_string(cx + 20, cy + 18, "✦", COLOR_ACCENT, COLOR_TRANSPARENT);
        gfx_draw_string(cx + 14, cy + 38, "AURA", COLOR_WHITE, COLOR_TRANSPARENT);

        gfx_draw_string(cx + 76, cy + 12, "AuraOS Graphical System", COLOR_WHITE, COLOR_TRANSPARENT);
        gfx_draw_string(cx + 76, cy + 32, "Version 1.2.0 (Release-x86)", COLOR_ACCENT, COLOR_TRANSPARENT);
        gfx_draw_string(cx + 76, cy + 50, "Custom 32-bit x86 Bare-Metal Operating System", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

        int ay = cy + 86;
        gfx_draw_line(cx, ay, cx + cw, ay, COLOR_BORDER);

        ay += 16;
        gfx_draw_string_clipped(cx, ay,       "Architecture :  Intel x86 (32-bit Protected Mode)", COLOR_TEXT, COLOR_TRANSPARENT, cw - 8);
        gfx_draw_string_clipped(cx, ay + 24,  "Memory Model :  Ring 0 Flat Model (1MB Stack)", COLOR_TEXT, COLOR_TRANSPARENT, cw - 8);
        gfx_draw_string_clipped(cx, ay + 48,  "File Systems :  ATA FAT16 Disk & ISO9660 CD-ROM", COLOR_TEXT, COLOR_TRANSPARENT, cw - 8);
        gfx_draw_string_clipped(cx, ay + 72,  "Input Engine :  PS/2 Mouse (200Hz) & Keyboard IRQ", COLOR_TEXT, COLOR_TRANSPARENT, cw - 8);
        gfx_draw_string_clipped(cx, ay + 96,  "Timer & RTC  :  100 Hz PIT & CMOS Real-Time Clock", COLOR_TEXT, COLOR_TRANSPARENT, cw - 8);

        char up_str[48];
        snprintf(up_str, sizeof(up_str), "System Uptime:  %u seconds (%u ticks)", pit_get_uptime_seconds(), pit_get_ticks());
        gfx_draw_string_clipped(cx, ay + 120, up_str, COLOR_GREEN, COLOR_TRANSPARENT, cw - 8);
    }
}

static void settings_click(window_t *win, int rx, int ry, int btn) {
    (void)btn;
    int sb_w = 140;

    // Check sidebar tabs clicks
    if (rx >= 6 && rx <= sb_w - 6) {
        for (int i = 0; i < 5; i++) {
            int ty = 48 + (i * 36);
            if (ry >= ty && ry <= ty + 30) {
                active_tab = i;
                return;
            }
        }
    }

    // Check right content clicks
    int cx = sb_w + 16;
    int cw = win->width - sb_w - 32;

    if (active_tab == 0) {
        int count = wallpaper_get_count();
        for (int i = 0; i < count; i++) {
            int by = 60 + (i * 54);
            if (rx >= cx && rx <= cx + cw && ry >= by && ry <= by + 48) {
                set_desktop_theme(i);
                return;
            }
        }
    } else if (active_tab == 1) {
        // Date & Time buttons
        int card_y = 16 + 40;
        int time_y = card_y + 64;
        int tby = time_y + 20;
        int btn_h = 28;

        if (ry >= tby && ry <= tby + btn_h) {
            if (rx >= cx && rx <= cx + 64)         { rtc_adjust_hour(-1); return; }
            if (rx >= cx + 70 && rx <= cx + 134)   { rtc_adjust_hour(+1); return; }
            if (rx >= cx + 144 && rx <= cx + 212)  { rtc_adjust_minute(-1); return; }
            if (rx >= cx + 218 && rx <= cx + 286)  { rtc_adjust_minute(+1); return; }
            if (rx >= cx + 292 && rx <= cx + 364)  { rtc_adjust_minute(+10); return; }
        }

        int date_y = time_y + 56;
        int dby = date_y + 20;
        if (ry >= dby && ry <= dby + btn_h) {
            if (rx >= cx && rx <= cx + 64)         { rtc_adjust_day(-1); return; }
            if (rx >= cx + 70 && rx <= cx + 134)   { rtc_adjust_day(+1); return; }
            if (rx >= cx + 144 && rx <= cx + 212)  { rtc_adjust_month(-1); return; }
            if (rx >= cx + 218 && rx <= cx + 286)  { rtc_adjust_month(+1); return; }
            if (rx >= cx + 292 && rx <= cx + 364)  { rtc_adjust_year(+1); return; }
        }

        int act_y = date_y + 56;
        int aby = act_y + 20;
        if (ry >= aby && ry <= aby + 32) {
            if (rx >= cx && rx <= cx + 176) {
                rtc_sync_from_cmos();
                return;
            }
            if (rx >= cx + 188 && rx <= cx + 364) {
                rtc_time_t t;
                rtc_get_datetime(&t);
                rtc_set_datetime(&t);
                return;
            }
        }
    } else if (active_tab == 2) {
        // Mouse speed buttons
        for (int i = 0; i < 3; i++) {
            int by = 60 + (i * 58);
            if (rx >= cx && rx <= cx + cw && ry >= by && ry <= by + 48) {
                mouse_set_speed(i);
                sys_set_setting_int("MOUSE_SPEED", i);
                return;
            }
        }
    } else if (active_tab == 3) {
        // 1. Auto-Detect Button clicked
        int auto_y = 16 + 36;
        int abtn_w = 160;
        int abtn_x = cx + cw - abtn_w - 8;
        int abtn_y = auto_y + 6;
        if (rx >= abtn_x && rx <= abtn_x + abtn_w && ry >= abtn_y && ry <= abtn_y + 26) {
            edid_info_t edid;
            display_get_edid_info(&edid);
            int res = display_auto_detect();
            if (res == 1) {
                snprintf(display_status_msg, sizeof(display_status_msg), "[v] Auto-detected & applied %dx%d!", edid.native_w, edid.native_h);
            } else {
                snprintf(display_status_msg, sizeof(display_status_msg), "[v] Auto-detected %dx%d (saved to config)!", edid.native_w, edid.native_h);
            }
            return;
        }

        // 2. Display Resolution row buttons
        int mode_count = display_get_mode_count();
        for (int i = 0; i < mode_count && i < 8; i++) {
            int by = 16 + 80 + (i * 30);
            if (rx >= cx && rx <= cx + cw && ry >= by && ry <= by + 28) {
                int res = display_set_mode_by_index(i);
                const display_mode_t *m = display_get_mode(i);
                if (res == 1) {
                    snprintf(display_status_msg, sizeof(display_status_msg), "[v] Switched to %s live!", m->label);
                } else {
                    snprintf(display_status_msg, sizeof(display_status_msg), "[v] Saved %s to boot config!", m->label);
                }
                return;
            }
        }

        // 3. Restart button in info card
        int info_y = 16 + 80 + (mode_count > 8 ? 8 : mode_count) * 30 + 4;
        int reb_x = cx + cw - 104;
        int reb_y = info_y + 16;
        if (rx >= reb_x && rx <= reb_x + 96 && ry >= reb_y && ry <= reb_y + 26) {
            sys_reboot();
            return;
        }
    }
}

void app_settings_open_tab(int tab) {
    if (tab >= 0 && tab < 5) active_tab = tab;
    app_settings_launch();
}

void app_settings_launch(void) {
    window_t *win = wm_create_window("Settings Control Panel", 160, 60, 640, 480, RGB(28, 30, 44));
    if (!win) return;
    win->draw_client = settings_draw;
    win->on_click = settings_click;
}
