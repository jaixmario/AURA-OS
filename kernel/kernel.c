#include "kernel.h"
#include "arch/idt.h"
#include "arch/pit.h"
#include "arch/kbd.h"
#include "arch/mouse.h"
#include "arch/rtc.h"
#include "fs/vfs.h"
#include "gfx/gfx.h"
#include "wm/wm.h"
#include "apps/apps.h"
#include "libc/string.h"

static int start_menu_open = 0;
static boot_info_t *g_boot_info = 0;
static int current_theme = 0; // 0=Nebula, 1=Midnight, 2=Cyberpunk, 3=Emerald

void set_desktop_theme(int theme) {
    if (theme >= 0 && theme <= 3) current_theme = theme;
}

int get_desktop_theme(void) {
    return current_theme;
}

boot_info_t *get_boot_info(void) {
    return g_boot_info;
}

static void draw_wallpaper(void) {
    int w = gfx_get_width();
    int h = gfx_get_height();

    unsigned int top_col = RGB(22, 24, 38);
    unsigned int bot_col = RGB(34, 37, 56);
    unsigned int accent_grid = RGB(48, 52, 78);

    if (current_theme == 1) { // Midnight Dark
        top_col = RGB(14, 16, 24);
        bot_col = RGB(22, 26, 38);
        accent_grid = RGB(34, 38, 56);
    } else if (current_theme == 2) { // Cyberpunk
        top_col = RGB(32, 16, 42);
        bot_col = RGB(16, 28, 52);
        accent_grid = RGB(55, 30, 72);
    } else if (current_theme == 3) { // Emerald Forest
        top_col = RGB(14, 28, 24);
        bot_col = RGB(20, 44, 38);
        accent_grid = RGB(28, 55, 45);
    }

    gfx_gradient_v(0, 0, w, h - 48, top_col, bot_col);

    // Subtle geometric horizon accent lines
    for (int y = h - 200; y < h - 48; y += 24) {
        int alpha_y = (y - (h - 200));
        unsigned int line_col = RGB(
            ((top_col >> 16) & 0xFF) + alpha_y / 10,
            ((top_col >> 8) & 0xFF) + alpha_y / 8,
            (top_col & 0xFF) + alpha_y / 6
        );
        gfx_draw_line(0, y, w, y, line_col);
    }

    // Centered Desktop Brand Watermark
    gfx_draw_string(w / 2 - 40, h / 2 - 30, "✦ AURA OS", accent_grid, COLOR_TRANSPARENT);
    gfx_draw_string(w / 2 - 80, h / 2 - 10, "Modern x86 Graphical System", accent_grid, COLOR_TRANSPARENT);
}

static void draw_taskbar(void) {
    int w = gfx_get_width();
    int h = gfx_get_height();
    int tb_y = h - 48;

    // Taskbar bar background
    gfx_fillrect(0, tb_y, w, 48, COLOR_TASKBAR);
    gfx_draw_line(0, tb_y, w - 1, tb_y, COLOR_BORDER);

    // Start Button ("✦ Aura") on the LEFT
    int mx = mouse_get_x();
    int my = mouse_get_y();
    int is_hover_start = (mx >= 8 && mx <= 100 && my >= tb_y + 6 && my <= tb_y + 42);

    unsigned int start_bg = is_hover_start ? COLOR_ACCENT : RGB(40, 43, 62);
    unsigned int start_fg = is_hover_start ? RGB(17, 17, 27) : COLOR_WHITE;

    gfx_fillrect(8, tb_y + 6, 92, 36, start_bg);
    gfx_drawrect(8, tb_y + 6, 92, 36, COLOR_BORDER);
    gfx_draw_string(20, tb_y + 16, "✦ Aura", start_fg, COLOR_TRANSPARENT);

    // System Tray (RIGHT SIDE)
    // RAM badge
    int ram_w = 88;
    int ram_x = w - 96;
    gfx_fillrect(ram_x, tb_y + 10, ram_w, 28, RGB(30, 32, 48));
    gfx_drawrect(ram_x, tb_y + 10, ram_w, 28, COLOR_BORDER);
    gfx_draw_string(ram_x + 6, tb_y + 16, "RAM: 3.2M", COLOR_ACCENT, COLOR_TRANSPARENT);

    // Live Status pulse dot
    gfx_fill_circle(w - 106, tb_y + 24, 4, COLOR_GREEN);

    // Live Date & Time Widget (on the RIGHT, directly next to tray)
    int clock_w = 180;
    int clock_x = w - 116 - clock_w;
    int is_hover_clock = (mx >= clock_x && mx <= clock_x + clock_w && my >= tb_y + 6 && my <= tb_y + 42);

    char dt_buf[32];
    rtc_time_t t;
    rtc_get_datetime(&t);
    snprintf(dt_buf, sizeof(dt_buf), "%04u-%02u-%02u %02u:%02u:%02u",
             t.year, t.month, t.day, t.hour, t.minute, t.second);

    gfx_fillrect(clock_x, tb_y + 6, clock_w, 36, is_hover_clock ? RGB(42, 46, 70) : RGB(30, 32, 48));
    gfx_drawrect(clock_x, tb_y + 6, clock_w, 36, is_hover_clock ? COLOR_ACCENT : COLOR_BORDER);
    gfx_draw_string(clock_x + 14, tb_y + 16, dt_buf, is_hover_clock ? COLOR_ACCENT : COLOR_WHITE, COLOR_TRANSPARENT);

    // Window items on taskbar (starts at x=108 after Start button, ends before Date/Time widget)
    int btn_x = 108;
    int tab_w = 98;
    int tab_stride = 102;
    window_t *active = wm_get_active_window();

    for (int i = 0; i < MAX_WINDOWS; i++) {
        window_t *win = wm_get_window_at_index(i);
        if (!win || !win->is_open) continue;

        if (btn_x + tab_w > clock_x - 6) break;

        int is_act = (win == active && !win->is_minimized);
        unsigned int tab_bg = is_act ? COLOR_ACTIVE_HEADER : RGB(30, 32, 48);
        unsigned int tab_fg = is_act ? COLOR_WHITE : COLOR_TEXT_MUTED;

        gfx_fillrect(btn_x, tb_y + 6, tab_w, 36, tab_bg);
        gfx_drawrect(btn_x, tb_y + 6, tab_w, 36, is_act ? COLOR_ACCENT : COLOR_BORDER);

        // Truncate title to fit button
        char title_buf[12];
        strncpy(title_buf, win->title, 11);
        title_buf[11] = '\0';
        gfx_draw_string(btn_x + 6, tb_y + 16, title_buf, tab_fg, COLOR_TRANSPARENT);

        btn_x += tab_stride;
    }
}

