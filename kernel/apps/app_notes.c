#include "apps.h"
#include "../fs/vfs.h"
#include "../gfx/gfx.h"
#include "../arch/pit.h"
#include "../libc/string.h"

#define NOTES_MAX_CHARS 2048

static char notes_buffer[NOTES_MAX_CHARS];
static int notes_len = 0;
static int notes_cursor = 0;
static char current_filename[VFS_MAX_FILENAME] = "NOTES.TXT";
static int is_modified = 0;
static char status_msg[64] = "Ready";
static int file_picker_open = 0;
static window_t *notes_win = 0;
static int new_doc_counter = 1;

static const char *default_notes_content =
    "==================================================\n"
    "        AuraOS Notes & Document Editor\n"
    "==================================================\n"
    "• File System : Virtual File System (RamFS)\n"
    "• Features    : Live Open, Save, Save As, New\n"
    "• Storage Path: C:\\AuraOS\\Storage\n\n"
    "Quick Controls:\n"
    "• Click [Open File] to browse & load any system file\n"
    "• Click [Save] to write your changes back to disk\n"
    "• Click [New] to start a blank document\n"
    "• Type directly to edit, use Backspace to erase\n"
    "• Click anywhere in the editor to position cursor\n";

static void notes_init_buffer(void) {
    vfs_file_t *f = vfs_find(current_filename);
    if (f) {
        strncpy(notes_buffer, f->data, NOTES_MAX_CHARS - 1);
        notes_buffer[NOTES_MAX_CHARS - 1] = '\0';
    } else {
        strncpy(notes_buffer, default_notes_content, NOTES_MAX_CHARS - 1);
        notes_buffer[NOTES_MAX_CHARS - 1] = '\0';
    }
    notes_len = strlen(notes_buffer);
    notes_cursor = notes_len;
    is_modified = 0;
    file_picker_open = 0;
    snprintf(status_msg, sizeof(status_msg), "Opened %s (%d B)", current_filename, notes_len);
}

static void notes_save_current(void) {
    vfs_file_t *f = vfs_find(current_filename);
    if (f) {
        int res = vfs_write_file(current_filename, notes_buffer, notes_len);
        if (res == 0) {
            is_modified = 0;
            snprintf(status_msg, sizeof(status_msg), "Saved to %s (%d B)", current_filename, notes_len);
        } else if (res == -2) {
            snprintf(status_msg, sizeof(status_msg), "Error: %s is read-only!", current_filename);
        } else {
            snprintf(status_msg, sizeof(status_msg), "Error: Failed to save file.");
        }
    } else {
        int res = vfs_create_file(current_filename, notes_buffer, notes_len, FS_ATTR_USER);
        if (res == 0) {
            is_modified = 0;
            snprintf(status_msg, sizeof(status_msg), "Created & saved %s (%d B)", current_filename, notes_len);
        } else {
            snprintf(status_msg, sizeof(status_msg), "Error: Failed to create file.");
        }
    }
}

static void notes_save_as_new(void) {
    char new_name[VFS_MAX_FILENAME];
    for (int i = 1; i <= 99; i++) {
        snprintf(new_name, sizeof(new_name), "DOC%d.TXT", new_doc_counter++);
        if (!vfs_find(new_name)) break;
    }

    int res = vfs_create_file(new_name, notes_buffer, notes_len, FS_ATTR_USER);
    if (res == 0) {
        strncpy(current_filename, new_name, sizeof(current_filename) - 1);
        current_filename[sizeof(current_filename) - 1] = '\0';
        is_modified = 0;
        snprintf(status_msg, sizeof(status_msg), "Saved as %s (%d B)", new_name, notes_len);

        if (notes_win) {
            char title[64];
            snprintf(title, sizeof(title), "Notes - %s", new_name);
            strncpy(notes_win->title, title, sizeof(notes_win->title) - 1);
        }
    } else {
        snprintf(status_msg, sizeof(status_msg), "Error: Storage full or write error.");
    }
}

