#include "kernel.h"
#include "arch/idt.h"
#include "arch/pit.h"
#include "arch/kbd.h"
#include "arch/mouse.h"
#include "gfx/gfx.h"
#include "wm/wm.h"
#include "apps/apps.h"
#include "libc/string.h"

static int start_menu_open = 0;

static void draw_wallpaper(void) {
    int w = gfx_get_width();
    int h = gfx_get_height();

    // Top to bottom rich nebula gradient
    gfx_gradient_v(0, 0, w, h - 48, RGB(22, 24, 38), RGB(34, 37, 56));

    // Subtle geometric horizon accent lines
    for (int y = h - 200; y < h - 48; y += 24) {
        int alpha_y = (y - (h - 200));
        unsigned int line_col = RGB(36 + alpha_y / 10, 40 + alpha_y / 8, 62 + alpha_y / 6);
        gfx_draw_line(0, y, w, y, line_col);
    }

    // Centered Desktop Brand Watermark
    gfx_draw_string(w / 2 - 40, h / 2 - 30, "✦ AURA OS", RGB(48, 52, 78), COLOR_TRANSPARENT);
    gfx_draw_string(w / 2 - 80, h / 2 - 10, "Modern x86 Graphical System", RGB(40, 44, 66), COLOR_TRANSPARENT);
}

static void draw_taskbar(void) {
    int w = gfx_get_width();
    int h = gfx_get_height();
    int tb_y = h - 48;

    // Taskbar bar background
    gfx_fillrect(0, tb_y, w, 48, COLOR_TASKBAR);
    gfx_draw_line(0, tb_y, w - 1, tb_y, COLOR_BORDER);

    // Start Button ("✦ Aura")
    int mx = mouse_get_x();
    int my = mouse_get_y();
    int is_hover_start = (mx >= 8 && mx <= 112 && my >= tb_y + 6 && my <= tb_y + 42);

    unsigned int start_bg = is_hover_start ? COLOR_ACCENT : RGB(40, 43, 62);
    unsigned int start_fg = is_hover_start ? RGB(17, 17, 27) : COLOR_WHITE;

    gfx_fillrect(8, tb_y + 6, 104, 36, start_bg);
    gfx_drawrect(8, tb_y + 6, 104, 36, COLOR_BORDER);
    gfx_draw_string(24, tb_y + 16, "✦ Aura", start_fg, COLOR_TRANSPARENT);

    // Window items on taskbar
    int btn_x = 124;
    window_t *active = wm_get_active_window();

    for (int i = 0; i < MAX_WINDOWS; i++) {
        window_t *win = wm_get_window_at_index(i);
        if (!win || !win->is_open) continue;

        int is_act = (win == active && !win->is_minimized);
        unsigned int tab_bg = is_act ? COLOR_ACTIVE_HEADER : RGB(30, 32, 48);
        unsigned int tab_fg = is_act ? COLOR_WHITE : COLOR_TEXT_MUTED;

        gfx_fillrect(btn_x, tb_y + 6, 140, 36, tab_bg);
        gfx_drawrect(btn_x, tb_y + 6, 140, 36, is_act ? COLOR_ACCENT : COLOR_BORDER);

        // Truncate title to fit button
        char title_buf[16];
        strncpy(title_buf, win->title, 15);
        title_buf[15] = '\0';
        gfx_draw_string(btn_x + 10, tb_y + 16, title_buf, tab_fg, COLOR_TRANSPARENT);

        btn_x += 148;
    }

    // System Tray (Right side)
    // Digital clock
    unsigned int total_sec = pit_get_uptime_seconds();
    unsigned int hrs = (total_sec / 3600) % 24;
    unsigned int mins = (total_sec / 60) % 60;
    unsigned int secs = total_sec % 60;

    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02u:%02u:%02u", hrs, mins, secs);

    int clock_x = w - 90;
    gfx_draw_string(clock_x, tb_y + 16, time_str, COLOR_WHITE, COLOR_TRANSPARENT);

    // RAM badge
    int ram_x = clock_x - 110;
    gfx_fillrect(ram_x, tb_y + 10, 96, 28, RGB(30, 32, 48));
    gfx_drawrect(ram_x, tb_y + 10, 96, 28, COLOR_BORDER);
    gfx_draw_string(ram_x + 8, tb_y + 16, "RAM: 3.2MB", COLOR_ACCENT, COLOR_TRANSPARENT);

    // Status dot
    gfx_fill_circle(w - 115, tb_y + 24, 4, COLOR_GREEN);
}

