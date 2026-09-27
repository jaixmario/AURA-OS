#include "apps.h"
#include "../kernel.h"
#include "../gfx/gfx.h"
#include "../arch/pit.h"
#include "../libc/string.h"

static void sysinfo_draw(window_t *win) {
    int sx = win->x + 20;
    int sy = win->y + TITLEBAR_HEIGHT + 14;
    int max_w = win->width - 40;

    // 1. Header badge & branding
    gfx_fillrect(sx, sy, 58, 58, RGB(42, 46, 68));
    gfx_drawrect(sx, sy, 58, 58, COLOR_ACCENT);
    gfx_draw_string(sx + 20, sy + 12, "✦", COLOR_ACCENT, COLOR_TRANSPARENT);
    gfx_draw_string(sx + 14, sy + 32, "AURA", COLOR_WHITE, COLOR_TRANSPARENT);

    // Title info
    gfx_draw_string(sx + 72, sy + 4, "AuraOS Graphical System", COLOR_WHITE, COLOR_TRANSPARENT);
    gfx_draw_string(sx + 72, sy + 24, "Version 1.2.0 (i686 Protected Mode)", COLOR_ACCENT, COLOR_TRANSPARENT);
    const char *status_str = sys_is_installed() ? "Status: Installed on Hard Disk (ATA)" : "Status: Live Media (CD-ROM ISO)";
    gfx_draw_string(sx + 72, sy + 42, status_str, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // 2. Separator line
    int line_y = sy + 70;
    gfx_draw_line(sx, line_y, sx + max_w, line_y, COLOR_BORDER);

    // 3. Hardware & Architecture specifications table
    int ty = line_y + 12;
    char u_line[64], h_line[64];
    snprintf(u_line, sizeof(u_line), "Active User  :  %s (%s)", sys_get_username(), sys_get_fullname());
    snprintf(h_line, sizeof(h_line), "Computer Name:  %s", sys_get_hostname());

    gfx_draw_string_clipped(sx, ty,       u_line, COLOR_WHITE, COLOR_TRANSPARENT, max_w);
    gfx_draw_string_clipped(sx, ty + 20,  h_line, COLOR_WHITE, COLOR_TRANSPARENT, max_w);
    gfx_draw_string_clipped(sx, ty + 40,  "Architecture :  Intel x86 (32-bit Protected Mode)", COLOR_TEXT, COLOR_TRANSPARENT, max_w);
    gfx_draw_string_clipped(sx, ty + 60,  "Display Mode :  1024x768 TrueColor (32bpp RGBA)", COLOR_TEXT, COLOR_TRANSPARENT, max_w);
    gfx_draw_string_clipped(sx, ty + 80,  "Graphics VRAM:  Hardware Double Buffered (REP MOVSL)", COLOR_TEXT, COLOR_TRANSPARENT, max_w);
    gfx_draw_string_clipped(sx, ty + 100, "Input Drivers:  PS/2 Mouse (200Hz) & PS/2 Keyboard", COLOR_TEXT, COLOR_TRANSPARENT, max_w);
    gfx_draw_string_clipped(sx, ty + 120, "Timer & Clock:  100 Hz PIT & CMOS Hardware RTC", COLOR_TEXT, COLOR_TRANSPARENT, max_w);

    // Live Uptime
    char up_str[64];
    unsigned int sec = pit_get_uptime_seconds();
    unsigned int hrs = sec / 3600;
    unsigned int mins = (sec % 3600) / 60;
    unsigned int s = sec % 60;
    snprintf(up_str, sizeof(up_str), "System Uptime:  %02u:%02u:%02u  (%u ticks)", hrs, mins, s, pit_get_ticks());
    gfx_draw_string_clipped(sx, ty + 140, up_str, COLOR_GREEN, COLOR_TRANSPARENT, max_w);

    // 4. Activity bar separator
    int act_sep_y = ty + 164;
    gfx_draw_line(sx, act_sep_y, sx + max_w, act_sep_y, COLOR_BORDER);

    // Animated Activity bar
    int bar_y = act_sep_y + 12;
    gfx_draw_string(sx, bar_y, "CPU Activity :", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    int bar_x = sx + 130;
    int bar_w = max_w - 130;
    gfx_fillrect(bar_x, bar_y + 2, bar_w, 14, RGB(22, 24, 34));
    gfx_drawrect(bar_x, bar_y + 2, bar_w, 14, COLOR_BORDER);

    int progress = (pit_get_ticks() * 4) % (bar_w - 4);
    gfx_fillrect(bar_x + 2, bar_y + 4, progress, 10, COLOR_ACCENT);
}

void app_sysinfo_launch(void) {
    window_t *win = wm_create_window("System Information", 250, 120, 500, 390, RGB(30, 32, 48));
    if (!win) return;
    win->draw_client = sysinfo_draw;
}
