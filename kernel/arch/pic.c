#include "pic.h"
#include "io.h"

void pic_send_eoi(unsigned char irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

static void apic_setup_virtual_wire_mode(void) {
    // Check CPUID for APIC support (EDX bit 9 of CPUID leaf 1)
    unsigned int eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    if (!(edx & (1 << 9))) {
        return; // No APIC on CPU
    }

    // Read IA32_APIC_BASE MSR (0x1B)
    unsigned int lo, hi;
    __asm__ volatile ("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0x1B));
    if (!(lo & (1 << 11))) {
        return; // APIC is hardware disabled; external PIC routes directly to CPU
    }

    unsigned int apic_base = lo & 0xFFFFF000;
    if (apic_base == 0) apic_base = 0xFEE00000;

    volatile unsigned int *apic = (volatile unsigned int *)apic_base;

    // 1. Clear Task Priority Register (TPR, offset 0x080) to allow all interrupt priorities
    apic[0x080 / 4] = 0;

    // 2. Enable Local APIC in software and set spurious interrupt vector to 0xFF (offset 0x0F0)
    // Bit 8 is the APIC Software Enable bit (required for LINT0 routing)
    apic[0x0F0 / 4] = 0x1FF;

    // 3. Mask unused Local APIC internal timers/counters to avoid unexpected APIC interrupts
    apic[0x320 / 4] = 0x10000; // Timer LVT (bit 16 = masked)
    apic[0x330 / 4] = 0x10000; // Thermal LVT (bit 16 = masked)
    apic[0x340 / 4] = 0x10000; // Performance Counter LVT (bit 16 = masked)
    apic[0x370 / 4] = 0x10000; // Error LVT (bit 16 = masked)

    // 4. Configure LINT0 for ExtINT mode (Virtual Wire Mode):
    // Delivery mode 111b (bits 10:8 = 0x700), edge-triggered (bit 15 = 0), active-high (bit 13 = 0),
    // and UNMASKED (bit 16 = 0).
    // This allows the 8259 Master PIC to transparently deliver interrupts (PIT IRQ0, KBD IRQ1, Mouse IRQ12) to the CPU!
    apic[0x350 / 4] = 0x00000700;

    // 5. Configure LINT1 for NMI delivery mode:
    // Delivery mode 100b (bits 10:8 = 0x400), unmasked (bit 16 = 0).
    apic[0x360 / 4] = 0x00000400;

    // 6. Clear any pending Local APIC interrupt
    apic[0x0B0 / 4] = 0;
}

void pic_init(void) {
    // 1. Configure Local APIC in Virtual Wire Mode so 8259 PIC interrupts pass through to CPU!
    apic_setup_virtual_wire_mode();

    unsigned char a1 = inb(PIC1_DATA);
    unsigned char a2 = inb(PIC2_DATA);
    (void)a1;
    (void)a2;

    // ICW1: Start initialization in cascade mode
    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();

    // ICW2: Vector offset (Master: 0x20=32, Slave: 0x28=40)
    outb(PIC1_DATA, 0x20);
    io_wait();
    outb(PIC2_DATA, 0x28);
    io_wait();

    // ICW3: Tell Master there is a slave PIC at IRQ2 (0000 0100)
    outb(PIC1_DATA, 0x04);
    io_wait();
    // Tell Slave its cascade identity (0000 0010)
    outb(PIC2_DATA, 0x02);
    io_wait();

    // ICW4: Have PICs operate in 8086 mode
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    // Mask all interrupts by default, except IRQ2 (cascade)
    outb(PIC1_DATA, 0xFB); // Bit 2 = 0 (unmask IRQ2 cascade so slave PIC can signal)
    outb(PIC2_DATA, 0xFF);
}

void pic_unmask_irq(unsigned char irq) {
    unsigned short port;
    unsigned char value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

void pic_mask_irq(unsigned char irq) {
    unsigned short port;
    unsigned char value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) | (1 << irq);
    outb(port, value);
}
