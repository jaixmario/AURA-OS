#include "apps.h"
#include "../gfx/gfx.h"
#include "../arch/pit.h"
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

static void term_execute_command(void) {
    if (input_len == 0) return;

    // Echo command
    char echo[TERM_LINE_LEN + 16];
    snprintf(echo, sizeof(echo), "> %s", input_buf);
    term_add_line(echo);

    if (strcmp(input_buf, "help") == 0) {
        term_add_line("AuraOS Terminal Commands:");
        term_add_line("  help     - show this command manual");
        term_add_line("  paint    - open Canvas Paint app");
        term_add_line("  notes    - open Notes Editor app");
        term_add_line("  calc     - open Calculator app");
        term_add_line("  settings - open Settings Control Panel");
        term_add_line("  sysinfo  - open System Specs app");
        term_add_line("  mem      - inspect memory regions");
        term_add_line("  uptime   - show system uptime");
        term_add_line("  clear/cls- clear terminal screen");
        term_add_line("  echo <t> - print message");
        term_add_line("  reboot   - restart virtual machine");
    } else if (strcmp(input_buf, "paint") == 0) {
        app_paint_launch();
        term_add_line("Launched Canvas Paint.");
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
        term_add_line("AuraOS Version 1.1.0 [i686 Protected Mode]");
    } else if (strcmp(input_buf, "date") == 0) {
        term_add_line("System Date: 2026-09-26  (RTC/PIT Synchronized)");
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
    gfx_draw_string(x, prompt_y, "aura@kernel:~$ ", COLOR_GREEN, COLOR_TRANSPARENT);
    gfx_draw_string(x + 120, prompt_y, input_buf, COLOR_WHITE, COLOR_TRANSPARENT);

    // Blinking cursor
    if ((pit_get_ticks() / 30) % 2 == 0) {
        int cursor_x = x + 120 + (input_len * 8);
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
