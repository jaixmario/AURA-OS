#ifndef GFX_H
#define GFX_H

#include "../kernel.h"

#define COLOR_TRANSPARENT 0xFFFFFFFF

#define RGB(r, g, b) ((((r) & 0xFF) << 16) | (((g) & 0xFF) << 8) | ((b) & 0xFF))

// Color definitions (Aesthetic Dark Theme)
#define COLOR_BG            RGB(24, 25, 38)
#define COLOR_CARD          RGB(36, 39, 58)
#define COLOR_HEADER        RGB(49, 50, 68)
#define COLOR_ACTIVE_HEADER RGB(69, 71, 90)
#define COLOR_ACCENT        RGB(138, 173, 244)
#define COLOR_ACCENT2       RGB(245, 189, 230)
#define COLOR_TEXT          RGB(202, 211, 245)
#define COLOR_TEXT_MUTED    RGB(128, 135, 162)
#define COLOR_TEXT_DARK     RGB(24, 25, 38)
#define COLOR_BORDER        RGB(88, 91, 112)
#define COLOR_TASKBAR       RGB(17, 17, 27)
#define COLOR_TASKBAR_HOVER RGB(49, 50, 68)
#define COLOR_SHADOW        RGB(10, 10, 15)

// Control button colors
#define COLOR_BTN_CLOSE     RGB(237, 135, 150)
#define COLOR_BTN_MIN       RGB(238, 212, 159)
#define COLOR_BTN_MAX       RGB(166, 218, 149)

// Accent Palette
#define COLOR_WHITE         RGB(255, 255, 255)
#define COLOR_BLACK         RGB(0, 0, 0)
#define COLOR_RED           RGB(237, 135, 150)
#define COLOR_GREEN         RGB(166, 218, 149)
#define COLOR_BLUE          RGB(138, 173, 244)
#define COLOR_YELLOW        RGB(238, 212, 159)
#define COLOR_CYAN          RGB(145, 215, 227)
#define COLOR_PURPLE        RGB(198, 160, 246)
#define COLOR_ORANGE        RGB(245, 169, 127)

void gfx_init(boot_info_t *bi);
void gfx_set_clip(int x, int y, int w, int h);
void gfx_reset_clip(void);

void gfx_clear(unsigned int color);
void gfx_putpixel(int x, int y, unsigned int color);
void gfx_fillrect(int x, int y, int w, int h, unsigned int color);
void gfx_drawrect(int x, int y, int w, int h, unsigned int color);
void gfx_draw_line(int x0, int y0, int x1, int y1, unsigned int color);
void gfx_draw_circle(int cx, int cy, int r, unsigned int color);
void gfx_fill_circle(int cx, int cy, int r, unsigned int color);
void gfx_gradient_v(int x, int y, int w, int h, unsigned int c_top, unsigned int c_bot);
void gfx_gradient_h(int x, int y, int w, int h, unsigned int c_left, unsigned int c_right);
void gfx_draw_shadow(int x, int y, int w, int h, int blur);

void gfx_draw_char(int x, int y, char c, unsigned int fg, unsigned int bg);
void gfx_draw_string(int x, int y, const char *str, unsigned int fg, unsigned int bg);
void gfx_draw_string_shadow(int x, int y, const char *str, unsigned int fg, unsigned int shadow_color);

void gfx_draw_cursor(int x, int y);
void gfx_swap_buffers(void);

int gfx_get_width(void);
int gfx_get_height(void);

#endif
