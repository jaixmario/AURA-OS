#include "pic.h"
#include "io.h"

void pic_send_eoi(unsigned char irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_init(void) {
    unsigned char a1 = inb(PIC1_DATA);
    unsigned char a2 = inb(PIC2_DATA);

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
