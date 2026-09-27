#include "gfx.h"
#include "font.h"
#include "../libc/string.h"

static boot_info_t *boot_info = 0;
static unsigned int *backbuffer = (unsigned int *)0x200000; // 2MB mark

static int clip_x0 = 0;
static int clip_y0 = 0;
static int clip_x1 = 1024;
static int clip_y1 = 768;

void gfx_init(boot_info_t *bi) {
    boot_info = bi;
    clip_x0 = 0;
    clip_y0 = 0;
    clip_x1 = bi->width;
    clip_y1 = bi->height;
    memset(backbuffer, 0, bi->width * bi->height * sizeof(unsigned int));
}

int gfx_get_width(void) {
    return boot_info ? boot_info->width : 1024;
}

int gfx_get_height(void) {
    return boot_info ? boot_info->height : 768;
}

void gfx_set_clip(int x, int y, int w, int h) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > boot_info->width) w = boot_info->width - x;
    if (y + h > boot_info->height) h = boot_info->height - y;
    clip_x0 = x;
    clip_y0 = y;
    clip_x1 = x + w;
    clip_y1 = y + h;
}

void gfx_reset_clip(void) {
    clip_x0 = 0;
    clip_y0 = 0;
    clip_x1 = boot_info->width;
    clip_y1 = boot_info->height;
}

void gfx_clear(unsigned int color) {
    if (!boot_info) return;
    int total = boot_info->width * boot_info->height;
    unsigned int *d = backbuffer;
    __asm__ volatile (
        "cld\n"
        "rep stosl\n"
        : "+D"(d), "+c"(total)
        : "a"(color)
        : "memory"
    );
}

void gfx_putpixel(int x, int y, unsigned int color) {
    if (x < clip_x0 || x >= clip_x1 || y < clip_y0 || y >= clip_y1) return;
    backbuffer[y * boot_info->width + x] = color;
}

void gfx_fillrect(int x, int y, int w, int h, unsigned int color) {
    int x0 = (x < clip_x0) ? clip_x0 : x;
    int y0 = (y < clip_y0) ? clip_y0 : y;
    int x1 = (x + w > clip_x1) ? clip_x1 : (x + w);
    int y1 = (y + h > clip_y1) ? clip_y1 : (y + h);

    if (x0 >= x1 || y0 >= y1) return;
    int count = x1 - x0;

    for (int cy = y0; cy < y1; cy++) {
        unsigned int *dest = backbuffer + (cy * boot_info->width + x0);
        int cnt = count;
        __asm__ volatile (
            "cld\n"
            "rep stosl\n"
            : "+D"(dest), "+c"(cnt)
            : "a"(color)
            : "memory"
        );
    }
}

void gfx_drawrect(int x, int y, int w, int h, unsigned int color) {
    for (int cx = x; cx < x + w; cx++) {
        gfx_putpixel(cx, y, color);
        gfx_putpixel(cx, y + h - 1, color);
    }
    for (int cy = y; cy < y + h; cy++) {
        gfx_putpixel(x, cy, color);
        gfx_putpixel(x + w - 1, cy, color);
    }
}

void gfx_draw_line(int x0, int y0, int x1, int y1, unsigned int color) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        gfx_putpixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void gfx_draw_circle(int cx, int cy, int r, unsigned int color) {
    int x = 0, y = r;
    int d = 3 - 2 * r;
    while (y >= x) {
        gfx_putpixel(cx + x, cy + y, color);
        gfx_putpixel(cx - x, cy + y, color);
        gfx_putpixel(cx + x, cy - y, color);
        gfx_putpixel(cx - x, cy - y, color);
        gfx_putpixel(cx + y, cy + x, color);
        gfx_putpixel(cx - y, cy + x, color);
        gfx_putpixel(cx + y, cy - x, color);
        gfx_putpixel(cx - y, cy - x, color);
        if (d < 0) {
            d += 4 * x + 6;
        } else {
            d += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

void gfx_fill_circle(int cx, int cy, int r, unsigned int color) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                gfx_putpixel(cx + x, cy + y, color);
            }
        }
    }
}

static inline unsigned int blend_component(unsigned int c0, unsigned int c1, int step, int total) {
    return c0 + ((int)(c1 - c0) * step) / total;
}

void gfx_gradient_v(int x, int y, int w, int h, unsigned int c_top, unsigned int c_bot) {
    int r0 = (c_top >> 16) & 0xFF, g0 = (c_top >> 8) & 0xFF, b0 = c_top & 0xFF;
    int r1 = (c_bot >> 16) & 0xFF, g1 = (c_bot >> 8) & 0xFF, b1 = c_bot & 0xFF;

    for (int cy = 0; cy < h; cy++) {
        unsigned int r = blend_component(r0, r1, cy, h);
        unsigned int g = blend_component(g0, g1, cy, h);
        unsigned int b = blend_component(b0, b1, cy, h);
        unsigned int col = RGB(r, g, b);
        for (int cx = x; cx < x + w; cx++) {
            gfx_putpixel(cx, y + cy, col);
        }
    }
}

void gfx_gradient_h(int x, int y, int w, int h, unsigned int c_left, unsigned int c_right) {
    int r0 = (c_left >> 16) & 0xFF, g0 = (c_left >> 8) & 0xFF, b0 = c_left & 0xFF;
    int r1 = (c_right >> 16) & 0xFF, g1 = (c_right >> 8) & 0xFF, b1 = c_right & 0xFF;

    for (int cx = 0; cx < w; cx++) {
        unsigned int r = blend_component(r0, r1, cx, w);
        unsigned int g = blend_component(g0, g1, cx, w);
        unsigned int b = blend_component(b0, b1, cx, w);
        unsigned int col = RGB(r, g, b);
        for (int cy = y; cy < y + h; cy++) {
            gfx_putpixel(x + cx, cy, col);
        }
    }
}

