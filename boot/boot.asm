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
    mov [is_live_boot], al

    ; 1. Check if booted from CD-ROM (El Torito preloaded at 0x7E00)
    cmp dword [0x7E00], 0x0010B866
    jne .read_disk
    inc byte [is_live_boot]
    jmp .kernel_ready

.read_disk:
    xor ax, ax                 ; Reset disk subsystem
    int 0x13

    mov si, dap
    mov bp, 10                 ; 10 chunks of 64 sectors = 640 sectors (320 KB)
.read_loop:
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_error
    add word [si + 6], 0x0800  ; Next segment: +32KB (0x1000 -> 0x1800 -> 0x2000 ...)
    add word [si + 8], 64      ; Next LBA sector: +64
    dec bp
    jnz .read_loop

.kernel_ready:
    ; 2. Dynamic VBE Mode Detection
    mov word [0x7010], 0

    mov ax, 0x4F00
    mov di, 0x5000
    mov dword [di], 'VBE2' ; Request VBE 2.0+
    int 0x10
    cmp ax, 0x004F
    jne .fallback_start

    mov si, [0x500E]
    mov ax, [0x5010]
    mov fs, ax

.scan_loop:
    mov cx, [fs:si]
    cmp cx, 0xFFFF
    je .scan_done
    add si, 2

    push si
    mov ax, 0x4F01
    mov di, 0x5200
    int 0x10
    pop si
    cmp ax, 0x004F
    jne .scan_loop

    ; ModeAttributes bit 0 (supported) and bit 7 (LFB) must be set
    mov ax, [0x5200]
    and ax, 0x0081
    cmp ax, 0x0081
    jne .scan_loop

    ; Check resolution & bpp
    cmp byte [0x5219], 24 ; Bpp must be 24 or 32
    jb .scan_loop
    mov ax, [0x5212]     ; Width
    mov dx, [0x5214]     ; Height

    ; Check for 1024x768
    cmp ax, 1024
    jne .save_fallback
    cmp dx, 768
    jne .save_fallback
    mov [0x7010], cx
    jmp .scan_done

.save_fallback:
    cmp word [0x7010], 0
    jnz .scan_loop
    mov [0x7010], cx
    jmp .scan_loop

.scan_done:
    mov ax, [0x7010]
    test ax, ax
    jz .fallback_start
    call .set_vbe
    jz .mode_set_ok

.fallback_start:
    mov si, fallback_modes
.fallback_loop:
    lodsw
    test ax, ax
    jz vbe_error
    push si
    call .set_vbe
    pop si
    jz .mode_set_ok
    jmp .fallback_loop

.set_vbe:
    mov cx, ax
    mov ax, 0x4F01
    mov di, 0x5200
    int 0x10
    cmp ax, 0x004F
    jne .fail
    mov bx, cx
    or bx, 0x4000 ; Enable Linear Frame Buffer
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
.fail:
    ret

.mode_set_ok:
    ; 3. Populate Boot Info at 0x7000
    mov eax, 0x41555241 ; Magic 'AURA'
    mov [0x7000], eax
    mov eax, [0x5228]   ; PhysBasePtr
    mov [0x7004], eax
    mov eax, [0x5212]   ; Width (ax) & Height (dx)
    mov [0x7008], eax
    mov ax, [0x5210]    ; Pitch
    mov [0x700C], ax
    mov al, [0x5219]    ; Bpp
    mov [0x700E], al
    mov ax, [is_live_boot]
    mov [0x700F], ax

    ; 4. Fast A20 Gate
    cli
    in al, 0x92
    or al, 2
    out 0x92, al

    ; 5. GDT & PM
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pm_start

disk_error:
    mov al, 'D'
    db 0x3C ; cmp al, <next byte> skips mov al, 'V'
vbe_error:
    mov al, 'V'
    mov ah, 0x4F
    push 0xB800
    pop es
    mov [es:0], ax
.hang:
    hlt
    jmp .hang

[BITS 32]
pm_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x1FFFF0

    ; If CD-ROM boot, copy 320KB kernel backwards from 0x7E00 to 0x10000
    cmp byte [0x700F], 1
    jne .jump_kernel
    mov esi, 0x7E00 + 327680 - 4
    mov edi, 0x10000 + 327680 - 4
    mov ecx, 81920
    std
    rep movsd
    cld
.jump_kernel:
    jmp 0x10000

    db 0                       ; 1-byte padding to align dap on 4-byte boundary
dap:
    db 0x10, 0
    dw 64                      ; 64 sectors = 32 KB per chunk (safe <= 127 limit)
    dw 0x0000, 0x1000          ; Target: 0x1000:0x0000 -> 0x10000 physical
    dq 1

is_live_boot: db 0
boot_drive:   db 0
fallback_modes: dw 0x0144, 0x0118, 0x0115, 0

gdt_start:
    dq 0
gdt_code:
    dw 0xFFFF, 0x0000, 0x9A00, 0x00CF
gdt_data:
    dw 0xFFFF, 0x0000, 0x9200, 0x00CF
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

times 446 - ($ - $$) db 0

partition1:
    db 0x80         ; 0x80 = Active / Bootable Partition
    db 0x01         ; Starting Head = 1
    db 0x01         ; Starting Sector = 1
    db 0x00         ; Starting Cylinder = 0
    db 0x06         ; Partition Type = FAT16
    db 0x0F         ; Ending Head = 15
    db 0x20         ; Ending Sector = 32
    db 0x27         ; Ending Cylinder = 39
    dd 2048         ; Starting LBA sector = 2048 (1 MB offset)
    dd 18432        ; Partition size = 18432 sectors (9 MB)

partition2: times 16 db 0
partition3: times 16 db 0
partition4: times 16 db 0

dw 0xAA55
