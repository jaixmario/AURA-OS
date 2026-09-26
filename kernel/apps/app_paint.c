#include "apps.h"
#include "../gfx/gfx.h"
#include "../arch/mouse.h"
#include "../libc/string.h"

#define CANVAS_W 460
#define CANVAS_H 280

static unsigned int paint_canvas[CANVAS_W * CANVAS_H];
static unsigned int active_color = COLOR_WHITE;

static const unsigned int palette_colors[8] = {
    COLOR_WHITE,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_YELLOW,
    COLOR_CYAN,
    COLOR_PURPLE,
    COLOR_ORANGE
};

static void paint_clear(void) {
    for (int i = 0; i < CANVAS_W * CANVAS_H; i++) {
        paint_canvas[i] = RGB(20, 20, 30);
    }
}

static void paint_draw(window_t *win) {
    int start_x = win->x + 16;
    int start_y = win->y + TITLEBAR_HEIGHT + 12;

    // Palette bar
    for (int i = 0; i < 8; i++) {
        int px = start_x + (i * 32);
        gfx_fillrect(px, start_y, 24, 24, palette_colors[i]);
        if (palette_colors[i] == active_color) {
            gfx_drawrect(px - 2, start_y - 2, 28, 28, COLOR_WHITE);
        } else {
            gfx_drawrect(px, start_y, 24, 24, COLOR_BORDER);
        }
    }

    // Clear button
    int clear_btn_x = start_x + (8 * 32) + 16;
    gfx_fillrect(clear_btn_x, start_y, 60, 24, RGB(49, 50, 68));
    gfx_drawrect(clear_btn_x, start_y, 60, 24, COLOR_BORDER);
    gfx_draw_string(clear_btn_x + 10, start_y + 4, "Clear", COLOR_WHITE, COLOR_TRANSPARENT);

    // Canvas border and pixels
    int cy = start_y + 36;
    gfx_drawrect(start_x - 1, cy - 1, CANVAS_W + 2, CANVAS_H + 2, COLOR_BORDER);

    // Blit canvas pixels
    for (int y = 0; y < CANVAS_H; y++) {
        for (int x = 0; x < CANVAS_W; x++) {
            gfx_putpixel(start_x + x, cy + y, paint_canvas[y * CANVAS_W + x]);
        }
    }

    // If mouse left button is held inside canvas, draw!
    if (mouse_is_left_down()) {
        int mx = mouse_get_x();
        int my = mouse_get_y();
        int rel_x = mx - start_x;
        int rel_y = my - cy;

        if (rel_x >= 2 && rel_x < CANVAS_W - 2 && rel_y >= 2 && rel_y < CANVAS_H - 2) {
            // Draw brush 3x3
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    int px = rel_x + dx;
                    int py = rel_y + dy;
                    if (px >= 0 && px < CANVAS_W && py >= 0 && py < CANVAS_H) {
                        paint_canvas[py * CANVAS_W + px] = active_color;
                    }
                }
            }
        }
    }
}

static void paint_click(window_t *win, int rel_x, int rel_y, int btn) {
    (void)win; (void)btn;
    int start_x = 16;
    int start_y = 12;

    // Check palette clicks
    if (rel_y >= start_y && rel_y < start_y + 24) {
        for (int i = 0; i < 8; i++) {
            int px = start_x + (i * 32);
            if (rel_x >= px && rel_x < px + 24) {
                active_color = palette_colors[i];
                return;
            }
        }

        // Clear button
        int clear_btn_x = start_x + (8 * 32) + 16;
        if (rel_x >= clear_btn_x && rel_x < clear_btn_x + 60) {
            paint_clear();
            return;
        }
    }
}

void app_paint_launch(void) {
    window_t *win = wm_create_window("Canvas Paint", 220, 160, 492, 380, RGB(30, 30, 46));
    if (!win) return;
    win->draw_client = paint_draw;
    win->on_click = paint_click;
    paint_clear();
}
