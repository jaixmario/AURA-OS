#ifndef WM_H
#define WM_H

#define MAX_WINDOWS 12
#define TITLEBAR_HEIGHT 32

typedef struct window {
    int id;
    char title[48];
    int x, y;
    int width, height;
    int is_open;
    int is_minimized;
    int is_maximized;
    int prev_x, prev_y, prev_w, prev_h;
    unsigned int bg_color;

    void (*draw_client)(struct window *win);
    void (*on_click)(struct window *win, int cx, int cy, int btn);
    void (*on_drag)(struct window *win, int cx, int cy, int btn);
    void (*on_release)(struct window *win, int cx, int cy, int btn);
    void (*on_key)(struct window *win, char key);

    void *user_data;
} window_t;

void wm_init(void);
window_t *wm_create_window(const char *title, int x, int y, int w, int h, unsigned int bg_color);
void wm_close_window(window_t *win);
void wm_focus_window(window_t *win);
void wm_minimize_window(window_t *win);
void wm_maximize_window(window_t *win);
void wm_restore_window(window_t *win);

void wm_handle_mouse(int mx, int my, int btn_left, int left_clicked);
void wm_handle_key(char key);
void wm_render(void);

window_t *wm_get_active_window(void);
window_t *wm_get_window_by_id(int id);
int wm_get_window_count(void);
window_t *wm_get_window_at_index(int idx);
void wm_on_resolution_change(int new_w, int new_h);

#endif
