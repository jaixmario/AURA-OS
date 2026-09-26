#include "apps.h"
#include "../gfx/gfx.h"
#include "../arch/pit.h"
#include "../arch/rtc.h"
#include "../fs/vfs.h"
#include "../arch/io.h"
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
        term_add_line("Memory Architecture Layout:");
        term_add_line("  Kernel Code : 0x00010000 (Flat Model)");
        term_add_line("  Kernel Stack: 0x001FFFF0 (1MB Ring 0 Stack)");
        term_add_line("  VRAM Buffer : 0x00200000 (3MB Double Buffer)");
        term_add_line("  Paint Canvas: 0x00600000 (Extended RAM)");
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
        term_add_line("AuraOS v1.1 [i686 Protected Mode]");
        term_add_line("Video: 1024x768 Dynamic VBE TrueColor");
        term_add_line("Drivers: PS/2 Mouse & Keyboard, PIT 100Hz");
    } else if (strcmp(input_buf, "clear") == 0 || strcmp(input_buf, "cls") == 0) {
        term_line_count = 0;
    } else if (strcmp(input_buf, "uptime") == 0) {
        unsigned int sec = pit_get_uptime_seconds();
        char msg[48];
        snprintf(msg, sizeof(msg), "System Uptime: %u seconds (%u ticks)", sec, pit_get_ticks());
        term_add_line(msg);
    } else if (strncmp(input_buf, "echo ", 5) == 0) {
        term_add_line(input_buf + 5);
    } else if (strcmp(input_buf, "about") == 0) {
        term_add_line("AuraOS - Built from scratch with C and Assembly");
        term_add_line("Custom Bare-Metal Graphical Operating System");
    } else if (strcmp(input_buf, "reboot") == 0) {
        term_add_line("Rebooting system...");
        outb(0x64, 0xFE); // Pulse reset line via keyboard controller
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

    // Draw previous lines
    for (int i = 0; i < term_line_count; i++) {
        gfx_draw_string(x, start_y + (i * 18), term_lines[i], COLOR_TEXT, COLOR_TRANSPARENT);
    }

    // Draw active prompt
    int prompt_y = start_y + (term_line_count * 18);
    char prompt_str[64];
    snprintf(prompt_str, sizeof(prompt_str), "aura@C:\\%s> ", term_cwd);
    int prompt_len = strlen(prompt_str);
    gfx_draw_string(x, prompt_y, prompt_str, COLOR_GREEN, COLOR_TRANSPARENT);
    gfx_draw_string(x + (prompt_len * 8), prompt_y, input_buf, COLOR_WHITE, COLOR_TRANSPARENT);

    // Blinking cursor
    if ((pit_get_ticks() / 30) % 2 == 0) {
        int cursor_x = x + (prompt_len * 8) + (input_len * 8);
        gfx_fillrect(cursor_x, prompt_y, 8, 16, COLOR_ACCENT);
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