static void draw_start_menu(void) {
    if (!start_menu_open) return;

    int h = gfx_get_height();
    int menu_x = 8;
    int menu_w = 210;
    int menu_h = 220;
    int menu_y = h - 48 - menu_h - 8;

    // Drop shadow
    gfx_draw_shadow(menu_x, menu_y, menu_w, menu_h, 8);

    // Menu background
    gfx_fillrect(menu_x, menu_y, menu_w, menu_h, RGB(26, 28, 42));
    gfx_drawrect(menu_x, menu_y, menu_w, menu_h, COLOR_ACCENT);

    // Header banner
    gfx_gradient_h(menu_x + 1, menu_y + 1, menu_w - 2, 34, COLOR_ACTIVE_HEADER, COLOR_HEADER);
    gfx_draw_string(menu_x + 16, menu_y + 10, "Applications", COLOR_WHITE, COLOR_TRANSPARENT);

    // Menu items
    const char *items[4] = {
        "> 1. Terminal",
        "> 2. Calculator",
        "> 3. Canvas Paint",
        "> 4. System Info"
    };

    int mx = mouse_get_x();
    int my = mouse_get_y();

    for (int i = 0; i < 4; i++) {
        int item_y = menu_y + 44 + (i * 40);
        int is_hover = (mx >= menu_x + 6 && mx <= menu_x + menu_w - 6 &&
                        my >= item_y && my <= item_y + 34);

        if (is_hover) {
            gfx_fillrect(menu_x + 6, item_y, menu_w - 12, 34, RGB(49, 50, 68));
            gfx_drawrect(menu_x + 6, item_y, menu_w - 12, 34, COLOR_ACCENT);
        }

        gfx_draw_string(menu_x + 16, item_y + 10, items[i], is_hover ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);
    }
}

static void handle_desktop_click(int mx, int my) {
    int h = gfx_get_height();
    int tb_y = h - 48;

    // Start Button clicked
    if (mx >= 8 && mx <= 112 && my >= tb_y + 6 && my <= tb_y + 42) {
        start_menu_open = !start_menu_open;
        return;
    }

    // Start Menu item clicked
    if (start_menu_open) {
        int menu_x = 8;
        int menu_w = 210;
        int menu_h = 220;
        int menu_y = h - 48 - menu_h - 8;

        if (mx >= menu_x && mx <= menu_x + menu_w && my >= menu_y && my <= menu_y + menu_h) {
            for (int i = 0; i < 4; i++) {
                int item_y = menu_y + 44 + (i * 40);
                if (my >= item_y && my <= item_y + 34) {
                    if (i == 0) app_term_launch();
                    else if (i == 1) app_calc_launch();
                    else if (i == 2) app_paint_launch();
                    else if (i == 3) app_sysinfo_launch();
                    start_menu_open = 0;
                    return;
                }
            }
        } else {
            // Click outside closes start menu
            start_menu_open = 0;
        }
    }

    // Taskbar window item clicked
    if (my >= tb_y + 6 && my <= tb_y + 42 && mx >= 124) {
        int btn_x = 124;
        for (int i = 0; i < MAX_WINDOWS; i++) {
            window_t *win = wm_get_window_at_index(i);
            if (!win || !win->is_open) continue;

            if (mx >= btn_x && mx <= btn_x + 140) {
                if (win->is_minimized) {
                    wm_restore_window(win);
                } else {
                    window_t *act = wm_get_active_window();
                    if (act == win) {
                        wm_minimize_window(win);
                    } else {
                        wm_focus_window(win);
                    }
                }
                return;
            }
            btn_x += 148;
        }
    }
}

void kernel_main(boot_info_t *bi) {
    if (bi->magic != 0x41555241) return;

    // 1. Initialize core architecture & interrupt descriptors
    idt_init();

    // 2. Initialize PIT timer (100 Hz = 10ms per tick)
    pit_init(100);

    // 3. Initialize PS/2 Keyboard
    kbd_init();

    // 4. Initialize PS/2 Mouse
    mouse_init(bi->width, bi->height);

    // Enable CPU interrupts!
    __asm__ volatile ("sti");

    // 5. Initialize Graphics Subsystem
    gfx_init(bi);

    // 6. Initialize Window Manager
    wm_init();

    // Launch default initial windows
    app_sysinfo_launch();
    app_term_launch();
    app_calc_launch();

    // Main Desktop Event & Render Loop
    while (1) {
        // 1. Process keyboard events
        while (kbd_has_char()) {
            char key = kbd_get_char();
            wm_handle_key(key);
        }

        // 2. Process mouse events
        int mx = mouse_get_x();
        int my = mouse_get_y();
        int btn_left = mouse_is_left_down();
        int clicked = mouse_clicked(0);

        if (clicked) {
            handle_desktop_click(mx, my);
        }

        wm_handle_mouse(mx, my, btn_left, clicked);

        // 3. Render Desktop Scene
        draw_wallpaper();
        wm_render();
        draw_taskbar();
        draw_start_menu();

        // 4. Render Mouse Cursor on top
        gfx_draw_cursor(mx, my);

        // 5. Blit backbuffer to VRAM
        gfx_swap_buffers();

        // 6. Halt CPU until next interrupt (power efficient & smooth timing)
        __asm__ volatile ("hlt");
    }
}
