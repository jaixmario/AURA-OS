#include "apps.h"
#include "../fs/vfs.h"
#include "../gfx/gfx.h"
#include "../arch/pit.h"
#include "../libc/string.h"

static int selected_index = 0;
static int active_folder = 0; // 0=All, 1=Documents, 2=System
static int new_doc_counter = 1;
static char status_msg[64] = "Ready";
static unsigned int last_click_time = 0;
static int last_clicked_index = -1;

// "Store New File on Disk" Modal State
static int new_file_modal_open = 0;
static int new_file_folder = 0; // 0: Documents, 1: Storage, 2: System
static char new_file_name[VFS_MAX_FILENAME] = "DOC1.TXT";
static int new_file_name_len = 8;

static const char *folder_names[3] = {
    "Documents",
    "Storage",
    "System"
};

static int get_filtered_files(vfs_file_t *out_files[], int max_out) {
    int total = vfs_get_count();
    int count = 0;
    for (int i = 0; i < total && count < max_out; i++) {
        vfs_file_t *f = vfs_get_at(i);
        if (!f) continue;
        if (active_folder == 0) {
            out_files[count++] = f;
        } else if (active_folder == 1) {
            if (strcmp(f->folder, "Documents") == 0) {
                out_files[count++] = f;
            }
        } else if (active_folder == 2) {
            if (strcmp(f->folder, "System") == 0) {
                out_files[count++] = f;
            }
        }
    }
    return count;
}

static int count_folder_files(int folder_idx) {
    int total = vfs_get_count();
    if (folder_idx == 0) return total;
    int c = 0;
    const char *target = (folder_idx == 1) ? "Documents" : "System";
    for (int i = 0; i < total; i++) {
        vfs_file_t *f = vfs_get_at(i);
        if (f && strcmp(f->folder, target) == 0) c++;
    }
    return c;
}

static void files_open_new_modal(void) {
    new_file_modal_open = 1;
    new_file_folder = (active_folder == 2) ? 2 : (active_folder == 1 ? 0 : 0);

    for (int i = 1; i <= 99; i++) {
        snprintf(new_file_name, sizeof(new_file_name), "DOC%d.TXT", new_doc_counter++);
        if (!vfs_find(new_file_name)) break;
    }
    new_file_name_len = strlen(new_file_name);
}

static void files_confirm_new_file(void) {
    if (new_file_name_len == 0) {
        strcpy(new_file_name, "NEWFILE.TXT");
        new_file_name_len = strlen(new_file_name);
    }

    const char *folder = folder_names[new_file_folder];
    const char *sample = "✦ New Document created in AuraOS File Explorer.\nStored persistently on your ATA Hard Disk!\n";

    int res = vfs_create_file(new_file_name, folder, sample, strlen(sample), FS_ATTR_USER);
    if (res == 0) {
        new_file_modal_open = 0;
        snprintf(status_msg, sizeof(status_msg), "Created C:\\%s\\%s on Disk!", folder, new_file_name);

        vfs_file_t *filtered[VFS_MAX_FILES];
        int count = get_filtered_files(filtered, VFS_MAX_FILES);
        selected_index = count - 1;
    } else if (res == -2) {
        snprintf(status_msg, sizeof(status_msg), "Error: File already exists!");
    } else {
        snprintf(status_msg, sizeof(status_msg), "Error: Disk storage full.");
    }
}

