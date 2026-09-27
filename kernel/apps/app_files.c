#include "apps.h"
#include "../fs/vfs.h"
#include "../gfx/gfx.h"
#include "../arch/pit.h"
#include "../libc/string.h"

// Selection & Navigation State
static int selected_index = 0;
static int scroll_offset = 0;
static int active_folder = 0; // 0=All/Storage, 1=Documents, 2=System, 3=Custom
static int new_doc_counter = 1;
static char status_msg[64] = "Ready";
static unsigned int last_click_time = 0;
static int last_clicked_index = -1;

// Path & History Navigation State
#define HISTORY_MAX 16
static char history_stack[HISTORY_MAX][48];
static int history_count = 0;
static int history_index = 0;

static int path_bar_editing = 0;
static char current_path_str[48] = "C:\\Storage";
static int current_path_len = 10;

// Search Filter State
static int search_bar_editing = 0;
static char search_query[32] = "";
static int search_query_len = 0;

// "New File" Modal State
static int new_file_modal_open = 0;
static int new_file_focus = 0; // 0: Filename, 1: Folder
static char new_file_folder_str[VFS_MAX_FOLDER] = "Documents";
static int new_file_folder_len = 9;
static char new_file_name[VFS_MAX_FILENAME] = "DOC1.TXT";
static int new_file_name_len = 8;

// "Move File" Modal State
static int move_modal_open = 0;
static char move_target_folder[VFS_MAX_FOLDER] = "Documents";
static int move_target_folder_len = 9;

static const char *folder_presets[3] = {
    "Documents",
    "Storage",
    "System"
};

static const char *clean_path_prefix(const char *path) {
    if (!path) return "Storage";
    if ((path[0] == 'C' || path[0] == 'c') && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
        return path + 3;
    }
    if (path[0] == '\\' || path[0] == '/') {
        return path + 1;
    }
    return path;
}

// Case-insensitive substring search for filter
static int str_contains_nocase(const char *haystack, const char *needle) {
    if (!needle || needle[0] == '\0') return 1;
    if (!haystack) return 0;
    int nlen = strlen(needle);
    int hlen = strlen(haystack);
    if (nlen > hlen) return 0;

    for (int i = 0; i <= hlen - nlen; i++) {
        int match = 1;
        for (int j = 0; j < nlen; j++) {
            char c1 = haystack[i + j];
            char c2 = needle[j];
            if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
            if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
            if (c1 != c2) {
                match = 0;
                break;
            }
        }
        if (match) return 1;
    }
    return 0;
}

