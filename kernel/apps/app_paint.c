#include "apps.h"
#include "../gfx/gfx.h"
#include "../libc/string.h"

#define CANVAS_W 480
#define CANVAS_H 280
#define CANVAS_BG RGB(22, 24, 35)

// Dedicated safe buffer in extended memory (6MB mark) - prevents stack collision
static unsigned int * const paint_canvas = (unsigned int *)0x600000;

static unsigned int active_color = COLOR_WHITE;
static int active_tool = 1; // 0=1px (Pencil), 1=3px (Brush), 2=6px (Marker), 3=Eraser
static int last_x = -1;
static int last_y = -1;
static int hover_x = 0;
static int hover_y = 0;

static const unsigned int palette_colors[16] = {
    // Row 1
    RGB(255, 255, 255), // White
    RGB(200, 205, 215), // Light Gray
    RGB(120, 125, 140), // Gray
    RGB(10, 10, 15),    // Black
    RGB(243, 68, 68),   // Red
    RGB(255, 110, 160), // Pink
    RGB(250, 140, 50),  // Orange
    RGB(250, 204, 21),  // Yellow
    // Row 2
    RGB(132, 204, 22),  // Lime
    RGB(34, 197, 94),   // Green
    RGB(16, 185, 129),  // Mint
    RGB(6, 182, 212),   // Cyan
    RGB(56, 189, 248),  // Sky Blue
    RGB(59, 130, 246),  // Blue
    RGB(168, 85, 247),  // Purple
    RGB(236, 72, 153)   // Magenta
};

static void paint_clear(void) {
    for (int i = 0; i < CANVAS_W * CANVAS_H; i++) {
        paint_canvas[i] = CANVAS_BG;
    }
}

static inline int tool_to_radius(int tool) {
    if (tool == 0) return 0; // 1px
    if (tool == 1) return 1; // 3px
    if (tool == 2) return 3; // 6px
    if (tool == 3) return 4; // Eraser 8px
    return 1;
}

static void canvas_draw_point(int cx, int cy, unsigned int col, int r) {
    if (r == 0) {
        if (cx >= 0 && cx < CANVAS_W && cy >= 0 && cy < CANVAS_H) {
            paint_canvas[cy * CANVAS_W + cx] = col;
        }
        return;
    }

    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (r > 1 && (dx * dx + dy * dy > r * r + 1)) continue;
            int px = cx + dx;
            int py = cy + dy;
            if (px >= 0 && px < CANVAS_W && py >= 0 && py < CANVAS_H) {
                paint_canvas[py * CANVAS_W + px] = col;
            }
        }
    }
}

static void canvas_draw_line(int x0, int y0, int x1, int y1, unsigned int col, int r) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        canvas_draw_point(x0, y0, col, r);
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

static void paint_stroke(int cx, int cy) {
    if (cx < 0 || cx >= CANVAS_W || cy < 0 || cy >= CANVAS_H) return;

    unsigned int col = (active_tool == 3) ? CANVAS_BG : active_color;
    int r = tool_to_radius(active_tool);

    if (last_x == -1 || last_y == -1) {
        canvas_draw_point(cx, cy, col, r);
    } else {
        canvas_draw_line(last_x, last_y, cx, cy, col, r);
    }
    last_x = cx;
    last_y = cy;
    hover_x = cx;
    hover_y = cy;
}

