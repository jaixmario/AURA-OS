#include "apps.h"
#include "../kernel.h"
#include "../gfx/gfx.h"
#include "../arch/pit.h"
#include "../arch/rtc.h"
#include "../fs/vfs.h"
#include "../arch/io.h"
#include "../arch/display.h"
#include "../libc/string.h"

#define TERM_MAX_LINES 16
#define TERM_LINE_LEN  64

static char term_lines[TERM_MAX_LINES][TERM_LINE_LEN];
static int term_line_count = 0;
static char input_buf[TERM_LINE_LEN];
static int input_len = 0;

static void term_add_line(const char *text) {
    if (term_line_count < TERM_MAX_LINES) {
        strncpy(term_lines[term_line_count], text, TERM_LINE_LEN - 1);
        term_line_count++;
    } else {
        // Shift up
        for (int i = 0; i < TERM_MAX_LINES - 1; i++) {
            strcpy(term_lines[i], term_lines[i + 1]);
        }
        strncpy(term_lines[TERM_MAX_LINES - 1], text, TERM_LINE_LEN - 1);
    }
}

static char term_cwd[32] = "Documents";

static void term_execute_command(void) {
    if (input_len == 0) return;

    // Echo command
    char echo[TERM_LINE_LEN + 16];
    snprintf(echo, sizeof(echo), "> %s", input_buf);
    term_add_line(echo);

    if (strcmp(input_buf, "help") == 0) {
        term_add_line("AuraOS Terminal Commands:");
        term_add_line("  files    - open File Explorer GUI");
        term_add_line("  edit <f> - edit file in Notes Editor");
        term_add_line("  cd <path>- change current directory path");
        term_add_line("  pwd      - print current directory path");
        term_add_line("  ls / dir - list files in current directory");
        term_add_line("  cat <f>  - display file contents");
        term_add_line("  touch <f>- create empty file");
        term_add_line("  rm <f>   - delete file");
        term_add_line("  mv <f> <p>- move file to destination path");
        term_add_line("  disk     - inspect physical ATA hard drive");
        term_add_line("  paint    - open Canvas Paint app");
        term_add_line("  notes    - open Notes Editor app");
        term_add_line("  calc     - open Calculator app");
        term_add_line("  settings - open Settings Control Panel");
        term_add_line("  sysinfo  - open System Specs app");
        term_add_line("  date/time- show CMOS hardware clock");
        term_add_line("  sync     - synchronize clock with CMOS");
        term_add_line("  mem      - inspect memory regions");
        term_add_line("  uptime   - show system uptime");
        term_add_line("  whoami   - show user and hostname");
        term_add_line("  lock     - lock desktop and show login screen");
        term_add_line("  wallpapers- list all 8 desktop wallpapers");
        term_add_line("  theme <n>- set desktop wallpaper (0-7)");
        term_add_line("  res [w h]- switch display size (e.g. res 1920 1080)");
        if (!sys_is_installed()) {
            term_add_line("  install  - launch AuraOS installer");
        }
        term_add_line("  clear/cls- clear terminal screen");
        term_add_line("  echo <t> - print message");
        term_add_line("  reboot   - restart virtual machine");
    } else if (strncmp(input_buf, "cd ", 3) == 0) {
        const char *new_p = input_buf + 3;
        while (*new_p == ' ') new_p++;
        if ((new_p[0] == 'C' || new_p[0] == 'c') && new_p[1] == ':' && (new_p[2] == '\\' || new_p[2] == '/')) {
            new_p += 3;
        } else if (new_p[0] == '\\' || new_p[0] == '/') {
            new_p += 1;
        }
        if (new_p[0] == '\0' || strcmp(new_p, "..") == 0) {
            strcpy(term_cwd, "Storage");
        } else {
            strncpy(term_cwd, new_p, sizeof(term_cwd) - 1);
            term_cwd[sizeof(term_cwd) - 1] = '\0';
        }
        char msg[48];
        snprintf(msg, sizeof(msg), "Changed path to C:\\%s", term_cwd);
        term_add_line(msg);
    } else if (strcmp(input_buf, "pwd") == 0) {
        char msg[48];
        snprintf(msg, sizeof(msg), "C:\\%s", term_cwd);
        term_add_line(msg);
    } else if (strcmp(input_buf, "disk") == 0) {
        char cap[32];
        vfs_get_disk_size_string(cap, sizeof(cap));
        char msg1[64], msg2[64];
        snprintf(msg1, sizeof(msg1), "Disk: %s (%s)", vfs_get_disk_model(), cap);
        snprintf(msg2, sizeof(msg2), "Total Sectors: %u (ATA PIO Active)", vfs_get_disk_sectors());
        term_add_line(msg1);
        term_add_line(msg2);
    } else if (strncmp(input_buf, "mv ", 3) == 0) {
        char fname[32] = {0}, fdest[32] = {0};
        const char *args = input_buf + 3;
        while (*args == ' ') args++;
        int ai = 0;
        while (*args && *args != ' ' && ai < 31) fname[ai++] = *args++;
        while (*args == ' ') args++;
        int di = 0;
        while (*args && *args != ' ' && di < 31) fdest[di++] = *args++;
        if (fname[0] && fdest[0]) {
            int res = vfs_move_file(fname, fdest);
            if (res == 0) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Moved %s to C:\\%s", fname, fdest);
                term_add_line(msg);
            } else {
                term_add_line("Error: File not found.");
            }
        } else {
            term_add_line("Usage: mv <filename> <destination_folder>");
        }
    } else if (strcmp(input_buf, "files") == 0 || strcmp(input_buf, "explorer") == 0) {
        app_files_launch();
        term_add_line("Launched File Explorer.");
    } else if (strcmp(input_buf, "ls") == 0 || strcmp(input_buf, "dir") == 0) {
        char head[48];
        snprintf(head, sizeof(head), "Directory of C:\\%s:", term_cwd);
        term_add_line(head);
        int cnt = vfs_get_count();
        for (int i = 0; i < cnt; i++) {
            vfs_file_t *f = vfs_get_at(i);
            if (strcmp(term_cwd, "Storage") == 0 || strcmp(term_cwd, "All") == 0 || strcmp(f->folder, term_cwd) == 0) {
                char line[64];
                snprintf(line, sizeof(line), "  %-12s [%-9s] %5u B  %04u-%02u-%02u",
                         f->name, f->folder, f->size, f->created_year, f->created_month, f->created_day);
                term_add_line(line);
            }
        }
    } else if (strncmp(input_buf, "cat ", 4) == 0) {
        const char *fname = input_buf + 4;
        vfs_file_t *f = vfs_find(fname);
        if (f) {
            char snippet[60];
            int s_i = 0;
            for (int k = 0; k < (int)f->size && k < 58; k++) {
                char c = f->data[k];
                if (c == '\n' || c == '\r') break;
                snippet[s_i++] = c;
            }
            snippet[s_i] = '\0';
            term_add_line(snippet);
        } else {
            term_add_line("Error: File not found.");
        }
    } else if (strncmp(input_buf, "touch ", 6) == 0) {
        const char *fname = input_buf + 6;
        if (vfs_create_file(fname, term_cwd, "", 0, FS_ATTR_USER) == 0) {
            term_add_line("File created on hard disk.");
        } else {
            term_add_line("Error: Cannot create file.");
        }
    } else if (strncmp(input_buf, "rm ", 3) == 0) {
        const char *fname = input_buf + 3;
        int res = vfs_delete_file(fname);
        if (res == 0) {
            term_add_line("File deleted successfully.");
        } else if (res == -2) {
            term_add_line("Error: Cannot delete protected system file.");
        } else {
            term_add_line("Error: File not found.");
        }
    } else if (strcmp(input_buf, "paint") == 0) {
        app_paint_launch();
        term_add_line("Launched Canvas Paint.");
    } else if (strncmp(input_buf, "edit ", 5) == 0) {
        const char *fname = input_buf + 5;
        app_notes_open_file(fname);
        term_add_line("Opened file in Notes Editor.");
    } else if (strncmp(input_buf, "notes ", 6) == 0) {
        const char *fname = input_buf + 6;
        app_notes_open_file(fname);
        term_add_line("Opened file in Notes Editor.");
    } else if (strcmp(input_buf, "notes") == 0) {
        app_notes_launch();
        term_add_line("Launched Notes Editor.");
    } else if (strcmp(input_buf, "calc") == 0) {
        app_calc_launch();
        term_add_line("Launched Calculator.");
    } else if (strcmp(input_buf, "settings") == 0) {
        app_settings_launch();
        term_add_line("Launched Settings Control Panel.");
    } else if (strcmp(input_buf, "sysinfo") == 0) {
        app_sysinfo_launch();
        term_add_line("Launched System Info.");
    } else if (strcmp(input_buf, "mem") == 0) {
        term_add_line("Memory Architecture Layout (Extended RAM):");
        term_add_line("  Kernel Code : 0x00010000 (Flat Model)");
        term_add_line("  Kernel Stack: 0x001FFFF0 (1MB Ring 0 Stack)");
        term_add_line("  VRAM Buffer : 0x00200000 (10MB Double Buffer)");
        term_add_line("  Wallpaper   : 0x00C00000 (10MB Dynamic Buffer)");
        term_add_line("  Paint Canvas: 0x01600000 (10MB Extended Canvas)");
        term_add_line("  VFS Cache   : 0x02000000 (8MB Storage RAM)");
        term_add_line("  Notes Buffer: 0x02800000 (8MB Document RAM)");
    } else if (strcmp(input_buf, "res") == 0 || strcmp(input_buf, "display") == 0) {
        term_add_line("AuraOS Display Resolutions & Monitor Info:");
        edid_info_t edid;
        display_get_edid_info(&edid);
        char mon_line[64];
        snprintf(mon_line, sizeof(mon_line), "  Monitor: %s (%dx%d %s)",
                 edid.monitor_name, edid.native_w, edid.native_h, edid.aspect);
        term_add_line(mon_line);
        term_add_line("Supported Modes:");
        int cur_idx = display_get_current_mode_index();
        for (int i = 0; i < display_get_mode_count(); i++) {
            const display_mode_t *m = display_get_mode(i);
            char line[64];
            snprintf(line, sizeof(line), "  %s[%d] %-11s (%s) - %s",
                     (i == cur_idx) ? "* " : "  ",
                     i, m->label, m->aspect, m->desc);
            term_add_line(line);
        }
        term_add_line("Usage: res auto         (Auto-detect native monitor size)");
        term_add_line("   or: res <w> <h>      (e.g. res 1920 1080)");
        term_add_line("   or: res <id>         (e.g. res 0)");
    } else if (strncmp(input_buf, "res ", 4) == 0 || strncmp(input_buf, "display ", 8) == 0) {
        const char *arg = input_buf;
        while (*arg && *arg != ' ') arg++;
        while (*arg == ' ') arg++;

        if (strcmp(arg, "auto") == 0) {
            edid_info_t edid;
            display_get_edid_info(&edid);
            int res = display_auto_detect();
            char msg[64];
            if (res == 1) {
                snprintf(msg, sizeof(msg), "[+] Auto-detected & applied %s (%dx%d) live!",
                         edid.monitor_name, edid.native_w, edid.native_h);
            } else {
                snprintf(msg, sizeof(msg), "[+] Auto-detected %s (%dx%d) saved to config!",
                         edid.monitor_name, edid.native_w, edid.native_h);
            }
            term_add_line(msg);
        } else if (arg[0] >= '0' && arg[0] <= '7' && (arg[1] == '\0' || arg[1] == ' ')) {
            int idx = arg[0] - '0';
            const display_mode_t *m = display_get_mode(idx);
            int res = display_set_mode_by_index(idx);
            char msg[64];
            if (res == 1) {
                snprintf(msg, sizeof(msg), "[+] Display switched live to %s!", m->label);
            } else {
                snprintf(msg, sizeof(msg), "[+] %s saved to boot config! Type 'reboot' to apply.", m->label);
            }
            term_add_line(msg);
        } else {
            int w = 0, h = 0;
            while (*arg >= '0' && *arg <= '9') {
                w = w * 10 + (*arg - '0');
                arg++;
            }
            while (*arg == ' ' || *arg == 'x' || *arg == 'X') arg++;
            while (*arg >= '0' && *arg <= '9') {
                h = h * 10 + (*arg - '0');
                arg++;
            }
            if (w >= 640 && h >= 480) {
                int res = display_set_resolution(w, h);
                char msg[80];
                if (res == 1) {
                    snprintf(msg, sizeof(msg), "[+] Display switched live to %dx%d!", w, h);
                } else {
                    snprintf(msg, sizeof(msg), "[+] %dx%d saved to boot config! Type 'reboot' to apply.", w, h);
                }
                term_add_line(msg);
            } else {
                term_add_line("Usage: res <width> <height> (e.g. res 1920 1080)");
            }
        }
    } else if (strcmp(input_buf, "ver") == 0 || strcmp(input_buf, "version") == 0) {
        term_add_line("AuraOS Version 1.2.0 [i686 Protected Mode]");
    } else if (strcmp(input_buf, "date") == 0 || strcmp(input_buf, "time") == 0) {
        rtc_time_t t;
        rtc_get_datetime(&t);
        char msg[64];
        snprintf(msg, sizeof(msg), "CMOS RTC: %04u-%02u-%02u %02u:%02u:%02u (Port 0x70/0x71)",
                 t.year, t.month, t.day, t.hour, t.minute, t.second);
        term_add_line(msg);
    } else if (strcmp(input_buf, "sync") == 0) {
        rtc_sync_from_cmos();
        term_add_line("Synchronized clock with CMOS hardware RTC.");
    } else if (strcmp(input_buf, "info") == 0) {
        char imsg[64];
        snprintf(imsg, sizeof(imsg), "Video: %dx%d Dynamic TrueColor (BGA / VBE 2.0+)", gfx_get_width(), gfx_get_height());
        term_add_line("AuraOS v1.2 [i686 Protected Mode]");
        term_add_line(imsg);
        term_add_line("Drivers: PS/2 Mouse & Keyboard, PIT 100Hz, CMOS RTC");
    } else if (strcmp(input_buf, "clear") == 0 || strcmp(input_buf, "cls") == 0) {
        term_line_count = 0;
    } else if (strcmp(input_buf, "uptime") == 0) {
        unsigned int sec = pit_get_uptime_seconds();
        char msg[48];
        snprintf(msg, sizeof(msg), "System Uptime: %u seconds (%u ticks)", sec, pit_get_ticks());
        term_add_line(msg);
    } else if (strncmp(input_buf, "echo ", 5) == 0) {
        term_add_line(input_buf + 5);
    } else if (strcmp(input_buf, "install") == 0) {
        if (!sys_is_installed()) {
            term_add_line("Launching AuraOS Setup Wizard...");
            app_installer_launch();
        } else {
            term_add_line("AuraOS is already installed on this hard disk.");
        }
    } else if (strcmp(input_buf, "whoami") == 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "%s@%s [%s]",
                 sys_get_username(), sys_get_hostname(),
                 sys_is_installed() ? "Installed" : "Live Media");
        term_add_line(msg);
    } else if (strcmp(input_buf, "lock") == 0) {
        term_add_line("Locking desktop...");
        sys_lock_screen();
    } else if (strcmp(input_buf, "wallpapers") == 0 || strcmp(input_buf, "themes") == 0) {
        char hdr[64];
        snprintf(hdr, sizeof(hdr), "Native 1024x768 Photographic Wallpapers (0-%d):", get_theme_count() - 1);
        term_add_line(hdr);
        for (int i = 0; i < get_theme_count(); i++) {
            char tmsg[64];
            snprintf(tmsg, sizeof(tmsg), "  [%d] %-18s - %s", i, get_theme_name(i), get_theme_desc(i));
            term_add_line(tmsg);
        }
        term_add_line("Type 'theme <id>' or 'wallpaper <id>' to apply.");
    } else if (strncmp(input_buf, "theme ", 6) == 0 || strncmp(input_buf, "wallpaper ", 10) == 0) {
        const char *arg = input_buf;
        while (*arg && *arg != ' ') arg++;
        while (*arg == ' ') arg++;
        if (*arg >= '0' && *arg < '0' + get_theme_count()) {
            int t = *arg - '0';
            set_desktop_theme(t);
            char tmsg[64];
            snprintf(tmsg, sizeof(tmsg), "Applied wallpaper: [%d] %s (Saved)", t, get_theme_name(t));
            term_add_line(tmsg);
        } else {
            char umsg[64];
            snprintf(umsg, sizeof(umsg), "Usage: theme <0-%d>  (type 'wallpapers' to list)", get_theme_count() - 1);
            term_add_line(umsg);
        }
    } else if (strcmp(input_buf, "about") == 0) {
        term_add_line("AuraOS - Built from scratch with C and Assembly");
        term_add_line("Custom Bare-Metal Graphical Operating System");
    } else if (strcmp(input_buf, "reboot") == 0) {
        term_add_line("Rebooting system...");
        sys_reboot();
    } else {
        char err[64];
        snprintf(err, sizeof(err), "Unknown command: %s (type 'help')", input_buf);
        term_add_line(err);
    }

    input_len = 0;
    input_buf[0] = '\0';
}

