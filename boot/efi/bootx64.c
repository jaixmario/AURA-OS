#include "efi.h"

extern const unsigned char kernel_bin_data[];
extern const unsigned int kernel_bin_size;
extern const unsigned char trampoline_bin_data[];
extern const unsigned int trampoline_bin_size;

typedef struct {
    unsigned int   magic;        // 0x41555241 ("AURA")
    unsigned int   fb_base;      // Physical Framebuffer address
    unsigned short width;        // Screen width
    unsigned short height;       // Screen height
    unsigned short pitch;        // Bytes per scanline
    unsigned char  bpp;          // Bits per pixel (32)
    unsigned char  is_live_media;// 1 = Live CD / USB
    unsigned char  boot_drive;   // Boot drive number
} __attribute__((packed)) boot_info_t;

static void *memcpy(void *dest, const void *src, UINTN n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    for (UINTN i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

static void *memset(void *s, int c, UINTN n) {
    unsigned char *p = (unsigned char *)s;
    for (UINTN i = 0; i < n; i++) {
        p[i] = (unsigned char)c;
    }
    return s;
}

EFI_STATUS EFIAPI EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS status;

    // 1. Locate Graphics Output Protocol (GOP)
    EFI_GUID gop_guid = { 0x9042a9de, 0x23dc, 0x4a38, { 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } };
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    status = SystemTable->BootServices->LocateProtocol(&gop_guid, NULL, (VOID **)&gop);

    UINT32 chosen_mode = 0;
    UINT32 best_w = 1024;
    UINT32 best_h = 768;
    UINT32 best_pitch = 1024 * 4;
    UINT64 fb_base = 0;

    if (status == EFI_SUCCESS && gop != NULL) {
        chosen_mode = gop->Mode->Mode;
        if (gop->Mode->Info) {
            best_w = gop->Mode->Info->HorizontalResolution;
            best_h = gop->Mode->Info->VerticalResolution;
            best_pitch = gop->Mode->Info->PixelsPerScanLine * 4;
            fb_base = gop->Mode->FrameBufferBase;
        }

        // Search for preferred 1024x768 resolution
        for (UINT32 m = 0; m < gop->Mode->MaxMode; m++) {
            EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = NULL;
            UINTN size_of_info = 0;
            if (gop->QueryMode(gop, m, &size_of_info, &info) == EFI_SUCCESS && info) {
                if (info->HorizontalResolution == 1024 && info->VerticalResolution == 768) {
                    chosen_mode = m;
                    best_w = 1024;
                    best_h = 768;
                    best_pitch = info->PixelsPerScanLine * 4;
                    break;
                }
            }
        }

        if (chosen_mode != gop->Mode->Mode) {
            gop->SetMode(gop, chosen_mode);
        }

        if (gop->Mode && gop->Mode->Info) {
            best_w = gop->Mode->Info->HorizontalResolution;
            best_h = gop->Mode->Info->VerticalResolution;
            best_pitch = gop->Mode->Info->PixelsPerScanLine * 4;
            fb_base = gop->Mode->FrameBufferBase;
        }
    }

    // 2. Populate boot_info_t structure at physical address 0x7000
    boot_info_t *bi = (boot_info_t *)0x7000;
    memset(bi, 0, sizeof(boot_info_t));
    bi->magic = 0x41555241; // 'AURA'
    bi->fb_base = (unsigned int)fb_base;
    bi->width = (unsigned short)best_w;
    bi->height = (unsigned short)best_h;
    bi->pitch = (unsigned short)best_pitch;
    bi->bpp = 32;
    bi->is_live_media = 1;
    bi->boot_drive = 0x80;

    // 3. Exit Boot Services
    UINT8 mmap_buffer[32768];
    UINTN mmap_size = sizeof(mmap_buffer);
    UINTN map_key = 0;
    UINTN desc_size = 0;
    UINT32 desc_ver = 0;

    status = SystemTable->BootServices->GetMemoryMap(&mmap_size, (EFI_MEMORY_DESCRIPTOR *)mmap_buffer,
                                                    &map_key, &desc_size, &desc_ver);
    if (status == EFI_SUCCESS) {
        status = SystemTable->BootServices->ExitBootServices(ImageHandle, map_key);
        if (status != EFI_SUCCESS) {
            mmap_size = sizeof(mmap_buffer);
            SystemTable->BootServices->GetMemoryMap(&mmap_size, (EFI_MEMORY_DESCRIPTOR *)mmap_buffer,
                                                   &map_key, &desc_size, &desc_ver);
            status = SystemTable->BootServices->ExitBootServices(ImageHandle, map_key);
        }
    }

    // 4. Disable interrupts
    __asm__ volatile ("cli");

    // 5. Copy Kernel to physical 0x10000
    memcpy((void *)0x10000, kernel_bin_data, kernel_bin_size);

    // 6. Copy Trampoline to physical 0x6000
    memcpy((void *)0x6000, trampoline_bin_data, trampoline_bin_size);

    // 7. Jump to Trampoline entry64 at 0x6000
    void (*jump_trampoline)(void) = (void (*)(void))0x6000;
    jump_trampoline();

    while (1) {
        __asm__ volatile ("hlt");
    }

    return EFI_SUCCESS;
}