static void paint_draw(window_t *win) {
    int wx = win->x;
    int wy = win->y + TITLEBAR_HEIGHT;

    // 1. Toolbar Background
    gfx_fillrect(wx, wy, win->width, 60, RGB(32, 34, 48));
    gfx_draw_line(wx, wy + 59, wx + win->width - 1, wy + 59, COLOR_BORDER);

    // 2. Palette (2 rows of 8 swatches)
    int pal_x = wx + 14;
    int pal_y = wy + 8;
    for (int i = 0; i < 16; i++) {
        int col = i % 8;
        int row = i / 8;
        int sx = pal_x + (col * 24);
        int sy = pal_y + (row * 24);

        gfx_fillrect(sx, sy, 20, 20, palette_colors[i]);
        if (palette_colors[i] == active_color && active_tool != 3) {
            gfx_drawrect(sx - 2, sy - 2, 24, 24, COLOR_WHITE);
        } else {
            gfx_drawrect(sx, sy, 20, 20, COLOR_BORDER);
        }
    }

    // 3. Tool Selector Buttons
    int btn_x = pal_x + (8 * 24) + 12;
    const char *tool_labels[4] = { "1px", "3px", "6px", "Eraser" };
    int tool_widths[4] = { 34, 34, 34, 56 };

    int cur_bx = btn_x;
    for (int t = 0; t < 4; t++) {
        int bw = tool_widths[t];
        int is_act = (active_tool == t);
        unsigned int bg_col = is_act ? COLOR_ACCENT : RGB(46, 49, 70);
        unsigned int fg_col = is_act ? RGB(17, 17, 27) : COLOR_WHITE;

        gfx_fillrect(cur_bx, wy + 8, bw, 22, bg_col);
        gfx_drawrect(cur_bx, wy + 8, bw, 22, is_act ? COLOR_WHITE : COLOR_BORDER);
        gfx_draw_string(cur_bx + 6, wy + 12, tool_labels[t], fg_col, COLOR_TRANSPARENT);

        cur_bx += bw + 6;
    }

    // 4. Action Buttons (Clear)
    int clear_x = cur_bx + 4;
    gfx_fillrect(clear_x, wy + 8, 48, 22, RGB(180, 50, 60));
    gfx_drawrect(clear_x, wy + 8, 48, 22, COLOR_BORDER);
    gfx_draw_string(clear_x + 8, wy + 12, "Clear", COLOR_WHITE, COLOR_TRANSPARENT);

    // 5. Active Tool & Color Preview Swatch
    int prev_x = wx + win->width - 44;
    gfx_fillrect(prev_x, wy + 8, 30, 44, (active_tool == 3) ? CANVAS_BG : active_color);
    gfx_drawrect(prev_x - 1, wy + 7, 32, 46, COLOR_BORDER);

    // 6. Canvas Frame & Surface
    int canvas_x = wx + 16;
    int canvas_y = wy + 64;
    gfx_drawrect(canvas_x - 1, canvas_y - 1, CANVAS_W + 2, CANVAS_H + 2, COLOR_ACCENT);

    for (int y = 0; y < CANVAS_H; y++) {
        int src_row = y * CANVAS_W;
        for (int x = 0; x < CANVAS_W; x++) {
            gfx_putpixel(canvas_x + x, canvas_y + y, paint_canvas[src_row + x]);
        }
    }

    // 7. Status Bar at Bottom
    int status_y = wy + 350;
    gfx_fillrect(wx, status_y, win->width, 24, RGB(26, 28, 40));
    gfx_draw_line(wx, status_y, wx + win->width - 1, status_y, COLOR_BORDER);

    char stat_buf[64];
    const char *tool_name = (active_tool == 0) ? "Pencil 1px" :
                            (active_tool == 1) ? "Brush 3px" :
                            (active_tool == 2) ? "Marker 6px" : "Eraser";
    snprintf(stat_buf, sizeof(stat_buf), "Tool: %s  |  Pos: %d, %d  |  Size: 480x280", tool_name, hover_x, hover_y);
    gfx_draw_string(wx + 16, status_y + 5, stat_buf, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
}

static void paint_click(window_t *win, int rx, int ry, int btn) {
    (void)btn;
    // rx, ry are relative to client area (below title bar)

    // Palette Clicks (Y between 8 and 56)
    if (ry >= 8 && ry <= 56) {
        int pal_x = 14;
        int pal_y = 8;
        if (rx >= pal_x && rx < pal_x + (8 * 24)) {
            int col = (rx - pal_x) / 24;
            int row = (ry - pal_y) / 24;
            int idx = row * 8 + col;
            if (idx >= 0 && idx < 16) {
                active_color = palette_colors[idx];
                if (active_tool == 3) active_tool = 1; // exit eraser on color pick
                return;
            }
        }

        // Tool buttons
        int btn_x = pal_x + (8 * 24) + 12;
        int tool_widths[4] = { 34, 34, 34, 56 };
        int cur_bx = btn_x;
        for (int t = 0; t < 4; t++) {
            if (ry >= 8 && ry <= 30 && rx >= cur_bx && rx <= cur_bx + tool_widths[t]) {
                active_tool = t;
                return;
            }
            cur_bx += tool_widths[t] + 6;
        }

        // Clear button
        int clear_x = cur_bx + 4;
        if (ry >= 8 && ry <= 30 && rx >= clear_x && rx <= clear_x + 48) {
            paint_clear();
            return;
        }
    }

    // Canvas click
    int canvas_x = 16;
    int canvas_y = 64;
    int cx = rx - canvas_x;
    int cy = ry - canvas_y;
    if (cx >= 0 && cx < CANVAS_W && cy >= 0 && cy < CANVAS_H) {
        last_x = -1;
        last_y = -1;
        paint_stroke(cx, cy);
    }
}

static void paint_drag(window_t *win, int rx, int ry, int btn) {
    (void)win; (void)btn;
    int canvas_x = 16;
    int canvas_y = 64;
    int cx = rx - canvas_x;
    int cy = ry - canvas_y;
    if (cx >= 0 && cx < CANVAS_W && cy >= 0 && cy < CANVAS_H) {
        paint_stroke(cx, cy);
    } else {
        last_x = -1;
        last_y = -1;
    }
}

static void paint_release(window_t *win, int rx, int ry, int btn) {
    (void)win; (void)rx; (void)ry; (void)btn;
    last_x = -1;
    last_y = -1;
}

void app_paint_launch(void) {
    window_t *win = wm_create_window("Canvas Paint", 200, 130, 512, 408, RGB(26, 27, 38));
    if (!win) return;
    win->draw_client = paint_draw;
    win->on_click = paint_click;
    win->on_drag = paint_drag;
    win->on_release = paint_release;
    paint_clear();
}