static void files_draw(window_t *win) {
    int wx = win->x;
    int wy = win->y + TITLEBAR_HEIGHT;
    int client_w = win->width;
    int client_h = win->height - TITLEBAR_HEIGHT;

    // 1. Top Action & Navigation Toolbar (height 36px)
    int tb_h = 36;
    gfx_fillrect(wx, wy, client_w, tb_h, RGB(22, 25, 38));
    gfx_draw_line(wx, wy + tb_h, wx + client_w - 1, wy + tb_h, COLOR_BORDER);

    // Location Breadcrumb Pill
    int crumb_w = 210;
    gfx_fillrect(wx + 10, wy + 5, crumb_w, 26, RGB(30, 34, 52));
    gfx_drawrect(wx + 10, wy + 5, crumb_w, 26, COLOR_BORDER);

    const char *crumb_text = "C:\\AuraOS\\Storage";
    if (active_folder == 1) crumb_text = "C:\\AuraOS\\Documents";
    else if (active_folder == 2) crumb_text = "C:\\AuraOS\\System";
    gfx_draw_string(wx + 18, wy + 10, crumb_text, COLOR_ACCENT, COLOR_TRANSPARENT);

    // Action Buttons in Toolbar
    // [ Open in Editor ]
    int btn_open_x = wx + client_w - 360;
    gfx_fillrect(btn_open_x, wy + 5, 116, 26, RGB(46, 76, 128));
    gfx_drawrect(btn_open_x, wy + 5, 116, 26, COLOR_ACCENT);
    gfx_draw_string(btn_open_x + 8, wy + 10, "Open in Editor", COLOR_WHITE, COLOR_TRANSPARENT);

    // [ + New ]
    int btn_new_x = wx + client_w - 236;
    unsigned int new_bg = new_file_modal_open ? COLOR_ACCENT : RGB(34, 38, 56);
    gfx_fillrect(btn_new_x, wy + 5, 76, 26, new_bg);
    gfx_drawrect(btn_new_x, wy + 5, 76, 26, COLOR_ACCENT);
    gfx_draw_string(btn_new_x + 12, wy + 10, "+ New", COLOR_WHITE, COLOR_TRANSPARENT);

    // [ Delete ]
    int btn_del_x = wx + client_w - 152;
    gfx_fillrect(btn_del_x, wy + 5, 70, 26, RGB(52, 28, 38));
    gfx_drawrect(btn_del_x, wy + 5, 70, 26, RGB(180, 60, 70));
    gfx_draw_string(btn_del_x + 12, wy + 10, "Delete", RGB(240, 140, 150), COLOR_TRANSPARENT);

    // [ Refresh ]
    int btn_ref_x = wx + client_w - 74;
    gfx_fillrect(btn_ref_x, wy + 5, 64, 26, RGB(34, 38, 56));
    gfx_drawrect(btn_ref_x, wy + 5, 64, 26, RGB(60, 68, 96));
    gfx_draw_string(btn_ref_x + 10, wy + 10, "Refresh", COLOR_TEXT, COLOR_TRANSPARENT);

    // 2. Sidebar (width 160px)
    int sb_w = 160;
    int body_y = wy + tb_h + 1;
    int body_h = client_h - tb_h - 25;
    gfx_fillrect(wx, body_y, sb_w, body_h, RGB(18, 20, 30));
    gfx_draw_line(wx + sb_w, body_y, wx + sb_w, body_y + body_h - 1, COLOR_BORDER);

    // Category: QUICK ACCESS
    gfx_draw_string(wx + 14, body_y + 10, "STORAGE FOLDERS", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    const char *folders[3] = {
        "Storage (All)",
        "Documents",
        "System Files"
    };

    for (int i = 0; i < 3; i++) {
        int ty = body_y + 32 + (i * 32);
        int is_act = (active_folder == i);

        if (is_act) {
            gfx_fillrect(wx + 6, ty, sb_w - 12, 26, RGB(36, 42, 64));
            gfx_drawrect(wx + 6, ty, sb_w - 12, 26, COLOR_ACCENT);
            gfx_fillrect(wx + 6, ty + 2, 3, 22, COLOR_ACCENT);
        }

        gfx_draw_string(wx + 14, ty + 5, folders[i], is_act ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);

        char cnt_badge[8];
        snprintf(cnt_badge, sizeof(cnt_badge), "%d", count_folder_files(i));
        gfx_draw_string(wx + sb_w - 28, ty + 5, cnt_badge, is_act ? COLOR_ACCENT : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    }

    // Divider in sidebar
    int div_y = body_y + 138;
    gfx_draw_line(wx + 10, div_y, wx + sb_w - 10, div_y, COLOR_BORDER);

    // Category: HARD DISK (C:)
    gfx_draw_string(wx + 14, div_y + 10, "HARD DISK (C:)", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    int card_y = div_y + 30;
    gfx_fillrect(wx + 8, card_y, sb_w - 16, 76, RGB(14, 16, 24));
    gfx_drawrect(wx + 8, card_y, sb_w - 16, 76, COLOR_BORDER);

    const char *drive_mode = vfs_is_disk_backed() ? "ATA Master (LBA)" : "RAMFS Cache";
    gfx_draw_string(wx + 14, card_y + 6, drive_mode, COLOR_WHITE, COLOR_TRANSPARENT);

    // Progress Bar for storage usage
    int bar_x = wx + 14;
    int bar_y = card_y + 26;
    int bar_w = sb_w - 28;
    int bar_h = 8;
    gfx_fillrect(bar_x, bar_y, bar_w, bar_h, RGB(26, 30, 44));
    gfx_drawrect(bar_x, bar_y, bar_w, bar_h, RGB(50, 56, 80));

    unsigned int used_bytes = vfs_get_total_used();
    unsigned int total_bytes = vfs_get_total_capacity();
    int filled_w = 0;
    if (total_bytes > 0) {
        filled_w = (int)((used_bytes * (unsigned long)bar_w) / total_bytes);
    }
    if (filled_w < 4 && used_bytes > 0) filled_w = 4;
    if (filled_w > bar_w) filled_w = bar_w;
    gfx_fillrect(bar_x + 1, bar_y + 1, filled_w, bar_h - 2, COLOR_ACCENT);

    char used_str[32];
    snprintf(used_str, sizeof(used_str), "%u / %u B", used_bytes, total_bytes);
    gfx_draw_string(wx + 14, card_y + 40, used_str, COLOR_ACCENT, COLOR_TRANSPARENT);

    char file_stat[32];
    snprintf(file_stat, sizeof(file_stat), "Files: %d / %d", vfs_get_count(), VFS_MAX_FILES);
    gfx_draw_string(wx + 14, card_y + 56, file_stat, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // 3. Main File List Content Area
    int cx = wx + sb_w + 12;
    int cy = body_y + 6;
    int cw = client_w - sb_w - 24;

    // Table Header
    int head_h = 24;
    gfx_fillrect(cx, cy, cw, head_h, RGB(22, 25, 38));
    gfx_draw_line(cx, cy + head_h, cx + cw, cy + head_h, COLOR_BORDER);
    gfx_draw_string(cx + 8,   cy + 4, "Filename", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_draw_string(cx + 164, cy + 4, "Folder",   COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_draw_string(cx + 250, cy + 4, "Size",     COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_draw_string(cx + 326, cy + 4, "Date",     COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // Filtered Files List
    vfs_file_t *filtered[VFS_MAX_FILES];
    int filtered_count = get_filtered_files(filtered, VFS_MAX_FILES);

    if (selected_index >= filtered_count) selected_index = filtered_count - 1;
    if (selected_index < 0) selected_index = 0;

    int max_rows = 6;
    for (int i = 0; i < filtered_count && i < max_rows; i++) {
        vfs_file_t *f = filtered[i];
        if (!f) continue;

        int ry = cy + head_h + 4 + (i * 24);
        int is_sel = (selected_index == i);

        unsigned int row_bg = is_sel ? RGB(40, 48, 76) : (i % 2 == 0 ? RGB(26, 29, 44) : RGB(22, 25, 38));
        gfx_fillrect(cx, ry, cw, 22, row_bg);
        if (is_sel) {
            gfx_drawrect(cx, ry, cw, 22, COLOR_ACCENT);
            gfx_fillrect(cx, ry + 2, 3, 18, COLOR_ACCENT);
        }

        // File badge
        const char *badge = "[TXT]";
        unsigned int badge_col = RGB(137, 220, 235);
        if (strstr(f->name, ".CFG")) { badge = "[CFG]"; badge_col = RGB(249, 226, 175); }
        else if (strstr(f->name, ".SYS")) { badge = "[SYS]"; badge_col = RGB(203, 166, 247); }
        else if (strstr(f->name, ".SH"))  { badge = "[SH ]"; badge_col = RGB(166, 227, 161); }
        else if (strstr(f->name, ".LOG")) { badge = "[LOG]"; badge_col = RGB(148, 226, 213); }

        gfx_draw_string(cx + 8, ry + 3, badge, badge_col, COLOR_TRANSPARENT);
        gfx_draw_string(cx + 56, ry + 3, f->name, is_sel ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);

        gfx_draw_string(cx + 164, ry + 3, f->folder, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

        char sz_str[16];
        snprintf(sz_str, sizeof(sz_str), "%u B", f->size);
        gfx_draw_string(cx + 250, ry + 3, sz_str, is_sel ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);

        char date_str[16];
        snprintf(date_str, sizeof(date_str), "%04u-%02u-%02u", f->created_year, f->created_month, f->created_day);
        gfx_draw_string(cx + 326, ry + 3, date_str, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    }

    // 4. File Inspector & Live Preview Pane (height 94px)
    int prev_y = cy + head_h + 4 + (max_rows * 24) + 6;
    int prev_h = 92;

    gfx_fillrect(cx, prev_y, cw, prev_h, RGB(16, 18, 26));
    gfx_drawrect(cx, prev_y, cw, prev_h, COLOR_BORDER);

    vfs_file_t *sel_file = (filtered_count > 0 && selected_index < filtered_count) ? filtered[selected_index] : 0;
    if (sel_file) {
        // Left Column: File Details
        int det_w = 180;
        char title_loc[48];
        snprintf(title_loc, sizeof(title_loc), "%s (C:\\%s)", sel_file->name, sel_file->folder);
        gfx_draw_string(cx + 10, prev_y + 8, title_loc, COLOR_WHITE, COLOR_TRANSPARENT);

        char meta1[48];
        snprintf(meta1, sizeof(meta1), "Size: %u B | LBA: %u", sel_file->size, sel_file->disk_lba);
        gfx_draw_string(cx + 10, prev_y + 26, meta1, COLOR_ACCENT, COLOR_TRANSPARENT);

        const char *perm = (sel_file->attr & FS_ATTR_SYSTEM) ? "System (Protected)" : ((sel_file->attr & FS_ATTR_READONLY) ? "Read-Only Disk File" : "Read/Write Disk File");
        gfx_draw_string(cx + 10, prev_y + 44, perm, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

        // Edit in Notes quick button
        int edit_btn_y = prev_y + 62;
        gfx_fillrect(cx + 10, edit_btn_y, 140, 22, RGB(42, 46, 68));
        gfx_drawrect(cx + 10, edit_btn_y, 140, 22, COLOR_ACCENT);
        gfx_draw_string(cx + 24, edit_btn_y + 3, "Edit in Notes", COLOR_WHITE, COLOR_TRANSPARENT);

        // Right Column: Monospace Code / Text Preview Frame
        int pf_x = cx + det_w + 10;
        int pf_w = cw - det_w - 20;
        int pf_h = prev_h - 16;
        gfx_fillrect(pf_x, prev_y + 8, pf_w, pf_h, RGB(10, 12, 18));
        gfx_drawrect(pf_x, prev_y + 8, pf_w, pf_h, RGB(38, 42, 60));

        // Render preview lines
        int cur_p_y = prev_y + 12;
        char line_buf[60];
        int l_idx = 0;
        int lines_drawn = 0;

        for (int k = 0; k < (int)sel_file->size && lines_drawn < 3; k++) {
            char c = sel_file->data[k];
            if (c == '\n' || c == '\r') {
                line_buf[l_idx] = '\0';
                gfx_draw_string(pf_x + 8, cur_p_y, line_buf, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
                cur_p_y += 18;
                lines_drawn++;
                l_idx = 0;
            } else if (l_idx < 50) {
                line_buf[l_idx++] = c;
            }
        }
        if (l_idx > 0 && lines_drawn < 3) {
            line_buf[l_idx] = '\0';
            gfx_draw_string(pf_x + 8, cur_p_y, line_buf, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
        }
    } else {
        gfx_draw_string(cx + 14, prev_y + 36, "No files found in this category.", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    }

    // 5. Bottom Status Strip
    int status_y = wy + client_h - 24;
    gfx_fillrect(wx, status_y, client_w, 24, RGB(16, 18, 28));
    gfx_draw_line(wx, status_y, wx + client_w - 1, status_y, COLOR_BORDER);

    char stat_count[32];
    snprintf(stat_count, sizeof(stat_count), "Items: %d", filtered_count);
    gfx_draw_string(wx + 14, status_y + 5, stat_count, COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    if (sel_file) {
        char stat_sel[48];
        snprintf(stat_sel, sizeof(stat_sel), "C:\\%s\\%s (%u B)", sel_file->folder, sel_file->name, sel_file->size);
        gfx_draw_string(wx + 110, status_y + 5, stat_sel, COLOR_WHITE, COLOR_TRANSPARENT);
    }

    gfx_draw_string(wx + client_w - 240, status_y + 5, status_msg, COLOR_ACCENT, COLOR_TRANSPARENT);

    // 6. Interactive "Store New File on Disk" Modal Overlay
    if (new_file_modal_open) {
        int px = 40;
        int py = tb_h + 12;
        int pw = client_w - 80;
        int ph = 260;

        gfx_fillrect(wx + px, wy + py, pw, ph, RGB(22, 25, 38));
        gfx_drawrect(wx + px, wy + py, pw, ph, COLOR_ACCENT);

        // Header
        gfx_fillrect(wx + px, wy + py, pw, 28, RGB(32, 38, 58));
        gfx_draw_line(wx + px, wy + py + 28, wx + px + pw, wy + py + 28, COLOR_BORDER);
        gfx_draw_string(wx + px + 12, wy + py + 6, "Store New File on Disk (Select Location)", COLOR_WHITE, COLOR_TRANSPARENT);

        gfx_draw_string(wx + px + pw - 20, wy + py + 6, "X", RGB(240, 120, 130), COLOR_TRANSPARENT);

        // Section 1: Folder Selection
        gfx_draw_string(wx + px + 16, wy + py + 38, "Where would you like to store this file on disk?", COLOR_ACCENT, COLOR_TRANSPARENT);

        for (int i = 0; i < 3; i++) {
            int bx = wx + px + 16 + (i * 140);
            int by = wy + py + 58;
            int bw = 130;
            int bh = 28;
            int is_act = (new_file_folder == i);

            unsigned int btn_bg = is_act ? RGB(46, 76, 128) : RGB(28, 32, 48);
            unsigned int btn_bd = is_act ? COLOR_ACCENT : COLOR_BORDER;
            gfx_fillrect(bx, by, bw, bh, btn_bg);
            gfx_drawrect(bx, by, bw, bh, btn_bd);

            char label[32];
            snprintf(label, sizeof(label), "%s %s", is_act ? "[X]" : "[ ]", folder_names[i]);
            gfx_draw_string(bx + 10, by + 6, label, is_act ? COLOR_WHITE : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
        }

        // Section 2: Filename input
        gfx_draw_string(wx + px + 16, wy + py + 98, "Enter Filename (type to change):", COLOR_WHITE, COLOR_TRANSPARENT);

        int ib_x = wx + px + 16;
        int ib_y = wy + py + 116;
        int ib_w = pw - 32;
        int ib_h = 28;
        gfx_fillrect(ib_x, ib_y, ib_w, ib_h, RGB(14, 16, 24));
        gfx_drawrect(ib_x, ib_y, ib_w, ib_h, COLOR_ACCENT);

        gfx_draw_string(ib_x + 10, ib_y + 6, new_file_name, COLOR_WHITE, COLOR_TRANSPARENT);

        // Input cursor
        if ((pit_get_ticks() / 30) % 2 == 0) {
            int cur_x = ib_x + 10 + (new_file_name_len * 8);
            if (cur_x < ib_x + ib_w - 10) {
                gfx_fillrect(cur_x, ib_y + 6, 8, 16, COLOR_ACCENT);
            }
        }

        // Section 3: Summary
        int inf_y = wy + py + 154;
        char path_sum[80];
        snprintf(path_sum, sizeof(path_sum), "Destination : C:\\%s\\%s", folder_names[new_file_folder], new_file_name);
        gfx_draw_string(wx + px + 16, inf_y, path_sum, RGB(166, 227, 161), COLOR_TRANSPARENT);

        gfx_draw_string(wx + px + 16, inf_y + 18, "Hardware    : ATA Primary Master (Sector 516+, Live Sync)", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

        // Section 4: Buttons
        int save_btn_x = wx + px + pw - 230;
        int save_btn_y = wy + py + ph - 38;
        gfx_fillrect(save_btn_x, save_btn_y, 140, 28, RGB(46, 76, 128));
        gfx_drawrect(save_btn_x, save_btn_y, 140, 28, COLOR_ACCENT);
        gfx_draw_string(save_btn_x + 14, save_btn_y + 6, "Create on Disk", COLOR_WHITE, COLOR_TRANSPARENT);

        int cancel_btn_x = wx + px + pw - 80;
        gfx_fillrect(cancel_btn_x, save_btn_y, 70, 28, RGB(34, 38, 56));
        gfx_drawrect(cancel_btn_x, save_btn_y, 70, 28, COLOR_BORDER);
        gfx_draw_string(cancel_btn_x + 12, save_btn_y + 6, "Cancel", COLOR_TEXT, COLOR_TRANSPARENT);
    }
}

static void files_click(window_t *win, int rx, int ry, int btn) {
    (void)btn;
    int client_w = win->width;
    int client_h = win->height - TITLEBAR_HEIGHT;
    int tb_h = 36;
    int sb_w = 160;

    // Handle "Store New File on Disk" Modal clicks
    if (new_file_modal_open) {
        int px = 40;
        int py = tb_h + 12;
        int pw = client_w - 80;
        int ph = 260;

        // Close 'X' button
        if (rx >= px + pw - 24 && rx <= px + pw && ry >= py && ry <= py + 28) {
            new_file_modal_open = 0;
            return;
        }

        // Folder selection buttons
        for (int i = 0; i < 3; i++) {
            int bx = px + 16 + (i * 140);
            int by = py + 58;
            if (rx >= bx && rx <= bx + 130 && ry >= by && ry <= by + 28) {
                new_file_folder = i;
                return;
            }
        }

        // Create on Disk button
        int save_btn_x = px + pw - 230;
        int save_btn_y = py + ph - 38;
        if (rx >= save_btn_x && rx <= save_btn_x + 140 && ry >= save_btn_y && ry <= save_btn_y + 28) {
            files_confirm_new_file();
            return;
        }

        // Cancel button
        int cancel_btn_x = px + pw - 80;
        if (rx >= cancel_btn_x && rx <= cancel_btn_x + 70 && ry >= save_btn_y && ry <= save_btn_y + 28) {
            new_file_modal_open = 0;
            return;
        }
        return;
    }

    // 1. Top Action Buttons
    if (ry >= 5 && ry <= 31) {
        // [ Open in Editor ]
        int btn_open_x = client_w - 360;
        if (rx >= btn_open_x && rx <= btn_open_x + 116) {
            vfs_file_t *filtered[VFS_MAX_FILES];
            int count = get_filtered_files(filtered, VFS_MAX_FILES);
            if (count > 0 && selected_index < count) {
                app_notes_open_file(filtered[selected_index]->name);
                snprintf(status_msg, sizeof(status_msg), "Opened %s in Notes", filtered[selected_index]->name);
            }
            return;
        }

        // [ + New ] -> Opens "Store New File on Disk" dialog!
        int btn_new_x = client_w - 236;
        if (rx >= btn_new_x && rx <= btn_new_x + 76) {
            files_open_new_modal();
            return;
        }

        // [ Delete ]
        int btn_del_x = client_w - 152;
        if (rx >= btn_del_x && rx <= btn_del_x + 70) {
            vfs_file_t *filtered[VFS_MAX_FILES];
            int count = get_filtered_files(filtered, VFS_MAX_FILES);
            if (count > 0 && selected_index < count) {
                vfs_file_t *f = filtered[selected_index];
                int res = vfs_delete_file(f->name);
                if (res == 0) {
                    snprintf(status_msg, sizeof(status_msg), "Deleted %s from Disk", f->name);
                    if (selected_index >= count - 1) selected_index = count - 2;
                    if (selected_index < 0) selected_index = 0;
                } else if (res == -2) {
                    snprintf(status_msg, sizeof(status_msg), "Cannot delete system file!");
                }
            }
            return;
        }

        // [ Refresh ]
        int btn_ref_x = client_w - 74;
        if (rx >= btn_ref_x && rx <= btn_ref_x + 64) {
            snprintf(status_msg, sizeof(status_msg), "View refreshed");
            return;
        }
    }

    // 2. Sidebar Navigation Items
    int body_y = tb_h + 1;
    if (rx >= 6 && rx <= sb_w - 6) {
        for (int i = 0; i < 3; i++) {
            int ty = body_y + 32 + (i * 32);
            if (ry >= ty && ry <= ty + 26) {
                active_folder = i;
                selected_index = 0;
                snprintf(status_msg, sizeof(status_msg), "Switched folder");
                return;
            }
        }
    }

    // 3. File Row Selection & Double-Click Detection
    int cx = sb_w + 12;
    int cw = client_w - sb_w - 24;
    int head_h = 24;
    int cy = body_y + 6;

    vfs_file_t *filtered[VFS_MAX_FILES];
    int filtered_count = get_filtered_files(filtered, VFS_MAX_FILES);
    int max_rows = 6;

    for (int i = 0; i < filtered_count && i < max_rows; i++) {
        int ry_row = cy + head_h + 4 + (i * 24);
        if (rx >= cx && rx <= cx + cw && ry >= ry_row && ry <= ry_row + 22) {
            unsigned int now = pit_get_ticks();
            if (selected_index == i && last_clicked_index == i && (now - last_click_time) < 40) {
                // Double click -> open file in Notes!
                app_notes_open_file(filtered[i]->name);
                snprintf(status_msg, sizeof(status_msg), "Opened %s in Notes", filtered[i]->name);
                last_click_time = 0;
                last_clicked_index = -1;
                return;
            }

            selected_index = i;
            last_clicked_index = i;
            last_click_time = now;
            return;
        }
    }

    // 4. File Inspector "Edit in Notes" Button
    int prev_y = cy + head_h + 4 + (max_rows * 24) + 6;
    int edit_btn_y = prev_y + 62;
    if (rx >= cx + 10 && rx <= cx + 150 && ry >= edit_btn_y && ry <= edit_btn_y + 22) {
        if (filtered_count > 0 && selected_index < filtered_count) {
            app_notes_open_file(filtered[selected_index]->name);
            snprintf(status_msg, sizeof(status_msg), "Opened %s in Notes", filtered[selected_index]->name);
        }
    }
}

static void files_key(window_t *win, char key) {
    (void)win;

    // Keyboard handling inside "Store New File on Disk" modal
    if (new_file_modal_open) {
        if (key == 27) { // Escape -> cancel
            new_file_modal_open = 0;
            return;
        }
        if (key == '\n') { // Enter -> confirm
            files_confirm_new_file();
            return;
        }
        if (key == '\t') { // Tab -> cycle folder
            new_file_folder = (new_file_folder + 1) % 3;
            return;
        }
        if (key == '\b') { // Backspace
            if (new_file_name_len > 0) {
                new_file_name[--new_file_name_len] = '\0';
            }
            return;
        }
        if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9') || key == '.' || key == '_' || key == '-') {
            if (new_file_name_len < VFS_MAX_FILENAME - 2) {
                if (key >= 'a' && key <= 'z') key -= 32; // Uppercase
                new_file_name[new_file_name_len++] = key;
                new_file_name[new_file_name_len] = '\0';
            }
            return;
        }
        return;
    }

    vfs_file_t *filtered[VFS_MAX_FILES];
    int count = get_filtered_files(filtered, VFS_MAX_FILES);
    if (count == 0) return;

    if (key == 'w' || key == 'W') { // Up
        if (selected_index > 0) selected_index--;
    } else if (key == 's' || key == 'S') { // Down
        if (selected_index < count - 1) selected_index++;
    } else if (key == 'n' || key == 'N') { // New file modal
        files_open_new_modal();
    } else if (key == '\n') { // Enter -> Open file
        if (selected_index < count) {
            app_notes_open_file(filtered[selected_index]->name);
            snprintf(status_msg, sizeof(status_msg), "Opened %s in Notes", filtered[selected_index]->name);
        }
    } else if (key == 'd' || key == 'D') { // Delete
        if (selected_index < count) {
            vfs_file_t *f = filtered[selected_index];
            int res = vfs_delete_file(f->name);
            if (res == 0) {
                snprintf(status_msg, sizeof(status_msg), "File deleted from disk");
                if (selected_index >= count - 1) selected_index = count - 2;
                if (selected_index < 0) selected_index = 0;
            } else if (res == -2) {
                snprintf(status_msg, sizeof(status_msg), "Cannot delete system file!");
            }
        }
    }
}

void app_files_launch(void) {
    window_t *win = wm_create_window("File Explorer", 160, 90, 620, 410, RGB(22, 25, 38));
    if (!win) return;
    win->draw_client = files_draw;
    win->on_click = files_click;
    win->on_key = files_key;
}
