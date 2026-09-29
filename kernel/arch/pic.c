#include "pic.h"
#include "io.h"

void pic_send_eoi(unsigned char irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

static void apic_disable_or_mask(void) {
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
        return; // APIC is not enabled
    }

    unsigned int apic_base = lo & 0xFFFFF000;
    if (apic_base == 0) apic_base = 0xFEE00000;

    volatile unsigned int *apic = (volatile unsigned int *)apic_base;
    // Mask all Local APIC LVT interrupt lines (bit 16 = 0x10000)
    apic[0x320 / 4] |= 0x10000; // Timer
    apic[0x330 / 4] |= 0x10000; // Thermal
    apic[0x340 / 4] |= 0x10000; // Performance Counter
    apic[0x350 / 4] |= 0x10000; // LINT0
    apic[0x360 / 4] |= 0x10000; // LINT1
    apic[0x370 / 4] |= 0x10000; // Error
    apic[0x0B0 / 4] = 0;        // EOI
}

void pic_init(void) {
    // 1. Mask and silence any active Local APIC interrupts left by UEFI firmware
    apic_disable_or_mask();

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

    // Mask all interrupts by default (0xFF)
    outb(PIC1_DATA, 0xFF);
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
