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

#define PAE_PDPT_ADDR  0x8000ULL
#define PAE_PD0_ADDR   0x9000ULL
#define PAE_PD1_ADDR   0xA000ULL
#define PAE_PD2_ADDR   0xB000ULL
#define PAE_PD3_ADDR   0xC000ULL

static UINT32 setup_pae_paging(UINT64 fb_base, UINT32 fb_size) {
    UINT64 *pdpt = (UINT64 *)PAE_PDPT_ADDR;
    UINT64 *pd0  = (UINT64 *)PAE_PD0_ADDR;
    UINT64 *pd1  = (UINT64 *)PAE_PD1_ADDR;
    UINT64 *pd2  = (UINT64 *)PAE_PD2_ADDR;
    UINT64 *pd3  = (UINT64 *)PAE_PD3_ADDR;

    memset(pdpt, 0, 4096);
    memset(pd0, 0, 4096);
    memset(pd1, 0, 4096);
    memset(pd2, 0, 4096);
    memset(pd3, 0, 4096);

    // 1. Link PDPT to the 4 Page Directories
    pdpt[0] = PAE_PD0_ADDR | 0x01; // Present
    pdpt[1] = PAE_PD1_ADDR | 0x01;
    pdpt[2] = PAE_PD2_ADDR | 0x01;
    pdpt[3] = PAE_PD3_ADDR | 0x01;

    // 2. Identity-map 0 to 4 GB with 2 MB large pages
    for (UINT32 i = 0; i < 512; i++) {
        pd0[i] = ((UINT64)i * 0x200000ULL) | 0x83;
        pd1[i] = (((UINT64)i + 512) * 0x200000ULL) | 0x83;
        pd2[i] = (((UINT64)i + 1024) * 0x200000ULL) | 0x83;
        pd3[i] = (((UINT64)i + 1536) * 0x200000ULL) | 0x83;
    }

    // 3. If fb_base is below 4GB, identity mapping in pd0..pd3 already covers it
    if (fb_base < 0x100000000ULL) {
        return (UINT32)fb_base;
    }

    // 4. fb_base is >= 4GB (Above 4G Decoding / AMD Ryzen 64-bit BAR):
    // Map it to 32-bit virtual window 0xE0000000 (3.5 GB mark, index 256 in PD3)
    UINT64 page_offset = fb_base & 0x1FFFFFULL;
    UINT64 page_base   = fb_base & ~0x1FFFFFULL;
    UINT32 num_pages   = (fb_size + (UINT32)page_offset + 0x1FFFFF) / 0x200000;
    if (num_pages < 1) num_pages = 1;
    if (num_pages > 64) num_pages = 64; // up to 128 MB aperture

    for (UINT32 p = 0; p < num_pages; p++) {
        pd3[256 + p] = (page_base + ((UINT64)p * 0x200000ULL)) | 0x8B; // Present, RW, PWT, 2MB
    }

    return 0xE0000000 + (UINT32)page_offset;
}