void gfx_draw_shadow(int x, int y, int w, int h, int blur) {
    for (int sy = y + blur; sy < y + h + blur; sy++) {
        for (int sx = x + blur; sx < x + w + blur; sx++) {
            if (sx >= x + w || sy >= y + h) {
                gfx_putpixel(sx, sy, COLOR_SHADOW);
            }
        }
    }
}

void gfx_draw_char(int x, int y, char c, unsigned int fg, unsigned int bg) {
    if (c < 32 || c > 127) c = ' ';
    const unsigned char *glyph = font8x16[(unsigned char)c - 32];

    for (int row = 0; row < 16; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                gfx_putpixel(x + col, y + row, fg);
            } else if (bg != COLOR_TRANSPARENT) {
                gfx_putpixel(x + col, y + row, bg);
            }
        }
    }
}

void gfx_draw_string(int x, int y, const char *str, unsigned int fg, unsigned int bg) {
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            cx = x;
            y += 18;
        } else {
            gfx_draw_char(cx, y, *str, fg, bg);
            cx += 8;
        }
        str++;
    }
}

void gfx_draw_string_clipped(int x, int y, const char *str, unsigned int fg, unsigned int bg, int max_w) {
    if (!str || max_w <= 0) return;
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            cx = x;
            y += 18;
        } else {
            if (cx + 8 > x + max_w) {
                if (cx >= x + 16) {
                    gfx_draw_char(cx - 16, y, '.', fg, bg);
                    gfx_draw_char(cx - 8, y, '.', fg, bg);
                }
                break;
            }
            gfx_draw_char(cx, y, *str, fg, bg);
            cx += 8;
        }
        str++;
    }
}

void gfx_draw_string_shadow(int x, int y, const char *str, unsigned int fg, unsigned int shadow_color) {
    gfx_draw_string(x + 1, y + 1, str, shadow_color, COLOR_TRANSPARENT);
    gfx_draw_string(x, y, str, fg, COLOR_TRANSPARENT);
}

// Sleek modern mouse cursor
static const unsigned char cursor_bitmap[16][12] = {
    { 1,0,0,0,0,0,0,0,0,0,0,0 },
    { 1,1,0,0,0,0,0,0,0,0,0,0 },
    { 1,2,1,0,0,0,0,0,0,0,0,0 },
    { 1,2,2,1,0,0,0,0,0,0,0,0 },
    { 1,2,2,2,1,0,0,0,0,0,0,0 },
    { 1,2,2,2,2,1,0,0,0,0,0,0 },
    { 1,2,2,2,2,2,1,0,0,0,0,0 },
    { 1,2,2,2,2,2,2,1,0,0,0,0 },
    { 1,2,2,2,2,2,2,2,1,0,0,0 },
    { 1,2,2,2,2,1,1,1,1,1,0,0 },
    { 1,2,2,1,2,1,0,0,0,0,0,0 },
    { 1,2,1,0,1,2,1,0,0,0,0,0 },
    { 1,1,0,0,1,2,1,0,0,0,0,0 },
    { 1,0,0,0,0,1,2,1,0,0,0,0 },
    { 0,0,0,0,0,1,2,1,0,0,0,0 },
    { 0,0,0,0,0,0,1,1,0,0,0,0 }
};

void gfx_draw_cursor(int x, int y) {
    for (int r = 0; r < 16; r++) {
        for (int c = 0; c < 12; c++) {
            unsigned char v = cursor_bitmap[r][c];
            if (v == 1) {
                gfx_putpixel(x + c, y + r, RGB(10, 10, 15)); // Dark outline
            } else if (v == 2) {
                if (r < 4 && c < 4) {
                    gfx_putpixel(x + c, y + r, COLOR_CYAN); // Cyan accent at tip
                } else {
                    gfx_putpixel(x + c, y + r, COLOR_WHITE); // Pure white fill
                }
            }
        }
    }
}

void gfx_swap_buffers(void) {
    if (!boot_info) return;

    unsigned char *vram = (unsigned char *)boot_info->fb_base;
    int w = boot_info->width;
    int h = boot_info->height;
    int pitch = boot_info->pitch;
    int bpp = boot_info->bpp;

    if (bpp == 24) {
        for (int y = 0; y < h; y++) {
            unsigned char *dest_row = vram + (y * pitch);
            unsigned int *src_row = backbuffer + (y * w);
            for (int x = 0; x < w; x++) {
                unsigned int c = src_row[x];
                dest_row[x * 3 + 0] = (unsigned char)(c & 0xFF);         // B
                dest_row[x * 3 + 1] = (unsigned char)((c >> 8) & 0xFF);  // G
                dest_row[x * 3 + 2] = (unsigned char)((c >> 16) & 0xFF); // R
            }
        }
    } else if (bpp == 32) {
        if (pitch == w * 4) {
            unsigned int *d = (unsigned int *)vram;
            unsigned int *s = backbuffer;
            int total = w * h;
            __asm__ volatile (
                "cld\n"
                "rep movsl\n"
                : "+D"(d), "+S"(s), "+c"(total)
                :
                : "memory"
            );
        } else {
            for (int y = 0; y < h; y++) {
                unsigned int *dest_row = (unsigned int *)(vram + (y * pitch));
                unsigned int *src_row = backbuffer + (y * w);
                int count = w;
                __asm__ volatile (
                    "cld\n"
                    "rep movsl\n"
                    : "+D"(dest_row), "+S"(src_row), "+c"(count)
                    :
                    : "memory"
                );
            }
        }
    }
}
