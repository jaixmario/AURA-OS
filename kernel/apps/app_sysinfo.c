#include "apps.h"
#include "../gfx/gfx.h"
#include "../arch/pit.h"
#include "../libc/string.h"

static void sysinfo_draw(window_t *win) {
    int sx = win->x + 24;
    int sy = win->y + TITLEBAR_HEIGHT + 20;

    // Header badge
    gfx_fillrect(sx, sy, 72, 72, RGB(49, 50, 68));
    gfx_drawrect(sx, sy, 72, 72, COLOR_ACCENT);
    gfx_draw_string(sx + 14, sy + 18, "✦", COLOR_ACCENT, COLOR_TRANSPARENT);
    gfx_draw_string(sx + 16, sy + 38, "OS", COLOR_WHITE, COLOR_TRANSPARENT);

    // Title info
    gfx_draw_string(sx + 88, sy + 12, "AuraOS Graphical System", COLOR_WHITE, COLOR_TRANSPARENT);
    gfx_draw_string(sx + 88, sy + 34, "Version 1.0.0 (Release-x86)", COLOR_ACCENT, COLOR_TRANSPARENT);
    gfx_draw_string(sx + 88, sy + 54, "Custom 32-Bit Bare-Metal Operating System", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // Separator line
    int line_y = sy + 90;
    gfx_draw_line(sx, line_y, sx + win->width - 48, line_y, COLOR_BORDER);

    // Hardware specifications table
    int ty = line_y + 16;
    gfx_draw_string(sx, ty,       "Architecture :  Intel x86 (32-bit Protected Mode)", COLOR_TEXT, COLOR_TRANSPARENT);
    gfx_draw_string(sx, ty + 22,  "Display Mode :  VESA VBE 2.0+ (1024x768 TrueColor)", COLOR_TEXT, COLOR_TRANSPARENT);
    gfx_draw_string(sx, ty + 44,  "VRAM Buffer  :  Double Buffered (Linear Framebuffer)", COLOR_TEXT, COLOR_TRANSPARENT);
    gfx_draw_string(sx, ty + 66,  "Input Devices:  PS/2 Mouse & PS/2 Keyboard", COLOR_TEXT, COLOR_TRANSPARENT);
    gfx_draw_string(sx, ty + 88,  "PIT Frequency:  100 Hz (10ms Quantum)", COLOR_TEXT, COLOR_TRANSPARENT);

    // Live Uptime
    char up_str[64];
    unsigned int sec = pit_get_uptime_seconds();
    unsigned int hrs = sec / 3600;
    unsigned int mins = (sec % 3600) / 60;
    unsigned int s = sec % 60;
    snprintf(up_str, sizeof(up_str), "System Uptime:  %02u:%02u:%02u  (%u ticks)", hrs, mins, s, pit_get_ticks());
    gfx_draw_string(sx, ty + 110, up_str, COLOR_GREEN, COLOR_TRANSPARENT);

    // Animated Activity bar
    int bar_y = ty + 140;
    gfx_draw_string(sx, bar_y, "CPU Activity :", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    int bar_x = sx + 130;
    int bar_w = 260;
    gfx_fillrect(bar_x, bar_y + 2, bar_w, 14, RGB(24, 25, 38));
    gfx_drawrect(bar_x, bar_y + 2, bar_w, 14, COLOR_BORDER);

    int progress = (pit_get_ticks() * 4) % (bar_w - 4);
    gfx_fillrect(bar_x + 2, bar_y + 4, progress, 10, COLOR_ACCENT);
}

void app_sysinfo_launch(void) {
    window_t *win = wm_create_window("System Information", 260, 140, 480, 360, RGB(30, 32, 48));
    if (!win) return;
    win->draw_client = sysinfo_draw;
}
