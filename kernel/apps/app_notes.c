#include "apps.h"
#include "../gfx/gfx.h"
#include "../arch/pit.h"
#include "../libc/string.h"

#define NOTES_MAX_CHARS 2048
#define NOTES_MAX_LINES 32
#define NOTES_LINE_LEN  64

static char notes_buffer[NOTES_MAX_CHARS] =
    "✦ Welcome to AuraOS Notes Editor!\n"
    "==================================\n"
    "• System: AuraOS 32-bit x86 Protected Mode\n"
    "• Display: 1024x768 TrueColor Framebuffer\n"
    "• Window Manager: Floating Draggable Windows\n"
    "• Apps: Terminal, Calculator, Paint, Notes\n"
    "\n"
    "Quick Tips:\n"
    "• Type anywhere to edit this note\n"
    "• Use Backspace to delete text\n"
    "• Press Enter for newlines\n"
    "• Drag titlebars to move windows\n"
    "• Enjoy your custom bare-metal OS!\n";

static int notes_len = 0;
static int notes_cursor = 0;

static void notes_init_buffer(void) {
    notes_len = strlen(notes_buffer);
    notes_cursor = notes_len;
}

static void notes_draw(window_t *win) {
    int sx = win->x + 12;
    int sy = win->y + TITLEBAR_HEIGHT + 10;
    int client_w = win->width - 24;
    int client_h = win->height - TITLEBAR_HEIGHT - 42;

    // Gutter & text background
    gfx_fillrect(win->x + 8, sy - 2, 36, client_h, RGB(22, 24, 34));
    gfx_draw_line(win->x + 44, sy - 2, win->x + 44, sy - 2 + client_h, COLOR_BORDER);

    // Render lines with line numbers
    int line_idx = 1;
    int text_x = win->x + 52;
    int cur_y = sy;
    int cur_x = text_x;

    char line_num_str[8];
    snprintf(line_num_str, sizeof(line_num_str), "%2d", line_idx);
    gfx_draw_string(win->x + 16, cur_y, line_num_str, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    int cursor_draw_x = cur_x;
    int cursor_draw_y = cur_y;

    for (int i = 0; i < notes_len; i++) {
        if (i == notes_cursor) {
            cursor_draw_x = cur_x;
            cursor_draw_y = cur_y;
        }

        char c = notes_buffer[i];
        if (c == '\n') {
            line_idx++;
            cur_y += 18;
            cur_x = text_x;
            if (cur_y + 16 < sy + client_h) {
                snprintf(line_num_str, sizeof(line_num_str), "%2d", line_idx);
                gfx_draw_string(win->x + 16, cur_y, line_num_str, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
            }
        } else {
            if (cur_y + 16 < sy + client_h && cur_x + 8 < win->x + client_w) {
                char ch_str[2] = { c, '\0' };
                gfx_draw_string(cur_x, cur_y, ch_str, COLOR_WHITE, COLOR_TRANSPARENT);
            }
            cur_x += 8;
        }
    }

    if (notes_cursor == notes_len) {
        cursor_draw_x = cur_x;
        cursor_draw_y = cur_y;
    }

    // Blinking cursor
    if ((pit_get_ticks() / 30) % 2 == 0) {
        if (cursor_draw_y + 16 < sy + client_h) {
            gfx_fillrect(cursor_draw_x, cursor_draw_y, 8, 16, COLOR_ACCENT);
        }
    }

    // Status Bar
    int status_y = win->y + win->height - 24;
    gfx_fillrect(win->x, status_y, win->width, 24, RGB(26, 28, 40));
    gfx_draw_line(win->x, status_y, win->x + win->width - 1, status_y, COLOR_BORDER);

    char stat_str[64];
    snprintf(stat_str, sizeof(stat_str), "Lines: %d  |  Chars: %d  |  Encoding: ASCII", line_idx, notes_len);
    gfx_draw_string(win->x + 14, status_y + 5, stat_str, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
}

static void notes_key(window_t *win, char key) {
    (void)win;
    if (key == '\b') {
        if (notes_cursor > 0 && notes_len > 0) {
            for (int i = notes_cursor - 1; i < notes_len; i++) {
                notes_buffer[i] = notes_buffer[i + 1];
            }
            notes_cursor--;
            notes_len--;
        }
    } else if (key == '\n' || (key >= 32 && key <= 126)) {
        if (notes_len < NOTES_MAX_CHARS - 2) {
            for (int i = notes_len; i >= notes_cursor; i--) {
                notes_buffer[i + 1] = notes_buffer[i];
            }
            notes_buffer[notes_cursor] = key;
            notes_cursor++;
            notes_len++;
            notes_buffer[notes_len] = '\0';
        }
    }
}

void app_notes_launch(void) {
    notes_init_buffer();
    window_t *win = wm_create_window("Notes Editor", 240, 160, 480, 340, RGB(26, 28, 40));
    if (!win) return;
    win->draw_client = notes_draw;
    win->on_key = notes_key;
}