static void term_draw(window_t *win) {
    int start_y = win->y + TITLEBAR_HEIGHT + 8;
    int x = win->x + 12;
    int max_w = win->width - 24;

    // Draw previous lines
    for (int i = 0; i < term_line_count; i++) {
        gfx_draw_string_clipped(x, start_y + (i * 18), term_lines[i], COLOR_TEXT, COLOR_TRANSPARENT, max_w);
    }

    // Draw active prompt
    int prompt_y = start_y + (term_line_count * 18);
    char prompt_str[64];
    snprintf(prompt_str, sizeof(prompt_str), "%s@%s:C:\\%s> ",
             sys_get_username(), sys_get_hostname(), term_cwd);
    int prompt_len = strlen(prompt_str);
    int prompt_w = prompt_len * 8;
    if (prompt_w > max_w - 40) prompt_w = max_w - 40;

    gfx_draw_string_clipped(x, prompt_y, prompt_str, COLOR_GREEN, COLOR_TRANSPARENT, prompt_w);
    int input_max_w = max_w - prompt_w;
    if (input_max_w < 10) input_max_w = 10;
    gfx_draw_string_clipped(x + prompt_w, prompt_y, input_buf, COLOR_WHITE, COLOR_TRANSPARENT, input_max_w);

    // Blinking cursor
    if ((pit_get_ticks() / 30) % 2 == 0) {
        int cursor_x = x + prompt_w + (input_len * 8);
        if (cursor_x < win->x + win->width - 12) {
            gfx_fillrect(cursor_x, prompt_y, 8, 16, COLOR_ACCENT);
        }
    }
}

static void term_key(window_t *win, char key) {
    (void)win;
    if (key == '\n') {
        term_execute_command();
    } else if (key == '\b') {
        if (input_len > 0) {
            input_len--;
            input_buf[input_len] = '\0';
        }
    } else if (key >= 32 && key <= 126) {
        if (input_len < TERM_LINE_LEN - 1) {
            input_buf[input_len++] = key;
            input_buf[input_len] = '\0';
        }
    }
}

void app_term_launch(void) {
    window_t *win = wm_create_window("Terminal", 100, 80, 540, 360, RGB(17, 17, 27));
    if (!win) return;

    win->draw_client = term_draw;
    win->on_key = term_key;

    if (term_line_count == 0) {
        term_add_line("Welcome to AuraOS Terminal v1.0");
        term_add_line("Type 'help' to see available commands.");
        term_add_line("");
    }
}
