[BITS 32]
section .text
global _start
global idt_flush

extern kernel_main
extern isr_handler
extern irq_handler

_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x1FFFF0

    ; If El Torito CD-ROM boot, we were entered at 0x7E00.
    ; Check current instruction pointer
    call .get_ip
.get_ip:
    pop eax
    cmp eax, 0x10000
    jae .kernel_ready

    ; Running at 0x7E00: relocate 512 KB backwards to 0x10000
    mov esi, 0x7E00 + 524288 - 4
    mov edi, 0x10000 + 524288 - 4
    mov ecx, 131072
    std
    rep movsd
    cld

    ; Far jump to relocated kernel at 0x10000
    jmp 0x08:0x10000 + (.kernel_ready - _start)

.kernel_ready:
    push 0x7000 ; pointer to boot_info_t
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

idt_flush:
    mov eax, [esp+4]
    lidt [eax]
    ret

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push 0
    push %1
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push %1
    jmp isr_common_stub
%endmacro

%macro IRQ 2
global irq%1
irq%1:
    push 0
    push %2
    jmp irq_common_stub
%endmacro

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_ERRCODE   17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_ERRCODE   30
ISR_NOERRCODE 31

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

isr_common_stub:
    pusha
    push ds

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call isr_handler
    add esp, 4

    pop ds
    popa
    add esp, 8
    iret

irq_common_stub:
    pusha
    push ds

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call irq_handler
    add esp, 4

    pop ds
    popa
    add esp, 8
    iret
