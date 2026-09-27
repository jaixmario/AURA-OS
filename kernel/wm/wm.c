#include "wm.h"
#include "../gfx/gfx.h"
#include "../libc/string.h"

static window_t windows[MAX_WINDOWS];
static int z_order[MAX_WINDOWS];
static int window_count = 0;

static window_t *dragging_win = 0;
static window_t *client_drag_win = 0;
static int drag_off_x = 0;
static int drag_off_y = 0;
static int prev_mouse_btn = 0;

void wm_init(void) {
    window_count = 0;
    dragging_win = 0;
    client_drag_win = 0;
    prev_mouse_btn = 0;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        windows[i].id = i;
        windows[i].is_open = 0;
        windows[i].is_minimized = 0;
        windows[i].is_maximized = 0;
        z_order[i] = i;
    }
}

window_t *wm_create_window(const char *title, int x, int y, int w, int h, unsigned int bg_color) {
    if (window_count >= MAX_WINDOWS) return 0;

    int slot = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!windows[i].is_open) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return 0;

    window_t *win = &windows[slot];
    win->id = slot;
    strncpy(win->title, title, sizeof(win->title) - 1);
    win->x = x;
    win->y = y;
    win->width = w;
    win->height = h;
    win->prev_x = x;
    win->prev_y = y;
    win->prev_w = w;
    win->prev_h = h;
    win->is_open = 1;
    win->is_minimized = 0;
    win->is_maximized = 0;
    win->bg_color = bg_color;
    win->draw_client = 0;
    win->on_click = 0;
    win->on_drag = 0;
    win->on_release = 0;
    win->on_key = 0;
    win->user_data = 0;

    window_count++;
    wm_focus_window(win);
    return win;
}

void wm_close_window(window_t *win) {
    if (!win || !win->is_open) return;
    win->is_open = 0;
    window_count--;
    if (dragging_win == win) dragging_win = 0;
    if (client_drag_win == win) client_drag_win = 0;
}

void wm_focus_window(window_t *win) {
    if (!win || !win->is_open) return;

    int current_idx = -1;
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (z_order[i] == win->id) {
            current_idx = i;
            break;
        }
    }

    if (current_idx != -1 && current_idx != MAX_WINDOWS - 1) {
        int win_id = z_order[current_idx];
        for (int i = current_idx; i < MAX_WINDOWS - 1; i++) {
            z_order[i] = z_order[i + 1];
        }
        z_order[MAX_WINDOWS - 1] = win_id;
    }
}

void wm_minimize_window(window_t *win) {
    if (!win) return;
    win->is_minimized = 1;
}

void wm_maximize_window(window_t *win) {
    if (!win) return;
    if (win->is_maximized) {
        wm_restore_window(win);
    } else {
        win->prev_x = win->x;
        win->prev_y = win->y;
        win->prev_w = win->width;
        win->prev_h = win->height;
        win->x = 0;
        win->y = 0;
        win->width = gfx_get_width();
        win->height = gfx_get_height() - 48; // keep taskbar
        win->is_maximized = 1;
    }
}

void wm_restore_window(window_t *win) {
    if (!win) return;
    if (win->is_maximized) {
        win->x = win->prev_x;
        win->y = win->prev_y;
        win->width = win->prev_w;
        win->height = win->prev_h;
        win->is_maximized = 0;
    }
    win->is_minimized = 0;
    wm_focus_window(win);
}

window_t *wm_get_active_window(void) {
    for (int i = MAX_WINDOWS - 1; i >= 0; i--) {
        int id = z_order[i];
        if (windows[id].is_open && !windows[id].is_minimized) {
            return &windows[id];
        }
    }
    return 0;
}

window_t *wm_get_window_by_id(int id) {
    if (id < 0 || id >= MAX_WINDOWS) return 0;
    return &windows[id];
}

int wm_get_window_count(void) {
    return window_count;
}

window_t *wm_get_window_at_index(int idx) {
    if (idx < 0 || idx >= MAX_WINDOWS) return 0;
    return &windows[idx];
}

