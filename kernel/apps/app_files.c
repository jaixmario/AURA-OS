#include "apps.h"
#include "../fs/vfs.h"
#include "../gfx/gfx.h"
#include "../libc/string.h"

static int selected_index = 0;
static int active_folder = 0; // 0=Storage, 1=Documents, 2=System
static int new_doc_counter = 1;

static void files_draw(window_t *win) {
    int wx = win->x;
    int wy = win->y + TITLEBAR_HEIGHT;
    int client_w = win->width;
    int client_h = win->height - TITLEBAR_HEIGHT;

    // 1. Sidebar Background
    int sb_w = 146;
    gfx_fillrect(wx, wy, sb_w, client_h, RGB(22, 24, 36));
    gfx_draw_line(wx + sb_w, wy, wx + sb_w, wy + client_h - 1, COLOR_BORDER);

    // Sidebar Title
    gfx_draw_string(wx + 16, wy + 14, "Explorer", COLOR_WHITE, COLOR_TRANSPARENT);

    // Sidebar Folders
    const char *folders[3] = {
        "• Storage (C:)",
        "• Documents",
        "• System Files"
    };

    for (int i = 0; i < 3; i++) {
        int ty = wy + 42 + (i * 34);
        int is_act = (active_folder == i);

        if (is_act) {
            gfx_fillrect(wx + 6, ty, sb_w - 12, 28, RGB(38, 42, 64));
            gfx_drawrect(wx + 6, ty, sb_w - 12, 28, COLOR_ACCENT);
            gfx_fillrect(wx + 6, ty + 2, 3, 24, COLOR_ACCENT);
        }

        gfx_draw_string(wx + 12, ty + 6, folders[i], is_act ? COLOR_WHITE : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    }

    // Storage capacity card in sidebar
    int cap_y = wy + 160;
    gfx_fillrect(wx + 8, cap_y, sb_w - 16, 54, RGB(16, 18, 28));
    gfx_drawrect(wx + 8, cap_y, sb_w - 16, 54, COLOR_BORDER);

    char stat_str[32];
    snprintf(stat_str, sizeof(stat_str), "Files: %d", vfs_get_count());
    gfx_draw_string(wx + 16, cap_y + 8, stat_str, COLOR_ACCENT, COLOR_TRANSPARENT);

    char used_str[32];
    snprintf(used_str, sizeof(used_str), "Used: %u B", vfs_get_total_used());
    gfx_draw_string(wx + 16, cap_y + 24, used_str, COLOR_WHITE, COLOR_TRANSPARENT);

    char cap_str[32];
    snprintf(cap_str, sizeof(cap_str), "Disk: FAT16 9MB", vfs_get_total_capacity());
    gfx_draw_string(wx + 16, cap_y + 38, cap_str, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // Action buttons in sidebar
    int new_y = wy + 230;
    gfx_fillrect(wx + 8, new_y, sb_w - 16, 28, RGB(36, 40, 60));
    gfx_drawrect(wx + 8, new_y, sb_w - 16, 28, COLOR_ACCENT);
    gfx_draw_string(wx + 16, new_y + 6, "+ New File", COLOR_WHITE, COLOR_TRANSPARENT);

    int del_y = new_y + 34;
    gfx_fillrect(wx + 8, del_y, sb_w - 16, 28, RGB(46, 28, 36));
    gfx_drawrect(wx + 8, del_y, sb_w - 16, 28, RGB(180, 60, 70));
    gfx_draw_string(wx + 34, del_y + 6, "Delete", RGB(240, 140, 150), COLOR_TRANSPARENT);

    // 2. Main File List Content Area
    int cx = wx + sb_w + 12;
    int cy = wy + 12;
    int cw = client_w - sb_w - 24;

    // Breadcrumb bar
    gfx_fillrect(cx, cy, cw, 26, RGB(30, 32, 46));
    gfx_drawrect(cx, cy, cw, 26, COLOR_BORDER);
    if (active_folder == 0) {
        gfx_draw_string(cx + 8, cy + 5, "C:\\AuraOS\\Storage", COLOR_ACCENT, COLOR_TRANSPARENT);
    } else if (active_folder == 1) {
        gfx_draw_string(cx + 8, cy + 5, "C:\\AuraOS\\Documents", COLOR_ACCENT, COLOR_TRANSPARENT);
    } else {
        gfx_draw_string(cx + 8, cy + 5, "C:\\AuraOS\\System", COLOR_ACCENT, COLOR_TRANSPARENT);
    }

    // Table Header
    int head_y = cy + 32;
    gfx_fillrect(cx, head_y, cw, 20, RGB(22, 24, 34));
    gfx_draw_line(cx, head_y + 20, cx + cw, head_y + 20, COLOR_BORDER);
    gfx_draw_string(cx + 8,   head_y + 2, "Filename", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_draw_string(cx + 170, head_y + 2, "Type",     COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_draw_string(cx + 250, head_y + 2, "Size",     COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_draw_string(cx + 330, head_y + 2, "Date",     COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // File Rows
    int total_files = vfs_get_count();
    if (selected_index >= total_files) selected_index = total_files - 1;
    if (selected_index < 0) selected_index = 0;

    int max_rows = 6;
    for (int i = 0; i < total_files && i < max_rows; i++) {
        vfs_file_t *f = vfs_get_at(i);
        if (!f) continue;

        int ry = head_y + 24 + (i * 24);
        int is_sel = (selected_index == i);

        unsigned int row_bg = is_sel ? RGB(44, 48, 72) : (i % 2 == 0 ? RGB(26, 28, 40) : RGB(22, 24, 36));
        gfx_fillrect(cx, ry, cw, 22, row_bg);
        if (is_sel) {
            gfx_drawrect(cx, ry, cw, 22, COLOR_ACCENT);
        }

        // File badge
        const char *badge = "[FILE]";
        unsigned int badge_col = COLOR_ACCENT;
        if (strstr(f->name, ".TXT")) { badge = "[TXT]"; badge_col = RGB(137, 220, 235); }
        else if (strstr(f->name, ".CFG")) { badge = "[CFG]"; badge_col = RGB(249, 226, 175); }
        else if (strstr(f->name, ".SYS")) { badge = "[SYS]"; badge_col = RGB(203, 166, 247); }
        else if (strstr(f->name, ".SH"))  { badge = "[SH ]"; badge_col = RGB(166, 227, 161); }
        else if (strstr(f->name, ".LOG")) { badge = "[LOG]"; badge_col = RGB(148, 226, 213); }

        gfx_draw_string(cx + 6, ry + 3, badge, badge_col, COLOR_TRANSPARENT);
        gfx_draw_string(cx + 56, ry + 3, f->name, is_sel ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);

        const char *type_name = (f->attr & FS_ATTR_SYSTEM) ? "System" : ((f->attr & FS_ATTR_READONLY) ? "Manual" : "Document");
        gfx_draw_string(cx + 170, ry + 3, type_name, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

        char sz_str[16];
        snprintf(sz_str, sizeof(sz_str), "%u B", f->size);
        gfx_draw_string(cx + 250, ry + 3, sz_str, COLOR_TEXT, COLOR_TRANSPARENT);

        char date_str[16];
        snprintf(date_str, sizeof(date_str), "%04u-%02u-%02u", f->created_year, f->created_month, f->created_day);
        gfx_draw_string(cx + 330, ry + 3, date_str, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    }

    // 3. File Preview Pane
    int prev_y = head_y + 24 + (max_rows * 24) + 8;
    int prev_h = client_h - (prev_y - wy) - 12;

    gfx_fillrect(cx, prev_y, cw, prev_h, RGB(18, 20, 30));
    gfx_drawrect(cx, prev_y, cw, prev_h, COLOR_BORDER);

    vfs_file_t *sel_file = vfs_get_at(selected_index);
    if (sel_file) {
        char prev_title[64];
        snprintf(prev_title, sizeof(prev_title), "Preview: %s (%u bytes)", sel_file->name, sel_file->size);
        gfx_draw_string(cx + 10, prev_y + 8, prev_title, COLOR_WHITE, COLOR_TRANSPARENT);

        // Preview snippet (first 2 lines)
        char snippet[80];
        int s_idx = 0;
        for (int k = 0; k < (int)sel_file->size && k < 75; k++) {
            char c = sel_file->data[k];
            if (c == '\n' || c == '\r') c = ' ';
            snippet[s_idx++] = c;
        }
        snippet[s_idx] = '\0';
        gfx_draw_string(cx + 10, prev_y + 28, snippet, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

        // Open in Notes Editor button
        int ob_x = cx + cw - 180;
        int ob_y = prev_y + prev_h - 32;
        gfx_fillrect(ob_x, ob_y, 170, 24, RGB(42, 46, 68));
        gfx_drawrect(ob_x, ob_y, 170, 24, COLOR_ACCENT);
        gfx_draw_string(ob_x + 30, ob_y + 4, "Open in Notes", COLOR_WHITE, COLOR_TRANSPARENT);
    } else {
        gfx_draw_string(cx + 10, prev_y + 20, "Select a file above to inspect details.", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    }
}

static void files_click(window_t *win, int rx, int ry, int btn) {
    (void)btn;
    int sb_w = 146;

    // Sidebar folders click
    if (rx >= 6 && rx <= sb_w - 6) {
        for (int i = 0; i < 3; i++) {
            int ty = 42 + (i * 34);
            if (ry >= ty && ry <= ty + 28) {
                active_folder = i;
                return;
            }
        }

        // New File button
        int new_y = 230;
        if (ry >= new_y && ry <= new_y + 28) {
            char new_name[24];
            snprintf(new_name, sizeof(new_name), "DOC%d.TXT", new_doc_counter++);
            const char *sample = "✦ New Document created in AuraOS File Explorer.\nReady for editing!\n";
            vfs_create_file(new_name, sample, strlen(sample), FS_ATTR_USER);
            selected_index = vfs_get_count() - 1;
            return;
        }

        // Delete button
        int del_y = new_y + 34;
        if (ry >= del_y && ry <= del_y + 28) {
            vfs_file_t *f = vfs_get_at(selected_index);
            if (f) {
                vfs_delete_file(f->name);
                if (selected_index >= vfs_get_count()) {
                    selected_index = vfs_get_count() - 1;
                }
            }
            return;
        }
    }

    // Main file row selection
    int cx = sb_w + 12;
    int cw = win->width - sb_w - 24;
    int head_y = 12 + 32;

    int total_files = vfs_get_count();
    int max_rows = 6;
    for (int i = 0; i < total_files && i < max_rows; i++) {
        int ry_row = head_y + 24 + (i * 24);
        if (rx >= cx && rx <= cx + cw && ry >= ry_row && ry <= ry_row + 22) {
            selected_index = i;
            return;
        }
    }

    // Open in Notes Editor button click
    int prev_y = head_y + 24 + (max_rows * 24) + 8;
    int client_h = win->height - TITLEBAR_HEIGHT;
    int prev_h = client_h - prev_y - 12;
    int ob_x = cx + cw - 180;
    int ob_y = prev_y + prev_h - 32;

    if (rx >= ob_x && rx <= ob_x + 170 && ry >= ob_y && ry <= ob_y + 24) {
        vfs_file_t *sel = vfs_get_at(selected_index);
        if (sel) {
            app_notes_load_text(sel->data);
        }
    }
}

void app_files_launch(void) {
    window_t *win = wm_create_window("File Explorer", 180, 100, 600, 390, RGB(24, 26, 38));
    if (!win) return;
    win->draw_client = files_draw;
    win->on_click = files_click;
}