static void notes_draw(window_t *win) {
    int wx = win->x;
    int wy = win->y + TITLEBAR_HEIGHT;
    int client_w = win->width;
    int client_h = win->height - TITLEBAR_HEIGHT;

    // 1. Top Action Toolbar
    int tb_h = 34;
    gfx_fillrect(wx, wy, client_w, tb_h, RGB(22, 25, 38));
    gfx_draw_line(wx, wy + tb_h, wx + client_w - 1, wy + tb_h, COLOR_BORDER);

    // [ + New ] button
    gfx_fillrect(wx + 8, wy + 4, 60, 26, RGB(34, 38, 56));
    gfx_drawrect(wx + 8, wy + 4, 60, 26, RGB(60, 68, 96));
    gfx_draw_string(wx + 16, wy + 9, "+ New", COLOR_WHITE, COLOR_TRANSPARENT);

    // [ Open File ] button
    unsigned int open_bg = file_picker_open ? COLOR_ACCENT : RGB(34, 38, 56);
    gfx_fillrect(wx + 74, wy + 4, 88, 26, open_bg);
    gfx_drawrect(wx + 74, wy + 4, 88, 26, COLOR_ACCENT);
    gfx_draw_string(wx + 82, wy + 9, "Open File", COLOR_WHITE, COLOR_TRANSPARENT);

    // [ Save ] button
    unsigned int save_bg = is_modified ? RGB(40, 75, 120) : RGB(34, 38, 56);
    unsigned int save_border = is_modified ? COLOR_ACCENT : RGB(60, 68, 96);
    gfx_fillrect(wx + 168, wy + 4, 58, 26, save_bg);
    gfx_drawrect(wx + 168, wy + 4, 58, 26, save_border);
    gfx_draw_string(wx + 178, wy + 9, "Save", is_modified ? COLOR_WHITE : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // [ Save As ] button
    gfx_fillrect(wx + 232, wy + 4, 72, 26, RGB(34, 38, 56));
    gfx_drawrect(wx + 232, wy + 4, 72, 26, RGB(60, 68, 96));
    gfx_draw_string(wx + 240, wy + 9, "Save As", COLOR_WHITE, COLOR_TRANSPARENT);

    // Active File Badge (Right side of toolbar)
    int badge_w = 170;
    int badge_x = wx + client_w - badge_w - 10;
    if (badge_x > wx + 310) {
        gfx_fillrect(badge_x, wy + 4, badge_w, 26, RGB(16, 18, 28));
        gfx_drawrect(badge_x, wy + 4, badge_w, 26, COLOR_BORDER);

        char badge_str[40];
        if (is_modified) {
            snprintf(badge_str, sizeof(badge_str), "* %s (Mod)", current_filename);
            gfx_draw_string(badge_x + 8, wy + 9, badge_str, RGB(249, 226, 175), COLOR_TRANSPARENT);
        } else {
            snprintf(badge_str, sizeof(badge_str), "[ok] %s", current_filename);
            gfx_draw_string(badge_x + 8, wy + 9, badge_str, RGB(166, 227, 161), COLOR_TRANSPARENT);
        }
    }

    // 2. Editor Body: Gutter & Text Area
    int text_area_y = wy + tb_h + 1;
    int text_area_h = client_h - tb_h - 25;

    // Gutter Background
    int gutter_w = 40;
    gfx_fillrect(wx, text_area_y, gutter_w, text_area_h, RGB(18, 20, 30));
    gfx_draw_line(wx + gutter_w, text_area_y, wx + gutter_w, text_area_y + text_area_h - 1, COLOR_BORDER);

    // Text lines with line numbering
    int line_idx = 1;
    int text_x = wx + gutter_w + 10;
    int cur_y = text_area_y + 8;
    int cur_x = text_x;

    char line_num_str[8];
    snprintf(line_num_str, sizeof(line_num_str), "%2d", line_idx);
    gfx_draw_string(wx + 10, cur_y, line_num_str, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

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
            if (cur_y + 16 < text_area_y + text_area_h) {
                snprintf(line_num_str, sizeof(line_num_str), "%2d", line_idx);
                gfx_draw_string(wx + 10, cur_y, line_num_str, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
            }
        } else {
            if (cur_y + 16 < text_area_y + text_area_h && cur_x + 8 < wx + client_w - 8) {
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
    if ((pit_get_ticks() / 30) % 2 == 0 && !file_picker_open) {
        if (cursor_draw_y + 16 < text_area_y + text_area_h) {
            gfx_fillrect(cursor_draw_x, cursor_draw_y, 8, 16, COLOR_ACCENT);
        }
    }

    // 3. Status Bar
    int status_y = wy + client_h - 24;
    gfx_fillrect(wx, status_y, client_w, 24, RGB(18, 20, 30));
    gfx_draw_line(wx, status_y, wx + client_w - 1, status_y, COLOR_BORDER);

    char stat_left[48];
    snprintf(stat_left, sizeof(stat_left), "File: %s", current_filename);
    gfx_draw_string(wx + 10, status_y + 5, stat_left, COLOR_WHITE, COLOR_TRANSPARENT);

    char stat_mid[48];
    snprintf(stat_mid, sizeof(stat_mid), "Ln %d | %d Chars", line_idx, notes_len);
    gfx_draw_string(wx + 150, status_y + 5, stat_mid, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    gfx_draw_string(wx + 300, status_y + 5, status_msg, COLOR_ACCENT, COLOR_TRANSPARENT);

    // 4. File Picker Modal Overlay (when open)
    if (file_picker_open) {
        int px = 28;
        int py = tb_h + 10;
        int pw = client_w - 56;
        int ph = client_h - tb_h - 38;

        // Modal Frame
        gfx_fillrect(wx + px, wy + py, pw, ph, RGB(22, 25, 38));
        gfx_drawrect(wx + px, wy + py, pw, ph, COLOR_ACCENT);

        // Modal Header
        gfx_fillrect(wx + px, wy + py, pw, 28, RGB(32, 38, 58));
        gfx_draw_line(wx + px, wy + py + 28, wx + px + pw, wy + py + 28, COLOR_BORDER);
        gfx_draw_string(wx + px + 12, wy + py + 6, "Open File from Storage (VFS)", COLOR_WHITE, COLOR_TRANSPARENT);

        // Close 'X' button
        gfx_draw_string(wx + px + pw - 20, wy + py + 6, "X", RGB(240, 120, 130), COLOR_TRANSPARENT);

        // Column Titles inside modal
        int list_top = wy + py + 34;
        gfx_draw_string(wx + px + 14,  list_top, "Type", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
        gfx_draw_string(wx + px + 70,  list_top, "Filename", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
        gfx_draw_string(wx + px + 210, list_top, "Size", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
        gfx_draw_string(wx + px + 280, list_top, "Date", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
        gfx_draw_line(wx + px + 10, list_top + 18, wx + px + pw - 10, list_top + 18, COLOR_BORDER);

        int total_files = vfs_get_count();
        int max_picker_rows = 7;
        for (int i = 0; i < total_files && i < max_picker_rows; i++) {
            vfs_file_t *f = vfs_get_at(i);
            if (!f) continue;

            int row_y = list_top + 22 + (i * 24);
            unsigned int row_bg = (i % 2 == 0) ? RGB(26, 29, 44) : RGB(22, 25, 38);
            gfx_fillrect(wx + px + 10, row_y, pw - 20, 22, row_bg);

            const char *badge = "[TXT]";
            unsigned int badge_col = RGB(137, 220, 235);
            if (strstr(f->name, ".CFG")) { badge = "[CFG]"; badge_col = RGB(249, 226, 175); }
            else if (strstr(f->name, ".SYS")) { badge = "[SYS]"; badge_col = RGB(203, 166, 247); }
            else if (strstr(f->name, ".SH"))  { badge = "[SH ]"; badge_col = RGB(166, 227, 161); }
            else if (strstr(f->name, ".LOG")) { badge = "[LOG]"; badge_col = RGB(148, 226, 213); }

            gfx_draw_string(wx + px + 14,  row_y + 3, badge, badge_col, COLOR_TRANSPARENT);
            gfx_draw_string(wx + px + 70,  row_y + 3, f->name, COLOR_WHITE, COLOR_TRANSPARENT);

            char sz_str[16];
            snprintf(sz_str, sizeof(sz_str), "%u B", f->size);
            gfx_draw_string(wx + px + 210, row_y + 3, sz_str, COLOR_TEXT, COLOR_TRANSPARENT);

            char dt_str[16];
            snprintf(dt_str, sizeof(dt_str), "%04u-%02u-%02u", f->created_year, f->created_month, f->created_day);
            gfx_draw_string(wx + px + 280, row_y + 3, dt_str, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
        }

        // Cancel Button at bottom right of modal
        int cb_x = wx + px + pw - 84;
        int cb_y = wy + py + ph - 30;
        gfx_fillrect(cb_x, cb_y, 74, 22, RGB(42, 46, 68));
        gfx_drawrect(cb_x, cb_y, 74, 22, COLOR_BORDER);
        gfx_draw_string(cb_x + 14, cb_y + 3, "Cancel", COLOR_WHITE, COLOR_TRANSPARENT);
    }
}

static void notes_click(window_t *win, int rx, int ry, int btn) {
    (void)btn;
    int client_w = win->width;
    int client_h = win->height - TITLEBAR_HEIGHT;
    int tb_h = 34;

    // Handle File Picker Modal clicks
    if (file_picker_open) {
        int px = 28;
        int py = tb_h + 10;
        int pw = client_w - 56;
        int ph = client_h - tb_h - 38;

        // Close 'X' button
        if (rx >= px + pw - 24 && rx <= px + pw && ry >= py && ry <= py + 28) {
            file_picker_open = 0;
            return;
        }

        // Cancel button
        int cb_x = px + pw - 84;
        int cb_y = py + ph - 30;
        if (rx >= cb_x && rx <= cb_x + 74 && ry >= cb_y && ry <= cb_y + 22) {
            file_picker_open = 0;
            return;
        }

        // File rows selection
        int list_top = py + 34;
        int total_files = vfs_get_count();
        int max_picker_rows = 7;
        for (int i = 0; i < total_files && i < max_picker_rows; i++) {
            int row_y = list_top + 22 + (i * 24);
            if (rx >= px + 10 && rx <= px + pw - 10 && ry >= row_y && ry <= row_y + 22) {
                vfs_file_t *f = vfs_get_at(i);
                if (f) {
                    strncpy(notes_buffer, f->data, NOTES_MAX_CHARS - 1);
                    notes_buffer[NOTES_MAX_CHARS - 1] = '\0';
                    notes_len = strlen(notes_buffer);
                    notes_cursor = notes_len;
                    strncpy(current_filename, f->name, sizeof(current_filename) - 1);
                    current_filename[sizeof(current_filename) - 1] = '\0';
                    is_modified = 0;
                    file_picker_open = 0;
                    snprintf(status_msg, sizeof(status_msg), "Opened %s (%d B)", f->name, notes_len);

                    char title[64];
                    snprintf(title, sizeof(title), "Notes - %s", f->name);
                    strncpy(win->title, title, sizeof(win->title) - 1);
                }
                return;
            }
        }
        return;
    }

    // Handle Top Toolbar Buttons
    if (ry >= 4 && ry <= 30) {
        // [ + New ]
        if (rx >= 8 && rx <= 68) {
            notes_buffer[0] = '\0';
            notes_len = 0;
            notes_cursor = 0;
            strncpy(current_filename, "UNTITLED.TXT", sizeof(current_filename) - 1);
            is_modified = 1;
            snprintf(status_msg, sizeof(status_msg), "New blank document");
            strncpy(win->title, "Notes - UNTITLED.TXT", sizeof(win->title) - 1);
            return;
        }

        // [ Open File ]
        if (rx >= 74 && rx <= 162) {
            file_picker_open = 1;
            return;
        }

        // [ Save ]
        if (rx >= 168 && rx <= 226) {
            notes_save_current();
            return;
        }

        // [ Save As ]
        if (rx >= 232 && rx <= 304) {
            notes_save_as_new();
            return;
        }
    }

    // Handle clicking inside text area to position cursor
    if (ry > tb_h && ry < client_h - 24 && rx > 48) {
        int clicked_line = (ry - (tb_h + 9)) / 18 + 1;
        int clicked_col = (rx - 50) / 8;
        if (clicked_line < 1) clicked_line = 1;
        if (clicked_col < 0) clicked_col = 0;

        int cur_line = 1;
        int cur_col = 0;
        int new_cursor = notes_len;

        for (int i = 0; i < notes_len; i++) {
            if (cur_line == clicked_line && cur_col >= clicked_col) {
                new_cursor = i;
                break;
            }
            if (notes_buffer[i] == '\n') {
                if (cur_line == clicked_line) {
                    new_cursor = i;
                    break;
                }
                cur_line++;
                cur_col = 0;
            } else {
                cur_col++;
            }
        }
        notes_cursor = new_cursor;
    }
}

static void notes_key(window_t *win, char key) {
    (void)win;

    if (file_picker_open) {
        if (key == 27) { // Escape
            file_picker_open = 0;
        }
        return;
    }

    if (key == '\b') {
        if (notes_cursor > 0 && notes_len > 0) {
            for (int i = notes_cursor - 1; i < notes_len; i++) {
                notes_buffer[i] = notes_buffer[i + 1];
            }
            notes_cursor--;
            notes_len--;
            is_modified = 1;
            snprintf(status_msg, sizeof(status_msg), "Editing...");
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
            is_modified = 1;
            snprintf(status_msg, sizeof(status_msg), "Editing...");
        }
    }
}

void app_notes_launch(void) {
    if (notes_win && notes_win->is_open) {
        wm_restore_window(notes_win);
        wm_focus_window(notes_win);
        return;
    }
    notes_init_buffer();
    char title[64];
    snprintf(title, sizeof(title), "Notes - %s", current_filename);
    notes_win = wm_create_window(title, 220, 130, 540, 380, RGB(22, 25, 38));
    if (!notes_win) return;
    notes_win->draw_client = notes_draw;
    notes_win->on_key = notes_key;
    notes_win->on_click = notes_click;
}

void app_notes_open_file(const char *filename) {
    if (!filename) return;
    vfs_file_t *f = vfs_find(filename);
    if (!f) return;

    strncpy(notes_buffer, f->data, NOTES_MAX_CHARS - 1);
    notes_buffer[NOTES_MAX_CHARS - 1] = '\0';
    notes_len = strlen(notes_buffer);
    notes_cursor = notes_len;
    strncpy(current_filename, f->name, sizeof(current_filename) - 1);
    current_filename[sizeof(current_filename) - 1] = '\0';
    is_modified = 0;
    file_picker_open = 0;
    snprintf(status_msg, sizeof(status_msg), "Opened %s (%u B)", f->name, f->size);

    char title[64];
    snprintf(title, sizeof(title), "Notes - %s", f->name);

    if (notes_win && notes_win->is_open) {
        strncpy(notes_win->title, title, sizeof(notes_win->title) - 1);
        wm_restore_window(notes_win);
        wm_focus_window(notes_win);
    } else {
        notes_win = wm_create_window(title, 220, 130, 540, 380, RGB(22, 25, 38));
        if (!notes_win) return;
        notes_win->draw_client = notes_draw;
        notes_win->on_key = notes_key;
        notes_win->on_click = notes_click;
    }
}

void app_notes_load_text(const char *text) {
    if (!text) return;
    strncpy(notes_buffer, text, NOTES_MAX_CHARS - 1);
    notes_buffer[NOTES_MAX_CHARS - 1] = '\0';
    notes_len = strlen(notes_buffer);
    notes_cursor = notes_len;
    strncpy(current_filename, "CLIPBOARD.TXT", sizeof(current_filename) - 1);
    is_modified = 1;
    file_picker_open = 0;
    snprintf(status_msg, sizeof(status_msg), "Loaded text into editor");

    char title[64];
    snprintf(title, sizeof(title), "Notes - %s", current_filename);
    if (notes_win && notes_win->is_open) {
        strncpy(notes_win->title, title, sizeof(notes_win->title) - 1);
        wm_restore_window(notes_win);
        wm_focus_window(notes_win);
    } else {
        notes_win = wm_create_window(title, 220, 130, 540, 380, RGB(22, 25, 38));
        if (!notes_win) return;
        notes_win->draw_client = notes_draw;
        notes_win->on_key = notes_key;
        notes_win->on_click = notes_click;
    }
}