static int get_filtered_files(vfs_file_t *out_files[], int max_out) {
    int total = vfs_get_count();
    int count = 0;
    const char *target = clean_path_prefix(current_path_str);

    for (int i = 0; i < total && count < max_out; i++) {
        vfs_file_t *f = vfs_get_at(i);
        if (!f) continue;

        // Search query filter
        if (search_query_len > 0 && !str_contains_nocase(f->name, search_query)) {
            continue;
        }

        // Folder filter
        if (active_folder == 0 || strcmp(target, "Storage") == 0 || strcmp(target, "All") == 0 || target[0] == '\0') {
            out_files[count++] = f;
        } else {
            if (strcmp(f->folder, target) == 0) {
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

static void files_push_history(const char *path) {
    if (!path) return;
    if (history_count > 0 && strcmp(history_stack[history_index], path) == 0) return;

    if (history_count < HISTORY_MAX) {
        history_index = history_count++;
        strncpy(history_stack[history_index], path, sizeof(history_stack[history_index]) - 1);
        history_stack[history_index][sizeof(history_stack[history_index]) - 1] = '\0';
    } else {
        for (int i = 0; i < HISTORY_MAX - 1; i++) {
            strcpy(history_stack[i], history_stack[i + 1]);
        }
        history_index = HISTORY_MAX - 1;
        strncpy(history_stack[history_index], path, sizeof(history_stack[history_index]) - 1);
        history_stack[history_index][sizeof(history_stack[history_index]) - 1] = '\0';
    }
}

static void files_navigate_to_path_internal(const char *path) {
    if (!path || path[0] == '\0') return;

    strncpy(current_path_str, path, sizeof(current_path_str) - 1);
    current_path_str[sizeof(current_path_str) - 1] = '\0';
    current_path_len = strlen(current_path_str);
    path_bar_editing = 0;
    search_bar_editing = 0;

    const char *clean = clean_path_prefix(current_path_str);
    if (strcmp(clean, "Storage") == 0 || strcmp(clean, "All") == 0 || clean[0] == '\0') {
        active_folder = 0;
    } else if (strcmp(clean, "Documents") == 0) {
        active_folder = 1;
    } else if (strcmp(clean, "System") == 0) {
        active_folder = 2;
    } else {
        active_folder = 3;
    }

    selected_index = 0;
    scroll_offset = 0;
    snprintf(status_msg, sizeof(status_msg), "Navigated to %s", current_path_str);
}

static void files_navigate_to_path(const char *path) {
    files_navigate_to_path_internal(path);
    files_push_history(current_path_str);
}

static void files_go_back(void) {
    if (history_index > 0) {
        history_index--;
        files_navigate_to_path_internal(history_stack[history_index]);
    }
}

static void files_go_forward(void) {
    if (history_index < history_count - 1) {
        history_index++;
        files_navigate_to_path_internal(history_stack[history_index]);
    }
}

static void files_go_up(void) {
    files_navigate_to_path("C:\\Storage");
}

static void files_open_new_modal(void) {
    new_file_modal_open = 1;
    move_modal_open = 0;
    new_file_focus = 0;

    const char *cur = clean_path_prefix(current_path_str);
    if (cur[0] != '\0' && strcmp(cur, "Storage") != 0) {
        strncpy(new_file_folder_str, cur, sizeof(new_file_folder_str) - 1);
    } else {
        strcpy(new_file_folder_str, "Documents");
    }
    new_file_folder_len = strlen(new_file_folder_str);

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
    if (new_file_folder_len == 0) {
        strcpy(new_file_folder_str, "Documents");
        new_file_folder_len = strlen(new_file_folder_str);
    }

    const char *sample = "✦ Document created in AuraOS File Explorer.\nStored persistently on your ATA Hard Disk!\n";

    int res = vfs_create_file(new_file_name, new_file_folder_str, sample, strlen(sample), FS_ATTR_USER);
    if (res == 0) {
        new_file_modal_open = 0;
        char new_path[48];
        snprintf(new_path, sizeof(new_path), "C:\\%s", new_file_folder_str);
        files_navigate_to_path(new_path);
        snprintf(status_msg, sizeof(status_msg), "Created %s\\%s on Disk!", new_path, new_file_name);

        vfs_file_t *filtered[VFS_MAX_FILES];
        int count = get_filtered_files(filtered, VFS_MAX_FILES);
        selected_index = count - 1;
        if (selected_index >= scroll_offset + 12) {
            scroll_offset = selected_index - 11;
        }
    } else if (res == -2) {
        snprintf(status_msg, sizeof(status_msg), "Error: File already exists!");
    } else {
        snprintf(status_msg, sizeof(status_msg), "Error: Disk storage full.");
    }
}

static void files_confirm_move(void) {
    vfs_file_t *filtered[VFS_MAX_FILES];
    int count = get_filtered_files(filtered, VFS_MAX_FILES);
    if (count > 0 && selected_index < count) {
        vfs_file_t *f = filtered[selected_index];
        vfs_move_file(f->name, move_target_folder);
        move_modal_open = 0;
        snprintf(status_msg, sizeof(status_msg), "Moved %s to C:\\%s", f->name, move_target_folder);
    }
}

// Map file extensions to friendly Windows types
static const char *get_file_type_desc(const char *name) {
    if (strstr(name, ".TXT")) return "Text Document";
    if (strstr(name, ".CFG")) return "Config settings";
    if (strstr(name, ".SYS")) return "System file";
    if (strstr(name, ".SH"))  return "Shell Script";
    if (strstr(name, ".LOG")) return "Text Document";
    if (strstr(name, ".BIN")) return "Binary file";
    return "File";
}

static void files_draw(window_t *win) {
    int wx = win->x;
    int wy = win->y + TITLEBAR_HEIGHT;
    int client_w = win->width;
    int client_h = win->height - TITLEBAR_HEIGHT;

    // =========================================================================
    // 1. Windows File Explorer Command Ribbon (Row 1, Height 32px)
    // =========================================================================
    int rb_h = 32;
    gfx_fillrect(wx, wy, client_w, rb_h, RGB(32, 34, 44));
    gfx_draw_line(wx, wy + rb_h - 1, wx + client_w - 1, wy + rb_h - 1, COLOR_BORDER);

    // [ + New File ] Button
    int btn_new_x = wx + 10;
    unsigned int new_bg = new_file_modal_open ? COLOR_ACCENT : RGB(42, 46, 62);
    gfx_fillrect(btn_new_x, wy + 4, 96, 24, new_bg);
    gfx_drawrect(btn_new_x, wy + 4, 96, 24, COLOR_ACCENT);
    gfx_draw_string(btn_new_x + 8, wy + 8, "+ New File", new_file_modal_open ? RGB(17, 17, 27) : COLOR_WHITE, COLOR_TRANSPARENT);

    // [ Open in Notes ] Button
    int btn_open_x = btn_new_x + 104;
    gfx_fillrect(btn_open_x, wy + 4, 116, 24, RGB(46, 76, 128));
    gfx_drawrect(btn_open_x, wy + 4, 116, 24, COLOR_ACCENT);
    gfx_draw_string(btn_open_x + 8, wy + 8, "Open in Notes", COLOR_WHITE, COLOR_TRANSPARENT);

    // [ Delete ] Button
    int btn_del_x = btn_open_x + 124;
    gfx_fillrect(btn_del_x, wy + 4, 72, 24, RGB(56, 30, 40));
    gfx_drawrect(btn_del_x, wy + 4, 72, 24, RGB(200, 70, 80));
    gfx_draw_string(btn_del_x + 10, wy + 8, "Delete", RGB(245, 140, 150), COLOR_TRANSPARENT);

    // [ Move ] Button
    int btn_mov_x = btn_del_x + 80;
    unsigned int mov_bg = move_modal_open ? COLOR_ACCENT : RGB(36, 40, 56);
    gfx_fillrect(btn_mov_x, wy + 4, 60, 24, mov_bg);
    gfx_drawrect(btn_mov_x, wy + 4, 60, 24, COLOR_BORDER);
    gfx_draw_string(btn_mov_x + 12, wy + 8, "Move", move_modal_open ? RGB(17, 17, 27) : COLOR_TEXT, COLOR_TRANSPARENT);

    // [ Refresh ] Button
    int btn_ref_x = btn_mov_x + 68;
    gfx_fillrect(btn_ref_x, wy + 4, 74, 24, RGB(36, 40, 56));
    gfx_drawrect(btn_ref_x, wy + 4, 74, 24, COLOR_BORDER);
    gfx_draw_string(btn_ref_x + 8, wy + 8, "Refresh", COLOR_TEXT, COLOR_TRANSPARENT);

    // =========================================================================
    // 2. Windows Address & Search Bar (Row 2, Height 32px)
    // =========================================================================
    int ab_y = wy + rb_h;
    int ab_h = 32;
    gfx_fillrect(wx, ab_y, client_w, ab_h, RGB(26, 28, 38));
    gfx_draw_line(wx, ab_y + ab_h - 1, wx + client_w - 1, ab_y + ab_h - 1, COLOR_BORDER);

    // Navigation buttons: [ < ] [ > ] [ ^ ]
    int nav_back_x = wx + 10;
    int nav_fwd_x  = nav_back_x + 28;
    int nav_up_x   = nav_fwd_x + 28;

    // [ < ] Back
    int can_back = (history_index > 0);
    gfx_fillrect(nav_back_x, ab_y + 4, 24, 24, can_back ? RGB(36, 42, 60) : RGB(28, 30, 42));
    gfx_drawrect(nav_back_x, ab_y + 4, 24, 24, COLOR_BORDER);
    gfx_draw_string(nav_back_x + 8, ab_y + 8, "<", can_back ? COLOR_WHITE : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // [ > ] Forward
    int can_fwd = (history_index < history_count - 1);
    gfx_fillrect(nav_fwd_x, ab_y + 4, 24, 24, can_fwd ? RGB(36, 42, 60) : RGB(28, 30, 42));
    gfx_drawrect(nav_fwd_x, ab_y + 4, 24, 24, COLOR_BORDER);
    gfx_draw_string(nav_fwd_x + 8, ab_y + 8, ">", can_fwd ? COLOR_WHITE : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // [ ^ ] Up
    gfx_fillrect(nav_up_x, ab_y + 4, 24, 24, RGB(36, 42, 60));
    gfx_drawrect(nav_up_x, ab_y + 4, 24, 24, COLOR_BORDER);
    gfx_draw_string(nav_up_x + 8, ab_y + 8, "^", COLOR_WHITE, COLOR_TRANSPARENT);

    // Windows Explorer Address Bar Box
    int search_w = 140;
    int adr_x = nav_up_x + 34;
    int adr_w = client_w - (adr_x - wx) - search_w - 20;
    if (adr_w < 180) adr_w = 180;

    unsigned int adr_border = path_bar_editing ? COLOR_ACCENT : COLOR_BORDER;
    gfx_fillrect(adr_x, ab_y + 4, adr_w, 24, RGB(18, 20, 30));
    gfx_drawrect(adr_x, ab_y + 4, adr_w, 24, adr_border);

    // Drive icon in address bar
    gfx_draw_string(adr_x + 6, ab_y + 8, "[C:]", COLOR_ACCENT, COLOR_TRANSPARENT);

    if (path_bar_editing) {
        // Direct editable path string
        gfx_draw_string_clipped(adr_x + 40, ab_y + 8, current_path_str, COLOR_WHITE, COLOR_TRANSPARENT, adr_w - 48);
        if ((pit_get_ticks() / 30) % 2 == 0) {
            int cur_x = adr_x + 40 + (current_path_len * 8);
            if (cur_x < adr_x + adr_w - 10) {
                gfx_fillrect(cur_x, ab_y + 8, 8, 16, COLOR_ACCENT);
            }
        }
    } else {
        // Windows Breadcrumb display: This PC > Local Disk (C:) > Folder
        char breadcrumb[80];
        const char *clean = clean_path_prefix(current_path_str);
        if (strcmp(clean, "Storage") == 0 || strcmp(clean, "All") == 0 || clean[0] == '\0') {
            snprintf(breadcrumb, sizeof(breadcrumb), " This PC > Local Disk (C:)");
        } else {
            snprintf(breadcrumb, sizeof(breadcrumb), " This PC > Local Disk (C:) > %s", clean);
        }
        gfx_draw_string_clipped(adr_x + 40, ab_y + 8, breadcrumb, COLOR_WHITE, COLOR_TRANSPARENT, adr_w - 48);
    }

    // Windows Explorer Search Box
    int search_x = adr_x + adr_w + 10;
    unsigned int search_border = search_bar_editing ? COLOR_ACCENT : COLOR_BORDER;
    gfx_fillrect(search_x, ab_y + 4, search_w, 24, RGB(18, 20, 30));
    gfx_drawrect(search_x, ab_y + 4, search_w, 24, search_border);

    if (search_query_len > 0) {
        gfx_draw_string_clipped(search_x + 6, ab_y + 8, search_query, COLOR_WHITE, COLOR_TRANSPARENT, search_w - 12);
        if (search_bar_editing && (pit_get_ticks() / 30) % 2 == 0) {
            int cur_x = search_x + 6 + (search_query_len * 8);
            if (cur_x < search_x + search_w - 10) {
                gfx_fillrect(cur_x, ab_y + 8, 8, 16, COLOR_ACCENT);
            }
        }
    } else {
        gfx_draw_string(search_x + 6, ab_y + 8, search_bar_editing ? "|" : "Search...", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    }

    // =========================================================================
    // 3. Windows Left Navigation Pane (Width 175px)
    // =========================================================================
    int sb_w = 175;
    int body_y = ab_y + ab_h;
    int body_h = client_h - rb_h - ab_h - 24;
    gfx_fillrect(wx, body_y, sb_w, body_h, RGB(22, 24, 34));
    gfx_draw_line(wx + sb_w, body_y, wx + sb_w, body_y + body_h - 1, COLOR_BORDER);

    // Section 1: QUICK ACCESS
    int tree_y = body_y + 10;
    gfx_draw_string(wx + 12, tree_y, "v Quick access", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // Pinned 1: Desktop
    int py0 = tree_y + 22;
    int is_desk = (active_folder == 1);
    if (is_desk) {
        gfx_fillrect(wx + 6, py0, sb_w - 12, 22, RGB(36, 44, 66));
        gfx_drawrect(wx + 6, py0, sb_w - 12, 22, COLOR_ACCENT);
    }
    gfx_draw_string(wx + 20, py0 + 3, "* Desktop", is_desk ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);

    // Pinned 2: Documents
    int py1 = py0 + 24;
    int is_docs = (active_folder == 1);
    if (is_docs) {
        gfx_fillrect(wx + 6, py1, sb_w - 12, 22, RGB(36, 44, 66));
        gfx_drawrect(wx + 6, py1, sb_w - 12, 22, COLOR_ACCENT);
    }
    gfx_draw_string(wx + 20, py1 + 3, "📁 Documents", is_docs ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);
    char cnt_doc[8];
    snprintf(cnt_doc, sizeof(cnt_doc), "%d", count_folder_files(1));
    gfx_draw_string(wx + sb_w - 26, py1 + 3, cnt_doc, is_docs ? COLOR_ACCENT : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // Pinned 3: Storage (All)
    int py2 = py1 + 24;
    int is_all = (active_folder == 0);
    if (is_all) {
        gfx_fillrect(wx + 6, py2, sb_w - 12, 22, RGB(36, 44, 66));
        gfx_drawrect(wx + 6, py2, sb_w - 12, 22, COLOR_ACCENT);
    }
    gfx_draw_string(wx + 20, py2 + 3, "📁 All Files", is_all ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);
    char cnt_all[8];
    snprintf(cnt_all, sizeof(cnt_all), "%d", count_folder_files(0));
    gfx_draw_string(wx + sb_w - 26, py2 + 3, cnt_all, is_all ? COLOR_ACCENT : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // Divider
    int div1_y = py2 + 30;
    gfx_draw_line(wx + 10, div1_y, wx + sb_w - 10, div1_y, COLOR_BORDER);

    // Section 2: THIS PC
    int pc_y = div1_y + 8;
    gfx_draw_string(wx + 12, pc_y, "v This PC", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // Local Disk (C:) with real capacity bar
    int c_y = pc_y + 20;
    int is_c = (active_folder == 0);
    gfx_fillrect(wx + 6, c_y, sb_w - 12, 54, is_c ? RGB(32, 38, 58) : RGB(18, 20, 28));
    gfx_drawrect(wx + 6, c_y, sb_w - 12, 54, is_c ? COLOR_ACCENT : COLOR_BORDER);

    gfx_draw_string(wx + 14, c_y + 4, "[C:] Local Disk (C:)", is_c ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);

    // Storage progress bar
    int bar_x = wx + 14;
    int bar_y = c_y + 24;
    int bar_w = sb_w - 28;
    int bar_h = 7;
    gfx_fillrect(bar_x, bar_y, bar_w, bar_h, RGB(28, 30, 42));
    gfx_drawrect(bar_x, bar_y, bar_w, bar_h, RGB(48, 54, 76));

    unsigned int used_bytes = vfs_get_total_used();
    unsigned int total_capacity = vfs_get_total_capacity();
    int filled_w = (int)((used_bytes * (unsigned long)bar_w) / (total_capacity ? total_capacity : 1));
    if (filled_w < 4 && used_bytes > 0) filled_w = 4;
    if (filled_w > bar_w) filled_w = bar_w;
    gfx_fillrect(bar_x + 1, bar_y + 1, filled_w, bar_h - 2, COLOR_ACCENT);

    char cap_label[32];
    snprintf(cap_label, sizeof(cap_label), "%u B used", used_bytes);
    gfx_draw_string_clipped(wx + 14, c_y + 36, cap_label, COLOR_TEXT_MUTED, COLOR_TRANSPARENT, sb_w - 28);

    // System Folder
    int sys_y = c_y + 60;
    int is_sys = (active_folder == 2);
    if (is_sys) {
        gfx_fillrect(wx + 6, sys_y, sb_w - 12, 22, RGB(36, 44, 66));
        gfx_drawrect(wx + 6, sys_y, sb_w - 12, 22, COLOR_ACCENT);
    }
    gfx_draw_string(wx + 20, sys_y + 3, "⚙ System", is_sys ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT);
    char cnt_sys[8];
    snprintf(cnt_sys, sizeof(cnt_sys), "%d", count_folder_files(2));
    gfx_draw_string(wx + sb_w - 24, sys_y + 3, cnt_sys, is_sys ? COLOR_ACCENT : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // Divider
    int div2_y = sys_y + 28;
    gfx_draw_line(wx + 10, div2_y, wx + sb_w - 10, div2_y, COLOR_BORDER);

    // Section 3: DRIVE INFO
    int dev_y = div2_y + 8;
    gfx_draw_string(wx + 12, dev_y, "Drive Information", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    char disk_sz_str[32];
    vfs_get_disk_size_string(disk_sz_str, sizeof(disk_sz_str));
    char model_lbl[32];
    snprintf(model_lbl, sizeof(model_lbl), "%s", vfs_get_disk_model());
    gfx_draw_string_clipped(wx + 14, dev_y + 20, model_lbl, COLOR_WHITE, COLOR_TRANSPARENT, sb_w - 28);

    char cap_summary[32];
    snprintf(cap_summary, sizeof(cap_summary), "Size: %s", disk_sz_str);
    gfx_draw_string_clipped(wx + 14, dev_y + 38, cap_summary, COLOR_ACCENT, COLOR_TRANSPARENT, sb_w - 28);

    char count_summary[32];
    snprintf(count_summary, sizeof(count_summary), "Files: %d / %d", vfs_get_count(), VFS_MAX_FILES);
    gfx_draw_string_clipped(wx + 14, dev_y + 54, count_summary, COLOR_TEXT_MUTED, COLOR_TRANSPARENT, sb_w - 28);

    // =========================================================================
    // 4. Windows Details View (Right Main Pane)
    // =========================================================================
    int cx = wx + sb_w + 1;
    int cw = client_w - sb_w - 1;
    int head_h = 24;

    // Details Table Header Row
    gfx_fillrect(cx, body_y, cw, head_h, RGB(28, 30, 42));
    gfx_draw_line(cx, body_y + head_h - 1, cx + cw - 1, body_y + head_h - 1, COLOR_BORDER);

    // Columns: Name, Date modified, Type, Size
    int col_name_x = cx + 8;
    int col_date_x = cx + 180;
    int col_type_x = cx + 296;
    int col_size_x = cx + 418;

    gfx_draw_string(col_name_x, body_y + 5, "Name", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_draw_line(col_date_x - 8, body_y + 2, col_date_x - 8, body_y + head_h - 3, COLOR_BORDER);
    gfx_draw_string(col_date_x, body_y + 5, "Date modified", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_draw_line(col_type_x - 8, body_y + 2, col_type_x - 8, body_y + head_h - 3, COLOR_BORDER);
    gfx_draw_string(col_type_x, body_y + 5, "Type", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_draw_line(col_size_x - 8, body_y + 2, col_size_x - 8, body_y + head_h - 3, COLOR_BORDER);
    gfx_draw_string(col_size_x, body_y + 5, "Size", COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // Filtered Files List
    vfs_file_t *filtered[VFS_MAX_FILES];
    int filtered_count = get_filtered_files(filtered, VFS_MAX_FILES);

    if (selected_index >= filtered_count) selected_index = filtered_count - 1;
    if (selected_index < 0) selected_index = 0;

    int max_rows = 11;
    if (scroll_offset > filtered_count - max_rows) scroll_offset = filtered_count - max_rows;
    if (scroll_offset < 0) scroll_offset = 0;

    int row_h = 24;
    int list_y = body_y + head_h;

    for (int i = 0; i < max_rows; i++) {
        int file_idx = scroll_offset + i;
        int ry = list_y + (i * row_h);

        if (file_idx >= filtered_count) {
            // Draw empty row background
            gfx_fillrect(cx, ry, cw, row_h, (i % 2 == 0) ? RGB(18, 20, 28) : RGB(22, 24, 34));
            continue;
        }

        vfs_file_t *f = filtered[file_idx];
        if (!f) continue;

        int is_sel = (selected_index == file_idx);
        unsigned int row_bg = is_sel ? RGB(38, 70, 115) : ((i % 2 == 0) ? RGB(18, 20, 28) : RGB(22, 24, 34));

        gfx_fillrect(cx, ry, cw - 14, row_h, row_bg);
        if (is_sel) {
            gfx_drawrect(cx, ry, cw - 14, row_h, COLOR_ACCENT);
            gfx_fillrect(cx, ry + 2, 3, row_h - 4, COLOR_ACCENT);
        }

        // File icon badge
        const char *badge = "[DOC]";
        unsigned int badge_col = RGB(220, 225, 235);
        if (strstr(f->name, ".TXT"))      { badge = "[TXT]"; badge_col = RGB(137, 220, 235); }
        else if (strstr(f->name, ".CFG")) { badge = "[CFG]"; badge_col = RGB(249, 226, 175); }
        else if (strstr(f->name, ".SYS")) { badge = "[SYS]"; badge_col = RGB(203, 166, 247); }
        else if (strstr(f->name, ".SH"))  { badge = "[SH ]"; badge_col = RGB(166, 227, 161); }
        else if (strstr(f->name, ".LOG")) { badge = "[LOG]"; badge_col = RGB(148, 226, 213); }

        gfx_draw_string(col_name_x, ry + 4, badge, badge_col, COLOR_TRANSPARENT);

        // Filename
        gfx_draw_string_clipped(col_name_x + 46, ry + 4, f->name, is_sel ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT, 114);

        // Date modified
        char date_str[16];
        snprintf(date_str, sizeof(date_str), "%04u-%02u-%02u", f->created_year, f->created_month, f->created_day);
        gfx_draw_string_clipped(col_date_x, ry + 4, date_str, COLOR_TEXT_MUTED, COLOR_TRANSPARENT, 90);

        // Type description
        const char *type_desc = get_file_type_desc(f->name);
        gfx_draw_string_clipped(col_type_x, ry + 4, type_desc, COLOR_TEXT_MUTED, COLOR_TRANSPARENT, 110);

        // Size
        char sz_str[16];
        if (f->size >= 1024) {
            snprintf(sz_str, sizeof(sz_str), "%u KB", (f->size + 1023) / 1024);
        } else {
            snprintf(sz_str, sizeof(sz_str), "%u B", f->size);
        }
        gfx_draw_string_clipped(col_size_x, ry + 4, sz_str, is_sel ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT, 48);
    }

    // Scrollbar on the right edge
    int sc_x = cx + cw - 14;
    int sc_y = list_y;
    int sc_h = max_rows * row_h;
    gfx_fillrect(sc_x, sc_y, 14, sc_h, RGB(24, 26, 36));
    gfx_draw_line(sc_x, sc_y, sc_x, sc_y + sc_h - 1, COLOR_BORDER);

    // Up / Down arrow buttons
    gfx_draw_string(sc_x + 3, sc_y + 2, "^", (scroll_offset > 0) ? COLOR_WHITE : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
    gfx_draw_string(sc_x + 3, sc_y + sc_h - 14, "v", (scroll_offset + max_rows < filtered_count) ? COLOR_WHITE : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);

    // Scroll thumb
    if (filtered_count > max_rows) {
        int thumb_track = sc_h - 32;
        int thumb_h = (max_rows * thumb_track) / filtered_count;
        if (thumb_h < 12) thumb_h = 12;
        int thumb_y = sc_y + 16 + (scroll_offset * (thumb_track - thumb_h)) / (filtered_count - max_rows);
        gfx_fillrect(sc_x + 2, thumb_y, 10, thumb_h, RGB(60, 68, 92));
        gfx_drawrect(sc_x + 2, thumb_y, 10, thumb_h, COLOR_BORDER);
    }

    // =========================================================================
    // 5. Windows Status Bar (Bottom Strip, Height 24px)
    // =========================================================================
    int status_y = wy + client_h - 24;
    gfx_fillrect(wx, status_y, client_w, 24, RGB(22, 24, 34));
    gfx_draw_line(wx, status_y, wx + client_w - 1, status_y, COLOR_BORDER);

    char stat_items[32];
    snprintf(stat_items, sizeof(stat_items), "%d items", filtered_count);
    gfx_draw_string_clipped(wx + 14, status_y + 5, stat_items, COLOR_TEXT_MUTED, COLOR_TRANSPARENT, 100);

    vfs_file_t *sel_file = (filtered_count > 0 && selected_index < filtered_count) ? filtered[selected_index] : 0;
    if (sel_file) {
        char stat_sel[64];
        if (sel_file->size >= 1024) {
            snprintf(stat_sel, sizeof(stat_sel), "1 item selected  %u KB (%u bytes)", (sel_file->size + 1023) / 1024, sel_file->size);
        } else {
            snprintf(stat_sel, sizeof(stat_sel), "1 item selected  %u bytes", sel_file->size);
        }
        gfx_draw_string_clipped(wx + 130, status_y + 5, stat_sel, COLOR_WHITE, COLOR_TRANSPARENT, 240);
    } else {
        gfx_draw_string_clipped(wx + 130, status_y + 5, status_msg, COLOR_TEXT_MUTED, COLOR_TRANSPARENT, 240);
    }

    char disk_status[48];
    snprintf(disk_status, sizeof(disk_status), "Local Disk (C:) | ATA PIO Active");
    gfx_draw_string_clipped(wx + client_w - 270, status_y + 5, disk_status, COLOR_ACCENT, COLOR_TRANSPARENT, 260);

    // =========================================================================
    // 6. Interactive "New File" Modal
    // =========================================================================
    if (new_file_modal_open) {
        int pw = 420;
        int ph = 240;
        int px = (client_w - pw) / 2;
        int py = (client_h - ph) / 2;

        gfx_draw_shadow(wx + px, wy + py, pw, ph, 8);
        gfx_fillrect(wx + px, wy + py, pw, ph, RGB(26, 28, 40));
        gfx_drawrect(wx + px, wy + py, pw, ph, COLOR_ACCENT);

        // Modal titlebar
        gfx_fillrect(wx + px, wy + py, pw, 28, RGB(36, 40, 60));
        gfx_draw_line(wx + px, wy + py + 28, wx + px + pw, wy + py + 28, COLOR_BORDER);
        gfx_draw_string(wx + px + 12, wy + py + 6, "+ Create New File on Hard Disk", COLOR_WHITE, COLOR_TRANSPARENT);
        gfx_draw_string(wx + px + pw - 20, wy + py + 6, "X", RGB(240, 120, 130), COLOR_TRANSPARENT);

        // Folder field
        gfx_draw_string(wx + px + 16, wy + py + 38, "Folder / Destination:", COLOR_ACCENT, COLOR_TRANSPARENT);

        int fb_x = wx + px + 16;
        int fb_y = wy + py + 54;
        int fb_w = pw - 32;
        int fb_h = 24;
        gfx_fillrect(fb_x, fb_y, fb_w, fb_h, RGB(16, 18, 26));
        gfx_drawrect(fb_x, fb_y, fb_w, fb_h, (new_file_focus == 1) ? COLOR_ACCENT : COLOR_BORDER);

        char disp_f[48];
        snprintf(disp_f, sizeof(disp_f), "C:\\%s", new_file_folder_str);
        gfx_draw_string_clipped(fb_x + 8, fb_y + 4, disp_f, (new_file_focus == 1) ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT, fb_w - 16);

        // Folder preset buttons
        for (int i = 0; i < 3; i++) {
            int pbx = wx + px + 16 + (i * 128);
            int pby = wy + py + 84;
            int is_sel = (strcmp(new_file_folder_str, folder_presets[i]) == 0);
            gfx_fillrect(pbx, pby, 120, 20, is_sel ? RGB(46, 76, 128) : RGB(30, 34, 48));
            gfx_drawrect(pbx, pby, 120, 20, is_sel ? COLOR_ACCENT : COLOR_BORDER);
            gfx_draw_string(pbx + 8, pby + 2, folder_presets[i], is_sel ? COLOR_WHITE : COLOR_TEXT_MUTED, COLOR_TRANSPARENT);
        }

        // Filename field
        gfx_draw_string(wx + px + 16, wy + py + 112, "Filename:", COLOR_ACCENT, COLOR_TRANSPARENT);

        int ib_x = wx + px + 16;
        int ib_y = wy + py + 128;
        int ib_w = pw - 32;
        int ib_h = 24;
        gfx_fillrect(ib_x, ib_y, ib_w, ib_h, RGB(16, 18, 26));
        gfx_drawrect(ib_x, ib_y, ib_w, ib_h, (new_file_focus == 0) ? COLOR_ACCENT : COLOR_BORDER);
        gfx_draw_string_clipped(ib_x + 8, ib_y + 4, new_file_name, (new_file_focus == 0) ? COLOR_WHITE : COLOR_TEXT, COLOR_TRANSPARENT, ib_w - 16);

        if (new_file_focus == 0 && (pit_get_ticks() / 30) % 2 == 0) {
            int cur_x = ib_x + 8 + (new_file_name_len * 8);
            if (cur_x < ib_x + ib_w - 8) {
                gfx_fillrect(cur_x, ib_y + 4, 8, 16, COLOR_ACCENT);
            }
        }

        // Target path summary
        char target_str[64];
        snprintf(target_str, sizeof(target_str), "Target: C:\\%s\\%s", new_file_folder_str, new_file_name);
        gfx_draw_string_clipped(wx + px + 16, wy + py + 160, target_str, RGB(166, 227, 161), COLOR_TRANSPARENT, pw - 32);

        // Action buttons
        int save_btn_x = wx + px + pw - 210;
        int btn_y = wy + py + ph - 38;
        gfx_fillrect(save_btn_x, btn_y, 120, 26, RGB(46, 76, 128));
        gfx_drawrect(save_btn_x, btn_y, 120, 26, COLOR_ACCENT);
        gfx_draw_string(save_btn_x + 12, btn_y + 5, "Create File", COLOR_WHITE, COLOR_TRANSPARENT);

        int cancel_btn_x = wx + px + pw - 80;
        gfx_fillrect(cancel_btn_x, btn_y, 70, 26, RGB(34, 38, 56));
        gfx_drawrect(cancel_btn_x, btn_y, 70, 26, COLOR_BORDER);
        gfx_draw_string(cancel_btn_x + 12, btn_y + 5, "Cancel", COLOR_TEXT, COLOR_TRANSPARENT);
    }

    // =========================================================================
    // 7. Interactive "Move File" Modal
    // =========================================================================
    if (move_modal_open) {
        int pw = 380;
        int ph = 200;
        int px = (client_w - pw) / 2;
        int py = (client_h - ph) / 2;

        gfx_draw_shadow(wx + px, wy + py, pw, ph, 8);
        gfx_fillrect(wx + px, wy + py, pw, ph, RGB(26, 28, 40));
        gfx_drawrect(wx + px, wy + py, pw, ph, COLOR_ACCENT);

        gfx_fillrect(wx + px, wy + py, pw, 28, RGB(36, 40, 60));
        gfx_draw_line(wx + px, wy + py + 28, wx + px + pw, wy + py + 28, COLOR_BORDER);
        gfx_draw_string(wx + px + 12, wy + py + 6, "Move File to Destination Folder", COLOR_WHITE, COLOR_TRANSPARENT);
        gfx_draw_string(wx + px + pw - 20, wy + py + 6, "X", RGB(240, 120, 130), COLOR_TRANSPARENT);

        gfx_draw_string(wx + px + 16, wy + py + 38, "Destination Folder:", COLOR_ACCENT, COLOR_TRANSPARENT);

        int mb_x = wx + px + 16;
        int mb_y = wy + py + 56;
        int mb_w = pw - 32;
        int mb_h = 24;
        gfx_fillrect(mb_x, mb_y, mb_w, mb_h, RGB(16, 18, 26));
        gfx_drawrect(mb_x, mb_y, mb_w, mb_h, COLOR_ACCENT);

        char disp_m[48];
        snprintf(disp_m, sizeof(disp_m), "C:\\%s", move_target_folder);
        gfx_draw_string_clipped(mb_x + 8, mb_y + 4, disp_m, COLOR_WHITE, COLOR_TRANSPARENT, mb_w - 16);

        // Presets
        for (int i = 0; i < 3; i++) {
            int pbx = wx + px + 16 + (i * 115);
            int pby = wy + py + 90;
            gfx_fillrect(pbx, pby, 105, 22, RGB(30, 34, 48));
            gfx_drawrect(pbx, pby, 105, 22, COLOR_BORDER);
            gfx_draw_string(pbx + 8, pby + 3, folder_presets[i], COLOR_TEXT, COLOR_TRANSPARENT);
        }

        int mv_btn_x = wx + px + pw - 190;
        int btn_y = wy + py + ph - 38;
        gfx_fillrect(mv_btn_x, btn_y, 100, 26, RGB(46, 76, 128));
        gfx_drawrect(mv_btn_x, btn_y, 100, 26, COLOR_ACCENT);
        gfx_draw_string(mv_btn_x + 12, btn_y + 5, "Move File", COLOR_WHITE, COLOR_TRANSPARENT);

        int c_btn_x = wx + px + pw - 80;
        gfx_fillrect(c_btn_x, btn_y, 70, 26, RGB(34, 38, 56));
        gfx_drawrect(c_btn_x, btn_y, 70, 26, COLOR_BORDER);
        gfx_draw_string(c_btn_x + 12, btn_y + 5, "Cancel", COLOR_TEXT, COLOR_TRANSPARENT);
    }
}

static void files_click(window_t *win, int rx, int ry, int btn) {
    (void)btn;
    int client_w = win->width;
    int client_h = win->height - TITLEBAR_HEIGHT;
    int rb_h = 32;
    int ab_h = 32;
    int sb_w = 175;

    // -------------------------------------------------------------------------
    // Handle "Move File" Modal clicks
    // -------------------------------------------------------------------------
    if (move_modal_open) {
        int pw = 380;
        int ph = 200;
        int px = (client_w - pw) / 2;
        int py = (client_h - ph) / 2;

        if (rx >= px + pw - 24 && rx <= px + pw && ry >= py && ry <= py + 28) {
            move_modal_open = 0;
            return;
        }

        // Preset buttons
        for (int i = 0; i < 3; i++) {
            int pbx = px + 16 + (i * 115);
            int pby = py + 90;
            if (rx >= pbx && rx <= pbx + 105 && ry >= pby && ry <= pby + 22) {
                strncpy(move_target_folder, folder_presets[i], sizeof(move_target_folder) - 1);
                move_target_folder_len = strlen(move_target_folder);
                return;
            }
        }

        int mv_btn_x = px + pw - 190;
        int btn_y = py + ph - 38;
        if (rx >= mv_btn_x && rx <= mv_btn_x + 100 && ry >= btn_y && ry <= btn_y + 26) {
            files_confirm_move();
            return;
        }

        int c_btn_x = px + pw - 80;
        if (rx >= c_btn_x && rx <= c_btn_x + 70 && ry >= btn_y && ry <= btn_y + 26) {
            move_modal_open = 0;
            return;
        }
        return;
    }

    // -------------------------------------------------------------------------
    // Handle "New File" Modal clicks
    // -------------------------------------------------------------------------
    if (new_file_modal_open) {
        int pw = 420;
        int ph = 240;
        int px = (client_w - pw) / 2;
        int py = (client_h - ph) / 2;

        if (rx >= px + pw - 24 && rx <= px + pw && ry >= py && ry <= py + 28) {
            new_file_modal_open = 0;
            return;
        }

        // Focus folder input
        int fb_x = px + 16;
        int fb_y = py + 54;
        if (rx >= fb_x && rx <= fb_x + pw - 32 && ry >= fb_y && ry <= fb_y + 24) {
            new_file_focus = 1;
            return;
        }

        // Presets
        for (int i = 0; i < 3; i++) {
            int pbx = px + 16 + (i * 128);
            int pby = py + 84;
            if (rx >= pbx && rx <= pbx + 120 && ry >= pby && ry <= pby + 20) {
                strncpy(new_file_folder_str, folder_presets[i], sizeof(new_file_folder_str) - 1);
                new_file_folder_len = strlen(new_file_folder_str);
                return;
            }
        }

        // Focus filename input
        int ib_x = px + 16;
        int ib_y = py + 128;
        if (rx >= ib_x && rx <= ib_x + pw - 32 && ry >= ib_y && ry <= ib_y + 24) {
            new_file_focus = 0;
            return;
        }

        // Create File button
        int save_btn_x = px + pw - 210;
        int btn_y = py + ph - 38;
        if (rx >= save_btn_x && rx <= save_btn_x + 120 && ry >= btn_y && ry <= btn_y + 26) {
            files_confirm_new_file();
            return;
        }

        // Cancel button
        int cancel_btn_x = px + pw - 80;
        if (rx >= cancel_btn_x && rx <= cancel_btn_x + 70 && ry >= btn_y && ry <= btn_y + 26) {
            new_file_modal_open = 0;
            return;
        }
        return;
    }

    // -------------------------------------------------------------------------
    // 1. Command Ribbon Clicks (Row 1, ry between 0 and 32)
    // -------------------------------------------------------------------------
    if (ry >= 0 && ry < rb_h) {
        // [ + New File ]
        int btn_new_x = 10;
        if (rx >= btn_new_x && rx <= btn_new_x + 96) {
            files_open_new_modal();
            return;
        }

        // [ Open in Notes ]
        int btn_open_x = btn_new_x + 104;
        if (rx >= btn_open_x && rx <= btn_open_x + 116) {
            vfs_file_t *filtered[VFS_MAX_FILES];
            int count = get_filtered_files(filtered, VFS_MAX_FILES);
            if (count > 0 && selected_index < count) {
                app_notes_open_file(filtered[selected_index]->name);
                snprintf(status_msg, sizeof(status_msg), "Opened %s in Notes", filtered[selected_index]->name);
            }
            return;
        }

        // [ Delete ]
        int btn_del_x = btn_open_x + 124;
        if (rx >= btn_del_x && rx <= btn_del_x + 72) {
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

        // [ Move ]
        int btn_mov_x = btn_del_x + 80;
        if (rx >= btn_mov_x && rx <= btn_mov_x + 60) {
            vfs_file_t *filtered[VFS_MAX_FILES];
            int count = get_filtered_files(filtered, VFS_MAX_FILES);
            if (count > 0 && selected_index < count) {
                move_modal_open = 1;
                strncpy(move_target_folder, filtered[selected_index]->folder, sizeof(move_target_folder) - 1);
                move_target_folder_len = strlen(move_target_folder);
            }
            return;
        }

        // [ Refresh ]
        int btn_ref_x = btn_mov_x + 68;
        if (rx >= btn_ref_x && rx <= btn_ref_x + 74) {
            path_bar_editing = 0;
            search_bar_editing = 0;
            snprintf(status_msg, sizeof(status_msg), "View refreshed");
            return;
        }
    }

    // -------------------------------------------------------------------------
    // 2. Address & Navigation Row Clicks (Row 2, ry between 32 and 64)
    // -------------------------------------------------------------------------
    if (ry >= rb_h && ry < rb_h + ab_h) {
        int nav_back_x = 10;
        int nav_fwd_x  = nav_back_x + 28;
        int nav_up_x   = nav_fwd_x + 28;

        // [ < ] Back
        if (rx >= nav_back_x && rx <= nav_back_x + 24) {
            files_go_back();
            return;
        }

        // [ > ] Forward
        if (rx >= nav_fwd_x && rx <= nav_fwd_x + 24) {
            files_go_forward();
            return;
        }

        // [ ^ ] Up
        if (rx >= nav_up_x && rx <= nav_up_x + 24) {
            files_go_up();
            return;
        }

        // Address Bar
        int search_w = 140;
        int adr_x = nav_up_x + 34;
        int adr_w = client_w - adr_x - search_w - 20;
        if (adr_w < 180) adr_w = 180;

        if (rx >= adr_x && rx <= adr_x + adr_w) {
            path_bar_editing = 1;
            search_bar_editing = 0;
            return;
        }

        // Search Box
        int search_x = adr_x + adr_w + 10;
        if (rx >= search_x && rx <= search_x + search_w) {
            search_bar_editing = 1;
            path_bar_editing = 0;
            return;
        }
    }

    // -------------------------------------------------------------------------
    // 3. Left Navigation Pane Clicks (rx between 0 and 175)
    // -------------------------------------------------------------------------
    int body_y = rb_h + ab_h;
    if (rx >= 0 && rx <= sb_w) {
        path_bar_editing = 0;
        search_bar_editing = 0;

        int tree_y = body_y + 10;
        int py0 = tree_y + 22; // Desktop
        int py1 = py0 + 24;    // Documents
        int py2 = py1 + 24;    // All Files

        if (ry >= py0 && ry <= py0 + 22) { files_navigate_to_path("C:\\Documents"); return; }
        if (ry >= py1 && ry <= py1 + 22) { files_navigate_to_path("C:\\Documents"); return; }
        if (ry >= py2 && ry <= py2 + 22) { files_navigate_to_path("C:\\Storage"); return; }

        int div1_y = py2 + 30;
        int pc_y = div1_y + 8;
        int c_y = pc_y + 20;   // Local Disk C
        if (ry >= c_y && ry <= c_y + 54) { files_navigate_to_path("C:\\Storage"); return; }

        int sys_y = c_y + 60;  // System
        if (ry >= sys_y && ry <= sys_y + 22) { files_navigate_to_path("C:\\System"); return; }
        return;
    }

    // -------------------------------------------------------------------------
    // 4. Details View Clicks & Double-Click Detection
    // -------------------------------------------------------------------------
    int cx = sb_w + 1;
    int cw = client_w - sb_w - 1;
    int head_h = 24;
    int list_y = body_y + head_h;
    int row_h = 24;
    int max_rows = 11;

    // Scrollbar arrow buttons
    int sc_x = cx + cw - 14;
    int sc_y = list_y;
    int sc_h = max_rows * row_h;

    vfs_file_t *filtered[VFS_MAX_FILES];
    int filtered_count = get_filtered_files(filtered, VFS_MAX_FILES);

    if (rx >= sc_x && rx <= sc_x + 14 && ry >= sc_y && ry <= sc_y + sc_h) {
        if (ry <= sc_y + 16) {
            if (scroll_offset > 0) scroll_offset--;
        } else if (ry >= sc_y + sc_h - 16) {
            if (scroll_offset + max_rows < filtered_count) scroll_offset++;
        }
        return;
    }

    // File rows
    for (int i = 0; i < max_rows; i++) {
        int file_idx = scroll_offset + i;
        if (file_idx >= filtered_count) break;

        int ry_row = list_y + (i * row_h);
        if (rx >= cx && rx <= sc_x && ry >= ry_row && ry <= ry_row + row_h) {
            path_bar_editing = 0;
            search_bar_editing = 0;

            unsigned int now = pit_get_ticks();
            if (selected_index == file_idx && last_clicked_index == file_idx && (now - last_click_time) < 40) {
                app_notes_open_file(filtered[file_idx]->name);
                snprintf(status_msg, sizeof(status_msg), "Opened %s in Notes", filtered[file_idx]->name);
                last_click_time = 0;
                last_clicked_index = -1;
                return;
            }

            selected_index = file_idx;
            last_clicked_index = file_idx;
            last_click_time = now;
            return;
        }
    }
}

static void files_key(window_t *win, char key) {
    (void)win;

    // Keyboard handling inside "Move File" modal
    if (move_modal_open) {
        if (key == 27) { move_modal_open = 0; return; }
        if (key == '\n') { files_confirm_move(); return; }
        if (key == '\b') {
            if (move_target_folder_len > 0) move_target_folder[--move_target_folder_len] = '\0';
            return;
        }
        if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9') || key == '_' || key == '-' || key == '\\') {
            if (move_target_folder_len < VFS_MAX_FOLDER - 2) {
                move_target_folder[move_target_folder_len++] = key;
                move_target_folder[move_target_folder_len] = '\0';
            }
        }
        return;
    }

    // Keyboard handling inside "New File" modal
    if (new_file_modal_open) {
        if (key == 27) { new_file_modal_open = 0; return; }
        if (key == '\n') { files_confirm_new_file(); return; }
        if (key == '\t') { new_file_focus = !new_file_focus; return; }

        if (new_file_focus == 1) { // Editing Folder
            if (key == '\b') {
                if (new_file_folder_len > 0) new_file_folder_str[--new_file_folder_len] = '\0';
                return;
            }
            if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9') || key == '_' || key == '-' || key == '\\' || key == '/') {
                if (new_file_folder_len < VFS_MAX_FOLDER - 2) {
                    new_file_folder_str[new_file_folder_len++] = key;
                    new_file_folder_str[new_file_folder_len] = '\0';
                }
            }
        } else { // Editing Filename
            if (key == '\b') {
                if (new_file_name_len > 0) new_file_name[--new_file_name_len] = '\0';
                return;
            }
            if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9') || key == '.' || key == '_' || key == '-') {
                if (new_file_name_len < VFS_MAX_FILENAME - 2) {
                    if (key >= 'a' && key <= 'z') key -= 32;
                    new_file_name[new_file_name_len++] = key;
                    new_file_name[new_file_name_len] = '\0';
                }
            }
        }
        return;
    }

    // Keyboard handling inside Search Bar
    if (search_bar_editing) {
        if (key == 27) {
            search_bar_editing = 0;
            search_query_len = 0;
            search_query[0] = '\0';
            selected_index = 0;
            scroll_offset = 0;
            return;
        }
        if (key == '\n') {
            search_bar_editing = 0;
            return;
        }
        if (key == '\b') {
            if (search_query_len > 0) {
                search_query[--search_query_len] = '\0';
                selected_index = 0;
                scroll_offset = 0;
            }
            return;
        }
        if ((key >= 32 && key <= 126) && search_query_len < 30) {
            search_query[search_query_len++] = key;
            search_query[search_query_len] = '\0';
            selected_index = 0;
            scroll_offset = 0;
            return;
        }
        return;
    }

    // Keyboard handling inside Address Bar
    if (path_bar_editing) {
        if (key == 27) {
            path_bar_editing = 0;
            return;
        }
        if (key == '\n') {
            files_navigate_to_path(current_path_str);
            return;
        }
        if (key == '\b') {
            if (current_path_len > 0) {
                current_path_str[--current_path_len] = '\0';
            }
            return;
        }
        if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9') || key == ':' || key == '\\' || key == '/' || key == '_' || key == '-' || key == '.') {
            if (current_path_len < (int)sizeof(current_path_str) - 2) {
                current_path_str[current_path_len++] = key;
                current_path_str[current_path_len] = '\0';
            }
            return;
        }
        return;
    }

    // Standard list navigation
    vfs_file_t *filtered[VFS_MAX_FILES];
    int count = get_filtered_files(filtered, VFS_MAX_FILES);
    int max_rows = 11;

    if (key == 'w' || key == 'W') {
        if (selected_index > 0) {
            selected_index--;
            if (selected_index < scroll_offset) {
                scroll_offset = selected_index;
            }
        }
    } else if (key == 's' || key == 'S') {
        if (selected_index < count - 1) {
            selected_index++;
            if (selected_index >= scroll_offset + max_rows) {
                scroll_offset = selected_index - max_rows + 1;
            }
        }
    } else if (key == 'p' || key == 'P') {
        path_bar_editing = 1;
    } else if (key == 'n' || key == 'N') {
        files_open_new_modal();
    } else if (key == '\n') {
        if (count > 0 && selected_index < count) {
            app_notes_open_file(filtered[selected_index]->name);
            snprintf(status_msg, sizeof(status_msg), "Opened %s in Notes", filtered[selected_index]->name);
        }
    } else if (key == 'd' || key == 'D') {
        if (count > 0 && selected_index < count) {
            vfs_file_t *f = filtered[selected_index];
            int res = vfs_delete_file(f->name);
            if (res == 0) {
                snprintf(status_msg, sizeof(status_msg), "Deleted %s from disk", f->name);
                if (selected_index >= count - 1) selected_index = count - 2;
                if (selected_index < 0) selected_index = 0;
            } else if (res == -2) {
                snprintf(status_msg, sizeof(status_msg), "Cannot delete system file!");
            }
        }
    }
}

void app_files_launch(void) {
    if (history_count == 0) {
        files_push_history("C:\\Storage");
    }
    window_t *win = wm_create_window("File Explorer", 140, 70, 660, 440, RGB(22, 25, 38));
    if (!win) return;
    win->draw_client = files_draw;
    win->on_click = files_click;
    win->on_key = files_key;
}
