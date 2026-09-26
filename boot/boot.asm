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
    mov ax, 0x1000
    mov es, ax
    mov si, 0x7E00
    xor di, di
    mov cx, 8192
    rep movsd
    xor ax, ax
    mov es, ax
    jmp .kernel_ready

.read_disk:
    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_error

.kernel_ready:
    ; 2. Dynamic VBE Mode Detection
    mov word [0x7010], 0 ; chosen_mode = 0

    mov ax, 0x4F00
    mov di, 0x8000
    mov dword [di], 'VBE2' ; Request VBE 2.0+
    int 0x10
    cmp ax, 0x004F
    jne .fallback_start

    mov si, [0x800E]
    mov ax, [0x8010]
    mov fs, ax

.scan_loop:
    mov cx, [fs:si]
    cmp cx, 0xFFFF
    je .scan_done
    add si, 2

    push si
    mov ax, 0x4F01
    mov di, 0x8200
    int 0x10
    pop si
    cmp ax, 0x004F
    jne .scan_loop

    ; ModeAttributes bit 0 (supported) and bit 7 (LFB) must be set
    mov ax, [0x8200]
    and ax, 0x0081
    cmp ax, 0x0081
    jne .scan_loop

    ; Check resolution & bpp
    mov ax, [0x8212] ; width
    mov dx, [0x8214] ; height
    mov bl, [0x8219] ; bpp

    ; Optimal: 1024x768x32
    cmp ax, 1024
    jne .check_800
    cmp dx, 768
    jne .check_800
    cmp bl, 32
    je .found_optimal
    cmp bl, 24
    jne .check_800
    mov [0x7010], cx
    jmp .scan_loop

.check_800:
    cmp word [0x7010], 0
    jne .scan_loop
    cmp ax, 800
    jne .scan_loop
    cmp dx, 600
    jne .scan_loop
    cmp bl, 32
    je .found_800
    cmp bl, 24
    jne .scan_loop
.found_800:
    mov [0x7010], cx
    jmp .scan_loop

.found_optimal:
    mov [0x7010], cx

.scan_done:
    mov ax, [0x7010]
    test ax, ax
    jz .fallback_start
    call .set_vbe
    jnc .mode_set_ok

.fallback_start:
    mov si, fallback_modes
.fallback_loop:
    lodsw
    test ax, ax
    jz vbe_error
    push si
    call .set_vbe
    pop si
    jnc .mode_set_ok
    jmp .fallback_loop

.set_vbe:
    mov cx, ax
    mov ax, 0x4F01
    mov di, 0x8200
    int 0x10
    cmp ax, 0x004F
    jne .fail
    mov bx, cx
    or bx, 0x4000 ; Enable Linear Frame Buffer
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne .fail
    clc
    ret
.fail:
    stc
    ret

.mode_set_ok:
    ; 3. Populate Boot Info at 0x7000
    mov eax, 0x41555241 ; Magic 'AURA'
    mov [0x7000], eax
    mov eax, [0x8228]   ; PhysBasePtr
    mov [0x7004], eax
    mov eax, [0x8212]   ; Width (ax) & Height (dx)
    mov [0x7008], eax
    mov ax, [0x8210]    ; Pitch
    mov [0x700C], ax
    mov al, [0x8219]    ; Bpp
    mov [0x700E], al

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
    jmp show_error

vbe_error:
    mov al, 'V'

show_error:
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
    mov ss, ax
    mov esp, 0x90000
    jmp 0x10000

dap:
    db 0x10, 0
    dw 128
    dw 0x0000, 0x1000
    dq 1

boot_drive: db 0
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