static void draw_start_menu(void) {
    if (!start_menu_open) return;

    int h = gfx_get_height();
    int menu_x = 8;
    int menu_w = 210;
    int menu_h = 324;
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
    const char *items[7] = {
        "> 1. File Explorer",
        "> 2. Terminal",
        "> 3. Calculator",
        "> 4. Canvas Paint",
        "> 5. Notes Editor",
        "> 6. Settings Panel",
        "> 7. System Info"
    };

    int mx = mouse_get_x();
    int my = mouse_get_y();

    for (int i = 0; i < 7; i++) {
        int item_y = menu_y + 42 + (i * 38);
        int is_hover = (mx >= menu_x + 6 && mx <= menu_x + menu_w - 6 &&
                        my >= item_y && my <= item_y + 32);

        if (is_hover) {
            gfx_fillrect(menu_x + 6, item_y, menu_w - 12, 32, RGB(49, 50, 68));
            gfx_drawrect(menu_x + 6, item_y, menu_w - 12, 32, COLOR_ACCENT);
        }

        gfx_draw_string(menu_x + 16, item_y + 8, items[i], is_hover ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);
    }
}

static void handle_desktop_click(int mx, int my) {
    int w = gfx_get_width();
    int h = gfx_get_height();
    int tb_y = h - 48;

    // Start Button clicked
    if (mx >= 8 && mx <= 100 && my >= tb_y + 6 && my <= tb_y + 42) {
        start_menu_open = !start_menu_open;
        return;
    }

    // Start Menu item clicked
    if (start_menu_open) {
        int menu_x = 8;
        int menu_w = 210;
        int menu_h = 324;
        int menu_y = h - 48 - menu_h - 8;

        if (mx >= menu_x && mx <= menu_x + menu_w && my >= menu_y && my <= menu_y + menu_h) {
            for (int i = 0; i < 7; i++) {
                int item_y = menu_y + 42 + (i * 38);
                if (my >= item_y && my <= item_y + 32) {
                    if (i == 0) app_files_launch();
                    else if (i == 1) app_term_launch();
                    else if (i == 2) app_calc_launch();
                    else if (i == 3) app_paint_launch();
                    else if (i == 4) app_notes_launch();
                    else if (i == 5) app_settings_launch();
                    else if (i == 6) app_sysinfo_launch();
                    start_menu_open = 0;
                    return;
                }
            }
        } else {
            // Click outside closes start menu
            start_menu_open = 0;
        }
    }

    int clock_w = 180;
    int clock_x = w - 116 - clock_w;

    // Date & Time widget on the right clicked -> Open Date & Time settings!
    if (mx >= clock_x && mx <= clock_x + clock_w && my >= tb_y + 6 && my <= tb_y + 42) {
        app_settings_open_tab(1);
        return;
    }

    // Taskbar window item clicked (starts after Start button at x=108)
    int tab_w = 98;
    int tab_stride = 102;
    if (my >= tb_y + 6 && my <= tb_y + 42 && mx >= 108 && mx < clock_x - 6) {
        int btn_x = 108;
        for (int i = 0; i < MAX_WINDOWS; i++) {
            window_t *win = wm_get_window_at_index(i);
            if (!win || !win->is_open) continue;

            if (btn_x + tab_w > clock_x - 6) break;

            if (mx >= btn_x && mx <= btn_x + tab_w) {
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
            btn_x += tab_stride;
        }
    }
}

void kernel_main(boot_info_t *bi) {
    if (bi->magic != 0x41555241) return;
    g_boot_info = bi;

    // 1. Initialize core architecture & interrupt descriptors
    idt_init();

    // 2. Initialize PIT timer (100 Hz = 10ms per tick)
    pit_init(100);

    // 3. Initialize PS/2 Keyboard
    kbd_init();

    // 4. Initialize PS/2 Mouse
    mouse_init(bi->width, bi->height);

    // 5. Initialize Hardware CMOS Real-Time Clock
    rtc_init();

    // 6. Initialize Virtual File System
    vfs_init();

    // Enable CPU interrupts!
    __asm__ volatile ("sti");

    // 7. Initialize Graphics Subsystem
    gfx_init(bi);

    // 8. Initialize Window Manager
    wm_init();

    // Launch default initial windows
    app_sysinfo_launch();
    app_calc_launch();
    app_term_launch();
    app_paint_launch();
    app_settings_launch();
    app_notes_launch();
    app_files_launch();

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
