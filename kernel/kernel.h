#ifndef KERNEL_H
#define KERNEL_H

typedef struct {
    unsigned int   magic;        // 0x41555241 ("AURA")
    unsigned int   fb_base;      // Physical Framebuffer address
    unsigned short width;        // Screen width (1024)
    unsigned short height;       // Screen height (768)
    unsigned short pitch;        // Bytes per scanline
    unsigned char  bpp;          // Bits per pixel (24 or 32)
} __attribute__((packed)) boot_info_t;

void kernel_main(boot_info_t *bi);
void set_desktop_theme(int theme);
int  get_desktop_theme(void);
boot_info_t *get_boot_info(void);

#endif
