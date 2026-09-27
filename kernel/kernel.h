#ifndef KERNEL_H
#define KERNEL_H

typedef struct {
    unsigned int   magic;        // 0x41555241 ("AURA")
    unsigned int   fb_base;      // Physical Framebuffer address
    unsigned short width;        // Screen width (1024)
    unsigned short height;       // Screen height (768)
    unsigned short pitch;        // Bytes per scanline
    unsigned char  bpp;          // Bits per pixel (24 or 32)
    unsigned char  is_live_media;// 1 = Booted from Live CD / ISO, 0 = Hard Disk
    unsigned char  boot_drive;   // BIOS boot drive number
} __attribute__((packed)) boot_info_t;

void kernel_main(boot_info_t *bi);
void set_desktop_theme(int theme);
int  get_desktop_theme(void);
boot_info_t *get_boot_info(void);

// User & Installation state
int  sys_is_installed(void);
void sys_set_installed(int installed);
const char *sys_get_fullname(void);
const char *sys_get_username(void);
const char *sys_get_hostname(void);
void sys_set_user_info(const char *fullname, const char *username, const char *hostname, const char *password);

// Login & Lock screen state
int  sys_is_logged_in(void);
void sys_set_logged_in(int logged_in);
int  sys_verify_password(const char *pw);
void sys_lock_screen(void);

// Theme helpers
int  get_theme_count(void);
const char *get_theme_name(int theme);
const char *get_theme_desc(int theme);

// Persistent System Configuration
void sys_set_setting(const char *key, const char *value);
void sys_set_setting_int(const char *key, int value);
int  sys_get_setting_int(const char *key, int default_val);
const char *sys_get_setting(const char *key, char *out_buf, int max_len);

#endif
