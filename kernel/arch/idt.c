#include "idt.h"
#include "pic.h"
#include "../libc/string.h"
#include "../gfx/gfx.h"

static idt_entry_t idt_entries[256];
static idt_ptr_t   idt_ptr;
static isr_handler_t interrupt_handlers[256];

extern void idt_flush(unsigned int);

void idt_set_gate(unsigned char num, unsigned int base, unsigned short sel, unsigned char flags) {
    idt_entries[num].offset_low = base & 0xFFFF;
    idt_entries[num].offset_high = (base >> 16) & 0xFFFF;
    idt_entries[num].selector = sel;
    idt_entries[num].zero = 0;
    idt_entries[num].type_attr = flags;
}

void register_interrupt_handler(unsigned char n, isr_handler_t handler) {
    interrupt_handlers[n] = handler;
}

static const char * const exception_names[32] = {
    "Divide-by-zero (#DE)", "Debug (#DB)", "NMI Interrupt", "Breakpoint (#BP)",
    "Overflow (#OF)", "BOUND Range (#BR)", "Invalid Opcode (#UD)", "Device Not Available (#NM)",
    "Double Fault (#DF)", "Coprocessor Segment Overrun", "Invalid TSS (#TS)", "Segment Not Present (#NP)",
    "Stack Fault (#SS)", "General Protection Fault (#GP)", "Page Fault (#PF)", "Reserved",
    "x87 FPU Error (#MF)", "Alignment Check (#AC)", "Machine Check (#MC)", "SIMD Exception (#XM)",
    "Virtualization Exception", "Control Protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection", "VMM Communication", "Security Exception", "Reserved"
};

void isr_handler(registers_t *regs) {
    if (regs->int_no < 32) {
        // Unhandled CPU Exception / Panic!
        __asm__ volatile ("cli");

        unsigned int cr2 = 0;
        if (regs->int_no == 14) {
            __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        }

        // Draw Panic Dialog Box on Framebuffer
        gfx_fillrect(40, 40, 680, 240, RGB(180, 20, 30));
        gfx_drawrect(40, 40, 680, 240, COLOR_WHITE);
        gfx_draw_string(60, 55, "=== AURA OS KERNEL PANIC ===", COLOR_WHITE, COLOR_TRANSPARENT);

        char buf[128];
        snprintf(buf, sizeof(buf), "Exception %d: %s", regs->int_no,
                 (regs->int_no < 32) ? exception_names[regs->int_no] : "Unknown");
        gfx_draw_string(60, 85, buf, COLOR_WHITE, COLOR_TRANSPARENT);

        snprintf(buf, sizeof(buf), "EIP: 0x%08X   CS: 0x%04X   EFLAGS: 0x%08X", regs->eip, regs->cs, regs->eflags);
        gfx_draw_string(60, 115, buf, COLOR_YELLOW, COLOR_TRANSPARENT);

        snprintf(buf, sizeof(buf), "Error Code: 0x%08X   CR2 (Faulting Addr): 0x%08X", regs->err_code, cr2);
        gfx_draw_string(60, 145, buf, COLOR_YELLOW, COLOR_TRANSPARENT);

        snprintf(buf, sizeof(buf), "EAX: 0x%08X  EBX: 0x%08X  ECX: 0x%08X  EDX: 0x%08X",
                 regs->eax, regs->ebx, regs->ecx, regs->edx);
        gfx_draw_string(60, 175, buf, COLOR_WHITE, COLOR_TRANSPARENT);

        snprintf(buf, sizeof(buf), "ESP: 0x%08X  EBP: 0x%08X  ESI: 0x%08X  EDI: 0x%08X",
                 regs->esp, regs->ebp, regs->esi, regs->edi);
        gfx_draw_string(60, 200, buf, COLOR_WHITE, COLOR_TRANSPARENT);

        gfx_draw_string(60, 230, "System halted. Please take a photo of this screen.", RGB(220, 230, 255), COLOR_TRANSPARENT);

        gfx_swap_buffers();
        while (1) {
            __asm__ volatile ("cli; hlt");
        }
    }

    if (interrupt_handlers[regs->int_no] != 0) {
        isr_handler_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    }
}

void irq_handler(registers_t *regs) {
    if (interrupt_handlers[regs->int_no] != 0) {
        isr_handler_t handler = interrupt_handlers[regs->int_no];
        handler(regs);
    }
    // EOI
    pic_send_eoi(regs->int_no - 32);
}

// Stubs declared in k_entry.asm
extern void isr0(); extern void isr1(); extern void isr2(); extern void isr3();
extern void isr4(); extern void isr5(); extern void isr6(); extern void isr7();
extern void isr8(); extern void isr9(); extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();
extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19();
extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23();
extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27();
extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();

extern void irq0(); extern void irq1(); extern void irq2(); extern void irq3();
extern void irq4(); extern void irq5(); extern void irq6(); extern void irq7();
extern void irq8(); extern void irq9(); extern void irq10(); extern void irq11();
extern void irq12(); extern void irq13(); extern void irq14(); extern void irq15();
extern void isr_default();