EFI_STATUS EFIAPI EfiMain(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS status;

    if (SystemTable && SystemTable->ConOut) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut, L"AuraOS UEFI Bootloader starting...\r\n");
    }

    // 1. Allocate fixed physical pages for Trampoline (0x6000..0xCFFF, 7 pages) and Kernel (0x10000, 128 pages)
    EFI_PHYSICAL_ADDRESS addr_tramp = 0x6000;
    SystemTable->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, 7, &addr_tramp);

    EFI_PHYSICAL_ADDRESS addr_kernel = 0x10000;
    SystemTable->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, 128, &addr_kernel);

    // 2. Locate Graphics Output Protocol (GOP)
    EFI_GUID gop_guid = { 0x9042a9de, 0x23dc, 0x4a38, { 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a } };
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
    status = SystemTable->BootServices->LocateProtocol(&gop_guid, NULL, (VOID **)&gop);

    UINT32 best_w = 1024;
    UINT32 best_h = 768;
    UINT32 best_pitch = 1024 * 4;
    UINT64 fb_base = 0;

    if (status == EFI_SUCCESS && gop != NULL && gop->Mode != NULL) {
        if (gop->Mode->Info && gop->Mode->Info->HorizontalResolution >= 800) {
            best_w = gop->Mode->Info->HorizontalResolution;
            best_h = gop->Mode->Info->VerticalResolution;
            best_pitch = (gop->Mode->Info->PixelsPerScanLine > 0) ? (gop->Mode->Info->PixelsPerScanLine * 4) : (best_w * 4);
            fb_base = gop->Mode->FrameBufferBase;
        } else {
            UINT32 chosen_mode = gop->Mode->Mode;
            for (UINT32 m = 0; m < gop->Mode->MaxMode; m++) {
                EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info = NULL;
                UINTN size_of_info = 0;
                if (gop->QueryMode(gop, m, &size_of_info, &info) == EFI_SUCCESS && info) {
                    if (info->HorizontalResolution == 1024 && info->VerticalResolution == 768) {
                        chosen_mode = m;
                        best_w = 1024;
                        best_h = 768;
                        best_pitch = (info->PixelsPerScanLine > 0) ? (info->PixelsPerScanLine * 4) : (1024 * 4);
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
                best_pitch = (gop->Mode->Info->PixelsPerScanLine > 0) ? (gop->Mode->Info->PixelsPerScanLine * 4) : (best_w * 4);
                fb_base = gop->Mode->FrameBufferBase;
            }
        }
    }

    UINT32 fb_size = best_pitch * best_h;
    UINT32 mapped_fb = setup_pae_paging(fb_base, fb_size);

    if (fb_base >= 0x100000000ULL && SystemTable && SystemTable->ConOut) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut,
            L"[+] 64-bit Framebuffer mapped to 0xE0000000 via 32-bit PAE MMU!\r\n");
    }

    // 3. Populate boot_info_t structure at physical address 0x7000
    boot_info_t *bi = (boot_info_t *)0x7000;
    memset(bi, 0, sizeof(boot_info_t));
    bi->magic = 0x41555241; // 'AURA'
    bi->fb_base = mapped_fb;
    bi->width = (unsigned short)best_w;
    bi->height = (unsigned short)best_h;
    bi->pitch = (unsigned short)best_pitch;
    bi->bpp = 32;
    bi->is_live_media = 1;
    bi->boot_drive = 0x80;

    // 4. Exit Boot Services (with retry loop)
    UINT8 mmap_buffer[65536];
    UINTN mmap_size = sizeof(mmap_buffer);
    UINTN map_key = 0;
    UINTN desc_size = 0;
    UINT32 desc_ver = 0;

    for (int retry = 0; retry < 5; retry++) {
        mmap_size = sizeof(mmap_buffer);
        status = SystemTable->BootServices->GetMemoryMap(&mmap_size, (EFI_MEMORY_DESCRIPTOR *)mmap_buffer,
                                                        &map_key, &desc_size, &desc_ver);
        if (status == EFI_SUCCESS) {
            status = SystemTable->BootServices->ExitBootServices(ImageHandle, map_key);
            if (status == EFI_SUCCESS) {
                break;
            }
        }
    }

    // 5. Disable interrupts
    __asm__ volatile ("cli");

    // 6. Copy Kernel to physical 0x10000
    memcpy((void *)0x10000, kernel_bin_data, kernel_bin_size);

    // 7. Copy Trampoline to physical 0x6000
    memcpy((void *)0x6000, trampoline_bin_data, trampoline_bin_size);

    // 8. Jump to Trampoline entry64 at 0x6000
    void (*jump_trampoline)(void) = (void (*)(void))0x6000;
    jump_trampoline();

    while (1) {
        __asm__ volatile ("hlt");
    }

    return EFI_SUCCESS;
}
