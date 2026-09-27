#include "apps.h"
#include "../kernel.h"
#include "../gfx/gfx.h"
#include "../gfx/wallpaper.h"
#include "../arch/mouse.h"
#include "../arch/pit.h"
#include "../arch/rtc.h"
#include "../libc/string.h"

static int active_tab = 1; // 0=Themes, 1=Date & Time, 2=Mouse, 3=Display, 4=About

static const char *tab_names[5] = {
    "Personalize",
    "Date & Time",
    "Mouse & Speed",
    "Display Specs",
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
        gfx_draw_string(cx, cy, "Real Image Wallpapers", COLOR_WHITE, COLOR_TRANSPARENT);
        gfx_draw_string(cx, cy + 18, "Select a high-resolution photographic wallpaper:", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

        int cur_theme = get_desktop_theme();
        int col_w = (cw - 12) / 2;

        for (int i = 0; i < 6; i++) {
            int col = i / 3;
            int row = i % 3;
            int bx = cx + (col * (col_w + 12));
            int by = cy + 44 + (row * 68);
            int is_sel = (cur_theme == i);

            gfx_fillrect(bx, by, col_w, 60, is_sel ? RGB(36, 40, 60) : RGB(26, 28, 40));
            gfx_drawrect(bx, by, col_w, 60, is_sel ? COLOR_ACCENT : COLOR_BORDER);

            unsigned int sw_col = wallpaper_get_color(i);

            if (is_sel) {
                gfx_fill_circle(bx + 16, by + 30, 8, sw_col);
                gfx_fill_circle(bx + 16, by + 30, 3, RGB(17, 17, 27));
            } else {
                gfx_fill_circle(bx + 16, by + 30, 7, sw_col);
            }

            gfx_draw_string(bx + 30, by + 12, get_theme_name(i), is_sel ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);
            gfx_draw_string(bx + 30, by + 32, get_theme_desc(i), COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

            if (is_sel) {
                gfx_draw_string(bx + col_w - 60, by + 12, "[Active]", COLOR_ACCENT, COLOR_TRANSPARENT);
            }
        }
    } else if (active_tab == 1) {
        // Tab 1: Date & Time Configuration
        gfx_draw_string(cx, cy, "Date & Time Configuration", COLOR_WHITE, COLOR_TRANSPARENT);
        gfx_draw_string(cx, cy + 18, "CMOS Real-Time Clock (RTC Ports 0x70 / 0x71):", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

        rtc_time_t t;
        rtc_get_datetime(&t);

        // 1. Current Live Date & Time Card
        int card_y = cy + 40;
        gfx_fillrect(cx, card_y, cw, 54, RGB(22, 24, 34));
        gfx_drawrect(cx, card_y, cw, 54, COLOR_ACCENT);

        char live_dt[48];
        snprintf(live_dt, sizeof(live_dt), "Date: %04u-%02u-%02u    Time: %02u:%02u:%02u",
                 t.year, t.month, t.day, t.hour, t.minute, t.second);
        gfx_draw_string(cx + 16, card_y + 10, live_dt, COLOR_WHITE, COLOR_TRANSPARENT);
        gfx_draw_string(cx + 16, card_y + 30, "Source: Hardware CMOS RTC (Synchronized with host)", COLOR_GREEN, COLOR_TRANSPARENT);

        // 2. Adjust Clock Time
        int time_y = card_y + 64;
        gfx_draw_string(cx, time_y, "Adjust Clock Time:", COLOR_WHITE, COLOR_TRANSPARENT);

        int tby = time_y + 20;
        int btn_h = 28;

        // Button: -1 Hr
        gfx_fillrect(cx, tby, 64, btn_h, RGB(36, 40, 60));
        gfx_drawrect(cx, tby, 64, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 12, tby + 6, "-1 Hr", COLOR_TEXT, COLOR_TRANSPARENT);

        // Button: +1 Hr
        gfx_fillrect(cx + 70, tby, 64, btn_h, RGB(36, 40, 60));
        gfx_drawrect(cx + 70, tby, 64, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 82, tby + 6, "+1 Hr", COLOR_TEXT, COLOR_TRANSPARENT);

        // Button: -1 Min
        gfx_fillrect(cx + 144, tby, 68, btn_h, RGB(36, 40, 60));
        gfx_drawrect(cx + 144, tby, 68, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 152, tby + 6, "-1 Min", COLOR_TEXT, COLOR_TRANSPARENT);

        // Button: +1 Min
        gfx_fillrect(cx + 218, tby, 68, btn_h, RGB(36, 40, 60));
        gfx_drawrect(cx + 218, tby, 68, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 226, tby + 6, "+1 Min", COLOR_TEXT, COLOR_TRANSPARENT);

        // Button: +10 Min
        gfx_fillrect(cx + 292, tby, 72, btn_h, RGB(36, 40, 60));
        gfx_drawrect(cx + 292, tby, 72, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 298, tby + 6, "+10 Min", COLOR_TEXT, COLOR_TRANSPARENT);

        // 3. Adjust Calendar Date
        int date_y = time_y + 56;
        gfx_draw_string(cx, date_y, "Adjust Calendar Date:", COLOR_WHITE, COLOR_TRANSPARENT);

        int dby = date_y + 20;

        // Button: -1 Day
        gfx_fillrect(cx, dby, 64, btn_h, RGB(36, 40, 60));
        gfx_drawrect(cx, dby, 64, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 8, dby + 6, "-1 Day", COLOR_TEXT, COLOR_TRANSPARENT);

        // Button: +1 Day
        gfx_fillrect(cx + 70, dby, 64, btn_h, RGB(36, 40, 60));
        gfx_drawrect(cx + 70, dby, 64, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 78, dby + 6, "+1 Day", COLOR_TEXT, COLOR_TRANSPARENT);

        // Button: -1 Mon
        gfx_fillrect(cx + 144, dby, 68, btn_h, RGB(36, 40, 60));
        gfx_drawrect(cx + 144, dby, 68, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 152, dby + 6, "-1 Mon", COLOR_TEXT, COLOR_TRANSPARENT);

        // Button: +1 Mon
        gfx_fillrect(cx + 218, dby, 68, btn_h, RGB(36, 40, 60));
        gfx_drawrect(cx + 218, dby, 68, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 226, dby + 6, "+1 Mon", COLOR_TEXT, COLOR_TRANSPARENT);

        // Button: +1 Year
        gfx_fillrect(cx + 292, dby, 72, btn_h, RGB(36, 40, 60));
        gfx_drawrect(cx + 292, dby, 72, btn_h, COLOR_BORDER);
        gfx_draw_string(cx + 298, dby + 6, "+1 Year", COLOR_TEXT, COLOR_TRANSPARENT);

        // 4. Hardware CMOS Sync Action
        int act_y = date_y + 56;
        gfx_draw_string(cx, act_y, "Hardware CMOS Synchronization:", COLOR_WHITE, COLOR_TRANSPARENT);

        int aby = act_y + 20;

        // Button: Sync from CMOS
        gfx_fillrect(cx, aby, 176, 32, RGB(42, 46, 68));
        gfx_drawrect(cx, aby, 176, 32, COLOR_ACCENT);
        gfx_draw_string(cx + 14, aby + 8, "⟳ Sync from CMOS", COLOR_WHITE, COLOR_TRANSPARENT);

        // Button: Save to CMOS
        gfx_fillrect(cx + 188, aby, 176, 32, RGB(42, 46, 68));
        gfx_drawrect(cx + 188, aby, 176, 32, COLOR_ACCENT);
        gfx_draw_string(cx + 202, aby + 8, "✓ Save to CMOS", COLOR_WHITE, COLOR_TRANSPARENT);

    } else if (active_tab == 2) {
        // Tab 2: Mouse & Speed
        gfx_draw_string(cx, cy, "Mouse Pointer & Sensitivity", COLOR_WHITE, COLOR_TRANSPARENT);
        gfx_draw_string(cx, cy + 20, "Adjust hardware cursor speed and acceleration curve:", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

        int cur_spd = mouse_get_speed();

        for (int i = 0; i < 3; i++) {
            int by = cy + 50 + (i * 58);
            int is_sel = (cur_spd == i);

            gfx_fillrect(cx, by, cw, 48, is_sel ? RGB(36, 40, 60) : RGB(28, 30, 44));
            gfx_drawrect(cx, by, cw, 48, is_sel ? COLOR_ACCENT : COLOR_BORDER);

            if (is_sel) {
                gfx_fill_circle(cx + 18, by + 24, 7, COLOR_ACCENT);
                gfx_fill_circle(cx + 18, by + 24, 3, RGB(17, 17, 27));
            } else {
                gfx_draw_circle(cx + 18, by + 24, 7, COLOR_BORDER);
            }

            gfx_draw_string(cx + 36, by + 10, speed_names[i], is_sel ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);
            gfx_draw_string(cx + 36, by + 28, speed_descs[i], COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

            if (is_sel) {
                gfx_draw_string(cx + cw - 70, by + 16, "[Active]", COLOR_ACCENT, COLOR_TRANSPARENT);
            }
        }

        // Live Cursor Coordinate Readout box
        int box_y = cy + 240;
        gfx_fillrect(cx, box_y, cw, 44, RGB(22, 24, 34));
        gfx_drawrect(cx, box_y, cw, 44, COLOR_BORDER);

        char pos_str[64];
        snprintf(pos_str, sizeof(pos_str), "Live Cursor Pos: (%d, %d)  |  Rate: 200 Hz", mouse_get_x(), mouse_get_y());
        gfx_draw_string(cx + 16, box_y + 14, pos_str, COLOR_WHITE, COLOR_TRANSPARENT);

    } else if (active_tab == 3) {
        // Tab 3: Display Specs
        gfx_draw_string(cx, cy, "Display & Graphics Configuration", COLOR_WHITE, COLOR_TRANSPARENT);
        gfx_draw_string(cx, cy + 20, "Hardware VESA VBE 2.0+ Video Subsystem:", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

        boot_info_t *bi = get_boot_info();
        char fb_str[48], res_str[48], pitch_str[48], bpp_str[48];

        snprintf(res_str, sizeof(res_str), "Resolution    :  %d x %d (SVGA TrueColor)", bi ? bi->width : 1024, bi ? bi->height : 768);
        snprintf(bpp_str, sizeof(bpp_str), "Color Depth   :  %d Bits Per Pixel (32bpp RGBA)", bi ? bi->bpp : 32);
        snprintf(pitch_str, sizeof(pitch_str), "Scanline Pitch:  %d Bytes Per Line", bi ? bi->pitch : 4096);
        snprintf(fb_str, sizeof(fb_str), "VRAM Base     :  0x%08X (Linear Framebuffer)", bi ? bi->fb_base : 0);

        int sy = cy + 60;
        gfx_draw_string(cx, sy,       res_str, COLOR_TEXT, COLOR_TRANSPARENT);
        gfx_draw_string(cx, sy + 26,  bpp_str, COLOR_TEXT, COLOR_TRANSPARENT);
        gfx_draw_string(cx, sy + 52,  pitch_str, COLOR_TEXT, COLOR_TRANSPARENT);
        gfx_draw_string(cx, sy + 78,  fb_str, COLOR_TEXT, COLOR_TRANSPARENT);
        gfx_draw_string(cx, sy + 104, "Double Buffer :  3.2 MB Hardware Backbuffer (0x200000)", COLOR_TEXT, COLOR_TRANSPARENT);
        gfx_draw_string(cx, sy + 130, "Fast Blit     :  Hardware Enhanced REP MOVSL Accelerated", COLOR_GREEN, COLOR_TRANSPARENT);

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
        gfx_draw_string(cx, ay,       "Architecture :  Intel x86 (32-bit Protected Mode)", COLOR_TEXT, COLOR_TRANSPARENT);
        gfx_draw_string(cx, ay + 24,  "Memory Model :  Ring 0 Flat Model with 1MB Stack (0x1FFFF0)", COLOR_TEXT, COLOR_TRANSPARENT);
        gfx_draw_string(cx, ay + 48,  "File Systems :  FAT16 Hard Disk & ISO9660 El Torito CD-ROM", COLOR_TEXT, COLOR_TRANSPARENT);
        gfx_draw_string(cx, ay + 72,  "Input Engine :  PS/2 Mouse (200Hz) & PS/2 Keyboard IRQ", COLOR_TEXT, COLOR_TRANSPARENT);
        gfx_draw_string(cx, ay + 96,  "Timer & RTC  :  100 Hz PIT & CMOS Hardware Real-Time Clock", COLOR_TEXT, COLOR_TRANSPARENT);

        char up_str[48];
        snprintf(up_str, sizeof(up_str), "System Uptime:  %u seconds (%u ticks)", pit_get_uptime_seconds(), pit_get_ticks());
        gfx_draw_string(cx, ay + 120, up_str, COLOR_GREEN, COLOR_TRANSPARENT);
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
        int col_w = (cw - 12) / 2;
        for (int i = 0; i < 6; i++) {
            int col = i / 3;
            int row = i % 3;
            int bx = cx + (col * (col_w + 12));
            int by = 60 + (row * 68);
            if (rx >= bx && rx <= bx + col_w && ry >= by && ry <= by + 60) {
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
            int by = 66 + (i * 58);
            if (rx >= cx && rx <= cx + cw && ry >= by && ry <= by + 48) {
                mouse_set_speed(i);
                return;
            }
        }
    }
}

void app_settings_open_tab(int tab) {
    if (tab >= 0 && tab < 5) active_tab = tab;
    app_settings_launch();
}

void app_settings_launch(void) {
    window_t *win = wm_create_window("Settings Control Panel", 200, 90, 560, 420, RGB(28, 30, 44));
    if (!win) return;
    win->draw_client = settings_draw;
    win->on_click = settings_click;
}
