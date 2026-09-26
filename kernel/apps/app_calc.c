#include "apps.h"
#include "../gfx/gfx.h"
#include "../libc/string.h"

static char calc_display[32] = "0";
static int calc_acc = 0;
static char calc_op = 0;
static int calc_clear_on_next = 0;

static const char *calc_btn_labels[4][4] = {
    { "7", "8", "9", "/" },
    { "4", "5", "6", "*" },
    { "1", "2", "3", "-" },
    { "C", "0", "=", "+" }
};

static void calc_press(char c) {
    if (c >= '0' && c <= '9') {
        if (calc_clear_on_next || strcmp(calc_display, "0") == 0) {
            calc_display[0] = c;
            calc_display[1] = '\0';
            calc_clear_on_next = 0;
        } else {
            size_t len = strlen(calc_display);
            if (len < 12) {
                calc_display[len] = c;
                calc_display[len + 1] = '\0';
            }
        }
    } else if (c == 'C') {
        strcpy(calc_display, "0");
        calc_acc = 0;
        calc_op = 0;
        calc_clear_on_next = 0;
    } else if (c == '+' || c == '-' || c == '*' || c == '/') {
        // parse int from display
        int val = 0;
        for (int i = 0; calc_display[i]; i++) {
            val = val * 10 + (calc_display[i] - '0');
        }
        calc_acc = val;
        calc_op = c;
        calc_clear_on_next = 1;
    } else if (c == '=') {
        if (calc_op != 0) {
            int val = 0;
            for (int i = 0; calc_display[i]; i++) {
                val = val * 10 + (calc_display[i] - '0');
            }
            int res = 0;
            if (calc_op == '+') res = calc_acc + val;
            else if (calc_op == '-') res = calc_acc - val;
            else if (calc_op == '*') res = calc_acc * val;
            else if (calc_op == '/') {
                if (val != 0) res = calc_acc / val;
                else res = 0;
            }
            itoa(res, calc_display, 10);
            calc_acc = res;
            calc_op = 0;
            calc_clear_on_next = 1;
        }
    }
}

static void calc_draw(window_t *win) {
    int start_x = win->x + 16;
    int start_y = win->y + TITLEBAR_HEIGHT + 16;

    // Display box
    gfx_fillrect(start_x, start_y, 240, 48, RGB(24, 25, 38));
    gfx_drawrect(start_x, start_y, 240, 48, COLOR_BORDER);

    // Operator hint
    if (calc_op != 0) {
        char op_str[2] = { calc_op, '\0' };
        gfx_draw_string(start_x + 8, start_y + 16, op_str, COLOR_ACCENT, COLOR_TRANSPARENT);
    }

    // Display text (right aligned)
    int text_len = strlen(calc_display);
    int text_x = start_x + 240 - (text_len * 8) - 12;
    gfx_draw_string(text_x, start_y + 16, calc_display, COLOR_WHITE, COLOR_TRANSPARENT);

    // Buttons (4x4)
    int btn_start_y = start_y + 64;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            int bx = start_x + (col * 60);
            int by = btn_start_y + (row * 52);

            const char *label = calc_btn_labels[row][col];
            unsigned int bg_col = RGB(49, 50, 68);
            unsigned int text_col = COLOR_WHITE;

            if (col == 3 || label[0] == '=') {
                bg_col = COLOR_ACCENT;
                text_col = RGB(17, 17, 27);
            } else if (label[0] == 'C') {
                bg_col = COLOR_RED;
                text_col = COLOR_WHITE;
            }

            gfx_fillrect(bx, by, 52, 44, bg_col);
            gfx_drawrect(bx, by, 52, 44, COLOR_BORDER);
            gfx_draw_string(bx + 22, by + 14, label, text_col, COLOR_TRANSPARENT);
        }
    }
}

static void calc_click(window_t *win, int rel_x, int rel_y, int btn) {
    (void)win; (void)btn;
    // Client area relative coordinates
    int btn_start_y = 16 + 64;
    int start_x = 16;

    if (rel_y >= btn_start_y && rel_y < btn_start_y + (4 * 52) &&
        rel_x >= start_x && rel_x < start_x + 240) {

        int row = (rel_y - btn_start_y) / 52;
        int col = (rel_x - start_x) / 60;

        if (row >= 0 && row < 4 && col >= 0 && col < 4) {
            calc_press(calc_btn_labels[row][col][0]);
        }
    }
}

static void calc_key(window_t *win, char key) {
    (void)win;
    if ((key >= '0' && key <= '9') || key == '+' || key == '-' || key == '*' || key == '/' || key == '=' || key == 'C' || key == 'c') {
        if (key == 'c') key = 'C';
        calc_press(key);
    } else if (key == '\n') {
        calc_press('=');
    }
}

void app_calc_launch(void) {
    window_t *win = wm_create_window("Calculator", 660, 100, 274, 350, RGB(36, 39, 58));
    if (!win) return;
    win->draw_client = calc_draw;
    win->on_click = calc_click;
    win->on_key = calc_key;
}
