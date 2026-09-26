[BITS 16]
[ORG 0x7C00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    ; 1. Check if booted from CD-ROM (El Torito preloaded at 0x7E00)
    cmp dword [0x7E00], 0x0010B866
    jne .read_disk

    ; CD-ROM boot: copy 32KB kernel from 0x7E00 to 0x10000
    push ds
    push es
    xor ax, ax
    mov ds, ax
    mov ax, 0x1000
    mov es, ax
    mov si, 0x7E00
    xor di, di
    mov cx, 8192 ; 32 KB in dwords
    rep movsd
    pop es
    pop ds
    jmp .kernel_ready

.read_disk:
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

.kernel_ready:

    ; 2. Query VBE mode 0x118 info into 0x8000
    mov ax, 0x4F01
    mov cx, 0x118
    mov di, 0x8000
    int 0x10
    cmp ax, 0x004F
    jne vbe_error

    ; 3. Set VBE mode 0x4118 (1024x768 with Linear Frame Buffer)
    mov ax, 0x4F02
    mov bx, 0x4118
    int 0x10
    cmp ax, 0x004F
    jne vbe_error

    ; 4. Populate Boot Info at 0x7000
    mov eax, 0x41555241 ; Magic 'AURA'
    mov [0x7000], eax
    mov eax, [0x8028]   ; PhysBasePtr (VRAM physical address)
    mov [0x7004], eax
    mov ax, [0x8012]    ; XResolution (1024)
    mov [0x7008], ax
    mov ax, [0x8014]    ; YResolution (768)
    mov [0x700A], ax
    mov ax, [0x8010]    ; BytesPerScanLine (pitch)
    mov [0x700C], ax
    mov al, [0x8019]    ; Bpp (24)
    mov [0x700E], al

    ; 5. Enable Fast A20 Gate
    cli
    in al, 0x92
    or al, 2
    out 0x92, al

    ; 6. Load Global Descriptor Table (GDT)
    lgdt [gdt_descriptor]

    ; 7. Enter 32-bit Protected Mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; 8. Far jump to 32-bit entry stub within boot sector
    jmp 0x08:pm_start

disk_error:
    mov ax, 0xB800
    mov es, ax
    mov word [es:0], 0x4F44 ; 'D' in red
.hang1:
    hlt
    jmp .hang1

vbe_error:
    mov ax, 0xB800
    mov es, ax
    mov word [es:0], 0x4F56 ; 'V' in red
.hang2:
    hlt
    jmp .hang2

[BITS 32]
pm_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; Jump to Kernel entry at 0x10000
    jmp 0x10000

align 4
dap:
    db 0x10     ; Packet size (16 bytes)
    db 0        ; Reserved
    dw 128      ; Read 128 sectors (64 KB)
    dw 0x0000   ; Destination Offset
    dw 0x1000   ; Destination Segment (0x1000:0000 = 0x10000)
    dq 1        ; Starting LBA sector 1

boot_drive: db 0

align 4
gdt_start:
    dq 0 ; Null Descriptor
gdt_code:
    dw 0xFFFF, 0x0000, 0x9A00, 0x00CF ; 32-bit Ring 0 Code, 4GB limit
gdt_data:
    dw 0xFFFF, 0x0000, 0x9200, 0x00CF ; 32-bit Ring 0 Data, 4GB limit
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; Pad exactly to offset 446 (0x1BE) for standard MBR Partition Table
times 446 - ($ - $$) db 0

; MBR Partition Table Entry 1 (offset 446 / 0x1BE)
partition1:
    db 0x80         ; 0x80 = Active / Bootable Partition
    db 0x01         ; Starting Head = 1
    db 0x01         ; Starting Sector = 1
    db 0x00         ; Starting Cylinder = 0
    db 0x06         ; Partition Type = FAT16 (or 0x0C for FAT32 LBA)
    db 0x0F         ; Ending Head = 15
    db 0x20         ; Ending Sector = 32
    db 0x27         ; Ending Cylinder = 39
    dd 2048         ; Starting LBA sector = 2048 (1 MB offset)
    dd 18432        ; Partition size = 18432 sectors (9 MB)

; MBR Partition Table Entries 2, 3, 4 (all empty)
partition2: times 16 db 0
partition3: times 16 db 0
partition4: times 16 db 0

; MBR Boot Signature at offset 510
dw 0xAA55