void idt_init(void) {
    idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
    idt_ptr.base  = (unsigned int)&idt_entries;

    memset(&idt_entries, 0, sizeof(idt_entry_t) * 256);
    memset(&interrupt_handlers, 0, sizeof(isr_handler_t) * 256);

    // Initialize PIC
    pic_init();

    // Set all 256 IDT gates to default stub first (guarantees no unhandled interrupts cause #GP or triple faults)
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, (unsigned int)isr_default, 0x08, 0x8E);
    }

    // CPU Exceptions (0..31)
    idt_set_gate(0,  (unsigned int)isr0,  0x08, 0x8E);
    idt_set_gate(1,  (unsigned int)isr1,  0x08, 0x8E);
    idt_set_gate(2,  (unsigned int)isr2,  0x08, 0x8E);
    idt_set_gate(3,  (unsigned int)isr3,  0x08, 0x8E);
    idt_set_gate(4,  (unsigned int)isr4,  0x08, 0x8E);
    idt_set_gate(5,  (unsigned int)isr5,  0x08, 0x8E);
    idt_set_gate(6,  (unsigned int)isr6,  0x08, 0x8E);
    idt_set_gate(7,  (unsigned int)isr7,  0x08, 0x8E);
    idt_set_gate(8,  (unsigned int)isr8,  0x08, 0x8E);
    idt_set_gate(9,  (unsigned int)isr9,  0x08, 0x8E);
    idt_set_gate(10, (unsigned int)isr10, 0x08, 0x8E);
    idt_set_gate(11, (unsigned int)isr11, 0x08, 0x8E);
    idt_set_gate(12, (unsigned int)isr12, 0x08, 0x8E);
    idt_set_gate(13, (unsigned int)isr13, 0x08, 0x8E);
    idt_set_gate(14, (unsigned int)isr14, 0x08, 0x8E);
    idt_set_gate(15, (unsigned int)isr15, 0x08, 0x8E);
    idt_set_gate(16, (unsigned int)isr16, 0x08, 0x8E);
    idt_set_gate(17, (unsigned int)isr17, 0x08, 0x8E);
    idt_set_gate(18, (unsigned int)isr18, 0x08, 0x8E);
    idt_set_gate(19, (unsigned int)isr19, 0x08, 0x8E);
    idt_set_gate(20, (unsigned int)isr20, 0x08, 0x8E);
    idt_set_gate(21, (unsigned int)isr21, 0x08, 0x8E);
    idt_set_gate(22, (unsigned int)isr22, 0x08, 0x8E);
    idt_set_gate(23, (unsigned int)isr23, 0x08, 0x8E);
    idt_set_gate(24, (unsigned int)isr24, 0x08, 0x8E);
    idt_set_gate(25, (unsigned int)isr25, 0x08, 0x8E);
    idt_set_gate(26, (unsigned int)isr26, 0x08, 0x8E);
    idt_set_gate(27, (unsigned int)isr27, 0x08, 0x8E);
    idt_set_gate(28, (unsigned int)isr28, 0x08, 0x8E);
    idt_set_gate(29, (unsigned int)isr29, 0x08, 0x8E);
    idt_set_gate(30, (unsigned int)isr30, 0x08, 0x8E);
    idt_set_gate(31, (unsigned int)isr31, 0x08, 0x8E);

    // PIC IRQs (32..47)
    idt_set_gate(32, (unsigned int)irq0,  0x08, 0x8E);
    idt_set_gate(33, (unsigned int)irq1,  0x08, 0x8E);
    idt_set_gate(34, (unsigned int)irq2,  0x08, 0x8E);
    idt_set_gate(35, (unsigned int)irq3,  0x08, 0x8E);
    idt_set_gate(36, (unsigned int)irq4,  0x08, 0x8E);
    idt_set_gate(37, (unsigned int)irq5,  0x08, 0x8E);
    idt_set_gate(38, (unsigned int)irq6,  0x08, 0x8E);
    idt_set_gate(39, (unsigned int)irq7,  0x08, 0x8E);
    idt_set_gate(40, (unsigned int)irq8,  0x08, 0x8E);
    idt_set_gate(41, (unsigned int)irq9,  0x08, 0x8E);
    idt_set_gate(42, (unsigned int)irq10, 0x08, 0x8E);
    idt_set_gate(43, (unsigned int)irq11, 0x08, 0x8E);
    idt_set_gate(44, (unsigned int)irq12, 0x08, 0x8E);
    idt_set_gate(45, (unsigned int)irq13, 0x08, 0x8E);
    idt_set_gate(46, (unsigned int)irq14, 0x08, 0x8E);
    idt_set_gate(47, (unsigned int)irq15, 0x08, 0x8E);

    idt_flush((unsigned int)&idt_ptr);
}
