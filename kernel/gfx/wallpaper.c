#include "wallpaper.h"
#include "wallpaper_data.h"
#include "picojpeg.h"
#include "gfx.h"
#include "../libc/string.h"

// Dedicated safe buffer in low extended memory:
// 0x00C00000 (12MB mark): 1024x768 decoded wallpaper source (3.14MB, ends at 15.14MB)
// This strictly avoids all overlap with the 1080p backbuffer (0x00200000 - 0x00A70000)
// and is fully documented in System Info (app_term.c).
static unsigned int *g_raw_1024 = (unsigned int *)0x00C00000;

static int g_current_wallpaper = 0;
static int g_loaded_wallpaper = -1;

typedef struct {
    const unsigned char *data;
    unsigned int size;
    unsigned int offset;
} jpeg_mem_stream_t;

static unsigned char pjpeg_need_bytes(unsigned char *pBuf, unsigned char buf_size, unsigned char *pBytes_actually_read, void *pCallback_data) {
    jpeg_mem_stream_t *stream = (jpeg_mem_stream_t *)pCallback_data;
    unsigned int left = stream->size - stream->offset;
    unsigned int to_read = (buf_size < left) ? buf_size : left;
    if (to_read > 0) {
        memcpy(pBuf, stream->data + stream->offset, to_read);
        stream->offset += to_read;
    }
    *pBytes_actually_read = (unsigned char)to_read;
    return 0;
}

static void wallpaper_decode_current(void) {
    if (g_current_wallpaper < 0 || g_current_wallpaper >= WALLPAPER_COUNT) {
        g_current_wallpaper = 0;
    }

    if (g_loaded_wallpaper == g_current_wallpaper) {
        return;
    }

    const wallpaper_info_t *info_wp = &g_wallpapers[g_current_wallpaper];
    jpeg_mem_stream_t stream = { info_wp->data, info_wp->size, 0 };
    pjpeg_image_info_t info;
    unsigned char status = pjpeg_decode_init(&info, pjpeg_need_bytes, &stream, 0);
    if (status != 0) {
        // Fallback: solid color
        for (int i = 0; i < 1024 * 768; i++) {
            g_raw_1024[i] = RGB(24, 25, 38);
        }
        g_loaded_wallpaper = g_current_wallpaper;
        return;
    }

    int scale_2x = (info.m_width == 512 && info.m_height == 384);
    int mcus_x = info.m_MCUSPerRow;
    int mcus_y = info.m_MCUSPerCol;

    for (int my = 0; my < mcus_y; my++) {
        for (int mx = 0; mx < mcus_x; mx++) {
            status = pjpeg_decode_mcu();
            if (status != 0 && status != PJPG_NO_MORE_BLOCKS) {
                break;
            }

            if (info.m_scanType == PJPG_YH2V2) {
                for (int by = 0; by < 2; by++) {
                    for (int bx = 0; bx < 2; bx++) {
                        int block_idx = by * 2 + bx;
                        int src_offset = block_idx * 64;
                        for (int py = 0; py < 8; py++) {
                            int img_y = my * 16 + by * 8 + py;
                            for (int px = 0; px < 8; px++) {
                                int img_x = mx * 16 + bx * 8 + px;
                                int p_idx = src_offset + py * 8 + px;
                                unsigned char r = info.m_pMCUBufR[p_idx];
                                unsigned char g = info.m_pMCUBufG[p_idx];
                                unsigned char b = info.m_pMCUBufB[p_idx];
                                unsigned int col = (r << 16) | (g << 8) | b;

                                if (scale_2x) {
                                    int dx = img_x * 2;
                                    int dy = img_y * 2;
                                    if (dx + 1 < 1024 && dy + 1 < 768) {
                                        g_raw_1024[dy * 1024 + dx] = col;
                                        g_raw_1024[dy * 1024 + dx + 1] = col;
                                        g_raw_1024[(dy + 1) * 1024 + dx] = col;
                                        g_raw_1024[(dy + 1) * 1024 + dx + 1] = col;
                                    }
                                } else {
                                    if (img_x < 1024 && img_y < 768) {
                                        g_raw_1024[img_y * 1024 + img_x] = col;
                                    }
                                }
                            }
                        }
                    }
                }
            } else if (info.m_scanType == PJPG_YH1V1) {
                for (int py = 0; py < 8; py++) {
                    int img_y = my * 8 + py;
                    for (int px = 0; px < 8; px++) {
                        int img_x = mx * 8 + px;
                        int p_idx = py * 8 + px;
                        unsigned char r = info.m_pMCUBufR[p_idx];
                        unsigned char g = info.m_pMCUBufG[p_idx];
                        unsigned char b = info.m_pMCUBufB[p_idx];
                        unsigned int col = (r << 16) | (g << 8) | b;

                        if (scale_2x) {
                            int dx = img_x * 2;
                            int dy = img_y * 2;
                            if (dx + 1 < 1024 && dy + 1 < 768) {
                                g_raw_1024[dy * 1024 + dx] = col;
                                g_raw_1024[dy * 1024 + dx + 1] = col;
                                g_raw_1024[(dy + 1) * 1024 + dx] = col;
                                g_raw_1024[(dy + 1) * 1024 + dx + 1] = col;
                            }
                        } else {
                            if (img_x < 1024 && img_y < 768) {
                                g_raw_1024[img_y * 1024 + img_x] = col;
                            }
                        }
                    }
                }
            }
        }
    }

    g_loaded_wallpaper = g_current_wallpaper;
}

