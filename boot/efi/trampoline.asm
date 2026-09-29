[BITS 64]
[ORG 0x6000]

jmp_entry64:
    jmp entry64
    times 16 - ($ - $$) db 0

[BITS 32]
jmp_entry32:
    jmp entry32
    times 32 - ($ - $$) db 0

[BITS 64]
entry64:
    lgdt [gdt_descriptor_64]

    ; Far return to 32-bit compatibility mode
    push 0x08
    push compat_mode
    o64 retf

[BITS 32]
compat_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Disable Paging
    mov eax, cr0
    btr eax, 31
    mov cr0, eax
    jmp unpaged_mode

unpaged_mode:
    ; Disable Long Mode in IA32_EFER MSR (0xC0000080)
    mov ecx, 0xC0000080
    rdmsr
    btr eax, 8
    wrmsr

    ; Disable PAE
    mov eax, cr4
    btr eax, 5
    mov cr4, eax

    ; Reload GDT in pure 32-bit protected mode
    lgdt [gdt_descriptor_32]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Disable Local APIC (MSR 0x1B, bit 11) to route all interrupts to 8259 PIC
    mov ecx, 0x1B
    rdmsr
    btr eax, 11
    wrmsr

    mov esp, 0x1FFFF0
    push 0x7000
    jmp 0x08:0x10000

entry32:
    lgdt [gdt_descriptor_32]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Disable Paging
    mov eax, cr0
    btr eax, 31
    mov cr0, eax
    jmp unpaged_32

unpaged_32:
    ; Disable PAE
    mov eax, cr4
    btr eax, 5
    mov cr4, eax

    lgdt [gdt_descriptor_32]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Disable Local APIC (MSR 0x1B, bit 11) to route all interrupts to 8259 PIC
    mov ecx, 0x1B
    rdmsr
    btr eax, 11
    wrmsr

    mov esp, 0x1FFFF0
    push 0x7000
    jmp 0x08:0x10000

align 16
gdt_start:
    dq 0                        ; Null descriptor
    dq 0x00CF9A000000FFFF       ; 0x08: 32-bit Code Segment (Base=0, Limit=4GB, RX)
    dq 0x00CF92000000FFFF       ; 0x10: 32-bit Data Segment (Base=0, Limit=4GB, RW)
gdt_end:

align 8
gdt_descriptor_64:
    dw (gdt_end - gdt_start - 1)
    dq gdt_start

gdt_descriptor_32:
    dw (gdt_end - gdt_start - 1)
    dd gdt_start
