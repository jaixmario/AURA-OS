#ifndef WALLPAPER_H
#define WALLPAPER_H

void wallpaper_init(void);
int  wallpaper_get_count(void);
const char *wallpaper_get_name(int id);
const char *wallpaper_get_desc(int id);
unsigned int wallpaper_get_color(int id);
int  wallpaper_get_current(void);
void wallpaper_set(int id);
void wallpaper_draw_desktop(void);
void wallpaper_draw_tinted(void);

#endif // WALLPAPER_H