void wallpaper_init(void) {
    g_loaded_wallpaper = -1;
    // Pre-initialize g_raw_1024 with a smooth deep twilight gradient
    for (int y = 0; y < 768; y++) {
        int r, g, b;
        if (y < 384) {
            r = 18 + (35 - 18) * y / 384;
            g = 24 + (55 - 24) * y / 384;
            b = 40 + (85 - 40) * y / 384;
        } else {
            int dy = y - 384;
            r = 35 + (20 - 35) * dy / 384;
            g = 55 + (26 - 55) * dy / 384;
            b = 85 + (42 - 85) * dy / 384;
        }
        unsigned int col = (r << 16) | (g << 8) | b;
        for (int x = 0; x < 1024; x++) {
            g_raw_1024[y * 1024 + x] = col;
        }
    }
    // Pre-decode default wallpaper during initialization so desktop opens instantly
    wallpaper_decode_current();
}

void wallpaper_invalidate(void) {
    g_loaded_wallpaper = -1;
}

int wallpaper_get_count(void) {
    return WALLPAPER_COUNT;
}

const char *wallpaper_get_name(int id) {
    if (id < 0 || id >= WALLPAPER_COUNT) return "Default";
    return g_wallpapers[id].name;
}

const char *wallpaper_get_desc(int id) {
    if (id < 0 || id >= WALLPAPER_COUNT) return "";
    return g_wallpapers[id].desc;
}

unsigned int wallpaper_get_color(int id) {
    if (id < 0 || id >= WALLPAPER_COUNT) return RGB(24, 25, 38);
    return g_wallpapers[id].swatch_color;
}

int wallpaper_get_current(void) {
    return g_current_wallpaper;
}

void wallpaper_set(int id) {
    if (id < 0 || id >= WALLPAPER_COUNT) return;
    g_current_wallpaper = id;
    wallpaper_decode_current();
}

void wallpaper_draw_desktop(void) {
    int screen_w = gfx_get_width();
    int screen_h = gfx_get_height();

    if (g_loaded_wallpaper != g_current_wallpaper) {
        wallpaper_decode_current();
    }

    unsigned int *dst = gfx_get_backbuffer();
    if (screen_w == 1024 && screen_h == 768) {
        int total = 1024 * 768;
        const unsigned int *src = g_raw_1024;
        __asm__ volatile (
            "cld\n"
            "rep movsl\n"
            : "+D"(dst), "+S"(src), "+c"(total)
            :
            : "memory"
        );
    } else {
        for (int y = 0; y < screen_h; y++) {
            int src_y = (y * 768) / screen_h;
            unsigned int *dst_row = dst + (y * screen_w);
            const unsigned int *src_row = g_raw_1024 + (src_y * 1024);
            for (int x = 0; x < screen_w; x++) {
                int src_x = (x * 1024) / screen_w;
                dst_row[x] = src_row[src_x];
            }
        }
    }
}

void wallpaper_draw_tinted(void) {
    int screen_w = gfx_get_width();
    int screen_h = gfx_get_height();

    if (g_loaded_wallpaper != g_current_wallpaper) {
        wallpaper_decode_current();
    }

    unsigned int *dst = gfx_get_backbuffer();
    for (int y = 0; y < screen_h; y++) {
        int src_y = (y * 768) / screen_h;
        unsigned int *dst_row = dst + (y * screen_w);
        const unsigned int *src_row = g_raw_1024 + (src_y * 1024);
        for (int x = 0; x < screen_w; x++) {
            int src_x = (x * 1024) / screen_w;
            unsigned int c = src_row[src_x];
            unsigned int r = ((c >> 16) & 0xFF) * 40 / 100;
            unsigned int g = ((c >> 8) & 0xFF) * 40 / 100;
            unsigned int b = (c & 0xFF) * 50 / 100;
            dst_row[x] = (r << 16) | (g << 8) | b;
        }
    }
}
