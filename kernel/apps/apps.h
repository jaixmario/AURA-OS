#ifndef APPS_H
#define APPS_H

#include "../wm/wm.h"

void app_term_launch(void);
void app_calc_launch(void);
void app_paint_launch(void);
void app_sysinfo_launch(void);
void app_notes_launch(void);
void app_notes_load_text(const char *text);
void app_settings_launch(void);
void app_settings_open_tab(int tab);
void app_files_launch(void);

#endif