void wm_handle_mouse(int mx, int my, int btn_left, int left_clicked) {
    // 1. Mouse release event
    if (!btn_left && prev_mouse_btn) {
        if (client_drag_win) {
            if (client_drag_win->is_open && !client_drag_win->is_minimized && client_drag_win->on_release) {
                client_drag_win->on_release(client_drag_win, mx - client_drag_win->x, my - (client_drag_win->y + TITLEBAR_HEIGHT), 0);
            }
            client_drag_win = 0;
        }
        dragging_win = 0;
    }

    // 2. Window titlebar dragging
    if (dragging_win) {
        if (!btn_left) {
            dragging_win = 0;
        } else {
            dragging_win->x = mx - drag_off_x;
            dragging_win->y = my - drag_off_y;
            // Clamping
            if (dragging_win->y < 0) dragging_win->y = 0;
            if (dragging_win->y > gfx_get_height() - 48 - TITLEBAR_HEIGHT) {
                dragging_win->y = gfx_get_height() - 48 - TITLEBAR_HEIGHT;
            }
            prev_mouse_btn = btn_left;
            return;
        }
    }

    // 3. Client area continuous drag / hold
    if (btn_left && !left_clicked && client_drag_win) {
        if (client_drag_win->is_open && !client_drag_win->is_minimized && client_drag_win->on_drag) {
            client_drag_win->on_drag(client_drag_win, mx - client_drag_win->x, my - (client_drag_win->y + TITLEBAR_HEIGHT), 0);
        }
        prev_mouse_btn = btn_left;
        return;
    }

    // 4. Initial left click event
    if (left_clicked) {
        client_drag_win = 0;

        // Check windows from top of z-order down
        for (int i = MAX_WINDOWS - 1; i >= 0; i--) {
            int id = z_order[i];
            window_t *win = &windows[id];
            if (!win->is_open || win->is_minimized) continue;

            if (mx >= win->x && mx < win->x + win->width &&
                my >= win->y && my < win->y + win->height) {

                wm_focus_window(win);

                // Check if in title bar
                if (my < win->y + TITLEBAR_HEIGHT) {
                    // Close button: cx = win->x + 16, cy = win->y + 16, r = 6
                    int dx = mx - (win->x + 16);
                    int dy = my - (win->y + 16);
                    if (dx * dx + dy * dy <= 49) {
                        wm_close_window(win);
                        prev_mouse_btn = btn_left;
                        return;
                    }
                    // Minimize button: cx = win->x + 34
                    dx = mx - (win->x + 34);
                    if (dx * dx + dy * dy <= 49) {
                        wm_minimize_window(win);
                        prev_mouse_btn = btn_left;
                        return;
                    }
                    // Maximize button: cx = win->x + 52
                    dx = mx - (win->x + 52);
                    if (dx * dx + dy * dy <= 49) {
                        wm_maximize_window(win);
                        prev_mouse_btn = btn_left;
                        return;
                    }

                    // Otherwise drag window
                    if (!win->is_maximized) {
                        dragging_win = win;
                        drag_off_x = mx - win->x;
                        drag_off_y = my - win->y;
                    }
                    prev_mouse_btn = btn_left;
                    return;
                } else {
                    // In client area
                    client_drag_win = win;
                    if (win->on_click) {
                        win->on_click(win, mx - win->x, my - (win->y + TITLEBAR_HEIGHT), 0);
                    }
                    prev_mouse_btn = btn_left;
                    return;
                }
            }
        }
    }

    prev_mouse_btn = btn_left;
}

void wm_handle_key(char key) {
    window_t *active = wm_get_active_window();
    if (active && active->on_key) {
        active->on_key(active, key);
    }
}

void wm_render(void) {
    window_t *active = wm_get_active_window();

    // Render from bottom to top
    for (int i = 0; i < MAX_WINDOWS; i++) {
        int id = z_order[i];
        window_t *win = &windows[id];
        if (!win->is_open || win->is_minimized) continue;

        int is_act = (win == active);

        // Window shadow
        gfx_draw_shadow(win->x, win->y, win->width, win->height, 6);

        // Title bar
        if (is_act) {
            gfx_gradient_h(win->x, win->y, win->width, TITLEBAR_HEIGHT, COLOR_ACTIVE_HEADER, COLOR_HEADER);
        } else {
            gfx_fillrect(win->x, win->y, win->width, TITLEBAR_HEIGHT, COLOR_HEADER);
        }

        // Control buttons (traffic lights)
        gfx_fill_circle(win->x + 16, win->y + 16, 6, COLOR_BTN_CLOSE);
        gfx_fill_circle(win->x + 34, win->y + 16, 6, COLOR_BTN_MIN);
        gfx_fill_circle(win->x + 52, win->y + 16, 6, COLOR_BTN_MAX);

        // Title text (clipped to window width)
        unsigned int title_col = is_act ? COLOR_WHITE : COLOR_TEXT_MUTED;
        gfx_draw_string_clipped(win->x + 72, win->y + 8, win->title, title_col, COLOR_TRANSPARENT, win->width - 80);

        // Client background
        gfx_fillrect(win->x, win->y + TITLEBAR_HEIGHT, win->width, win->height - TITLEBAR_HEIGHT, win->bg_color);

        // Client border / separator
        gfx_draw_line(win->x, win->y + TITLEBAR_HEIGHT, win->x + win->width - 1, win->y + TITLEBAR_HEIGHT, COLOR_BORDER);

        // Client drawing
        if (win->draw_client) {
            gfx_set_clip(win->x + 1, win->y + TITLEBAR_HEIGHT + 1, win->width - 2, win->height - TITLEBAR_HEIGHT - 2);
            win->draw_client(win);
            gfx_reset_clip();
        }

        // Outer border
        unsigned int border_col = is_act ? COLOR_ACCENT : COLOR_BORDER;
        gfx_drawrect(win->x, win->y, win->width, win->height, border_col);
    }
}
