#ifndef DISPLAY_H
#define DISPLAY_H

typedef struct {
    int width;
    int height;
    const char *label;
    const char *aspect;
    const char *desc;
} display_mode_t;

typedef struct {
    int valid;
    int native_w;
    int native_h;
    char monitor_name[32];
    char aspect[8];
} edid_info_t;

void display_init(void);
int display_get_mode_count(void);
const display_mode_t *display_get_mode(int index);
int display_get_current_mode_index(void);
int display_set_mode_by_index(int index);
int display_set_resolution(int width, int height);
int display_is_bga_supported(void);
const char *display_get_adapter_name(void);
int display_get_edid_info(edid_info_t *out_info);
int display_auto_detect(void);

#endif
