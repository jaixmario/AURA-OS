[BITS 16]
[ORG 0x7C00]

start:
    jmp 0x0000:.init_cs
.init_cs:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [0x7010], dl         ; Save BIOS boot drive directly
    mov al, 2
    cmp dword [0x7E00], 0x0010B866
    je .is_cd
    mov al, [is_live_default]
.is_cd:
    mov [0x700F], al         ; 0=HDD, 1=Rufus USB, 2=CD-ROM
    cmp al, 2
    je .kernel_ready

.read_disk:
    mov si, dap
    mov bp, 16               ; 16 chunks of 64 sectors = 1024 sectors (512 KB)
.read_loop:
    mov ah, 0x42
    int 0x13
    jc error
    add word [si + 6], 0x0800  ; Target segment: +32KB
    add word [si + 8], 64      ; Starting LBA: +64 sectors
    dec bp
    jnz .read_loop

.kernel_ready:
    ; 2. Auto-Detect Native Monitor Resolution via VBE DDC (EDID)
    mov ax, 0x4F15
    mov bl, 0x01
    xor cx, cx
    xor dx, dx
    mov di, 0x5400
    int 0x10
    cmp ax, 0x004F
    jne .check_key
    cmp word [di + 54], 0     ; Pixel clock != 0
    jz .check_key
    ; Extract native monitor width: ((byte[58] >> 4) << 8) | byte[56]
    mov ah, [di + 58]
    shr ah, 4
    mov al, [di + 56]
    mov [target_w], ax
    ; Extract native monitor height: ((byte[61] >> 4) << 8) | byte[59]
    mov ah, [di + 61]
    shr ah, 4
    mov al, [di + 59]
    mov [target_h], ax

.check_key:
    ; 3. Optional Key '1' forces 1080p
    mov ah, 0x01
    int 0x16
    jz .check_vbe
    mov ah, 0x00
    int 0x16
    cmp al, '1'
    jne .check_vbe
    mov word [target_w], 1920
    mov word [target_h], 1080

.check_vbe:
    ; 4. Query VBE 2.0+ Controller Info
    mov ax, 0x4F00
    mov di, 0x5000
    mov dword [di], 'VBE2'
    int 0x10
    cmp ax, 0x004F
    jne error

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

    mov al, [0x5200]
    and al, 0x81
    cmp al, 0x81
    jne .scan_loop

    ; Color depth must be 24 or 32 bpp
    cmp byte [0x5219], 24
    jb .scan_loop

    mov ax, [0x5212] ; Width
    mov dx, [0x5214] ; Height

    ; Check if matches target resolution
    cmp ax, [target_w]
    jne .save_fallback
    cmp dx, [target_h]
    jne .save_fallback
    mov [selected_mode], cx
    jmp .scan_done

.save_fallback:
    cmp word [fallback_mode], 0
    jnz .scan_loop
    mov [fallback_mode], cx
    jmp .scan_loop

.scan_done:
    mov bx, [selected_mode]
    test bx, bx
    jnz .set_mode
    mov bx, [fallback_mode]
    test bx, bx
    jz error

.set_mode:
    ; Read mode info block into 0x5200
    mov cx, bx
    mov ax, 0x4F01
    mov di, 0x5200
    int 0x10

    or bx, 0x4000            ; Enable Linear Frame Buffer
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne error

    ; 5. Populate Boot Info Structure at 0x7000
    mov di, 0x7000
    mov dword [di], 0x41555241     ; Magic 'AURA'
    mov eax, [0x5228]              ; PhysBasePtr (LFB Address)
    mov [di + 4], eax
    mov eax, [0x5212]              ; Width & Height
    mov [di + 8], eax
    mov ax, [0x5210]               ; Pitch
    mov [di + 12], ax
    mov al, [0x5219]               ; Bpp
    mov [di + 14], al

    ; 6. Fast A20 Gate
    cli
    in al, 0x92
    or al, 2
    and al, 0xFE
    out 0x92, al

    ; 7. Load GDT and Enter 32-bit Protected Mode
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to 32-bit entry (kernel relocated in k_entry if CD boot)
    cmp byte [0x700F], 2
    je .pm_cd
    jmp dword 0x08:0x10000
.pm_cd:
    jmp dword 0x08:0x7E00

error:
    mov ah, 0x0E
    mov al, 'E'
    int 0x10
.hang:
    hlt
    jmp .hang

is_live_default: db 0
target_w:      dw 1024
target_h:      dw 768
selected_mode: dw 0
fallback_mode: dw 0

dap:
    db 0x10, 0
    dw 64                      ; 64 sectors = 32 KB per chunk
    dw 0x0000, 0x1000          ; Target: 0x1000:0x0000 -> 0x10000 physical
    dq 1                       ; Starting LBA sector (patched for ISO hybrid MBR)

gdt_start:
    dq 0
    dw 0xFFFF, 0x0000, 0x9A00, 0x00CF
    dw 0xFFFF, 0x0000, 0x9200, 0x00CF
gdt_end:

gdt_descriptor:
    dw 23
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
