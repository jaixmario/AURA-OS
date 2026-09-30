#include "pic.h"
#include "io.h"

void pic_send_eoi(unsigned char irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

static unsigned int g_apic_base = 0;
static unsigned int g_apic_lint0 = 0;
static unsigned int g_apic_svr = 0;
static unsigned int g_apic_tpr = 0;
static int g_apic_present = 0;

unsigned int pic_get_apic_base(void) { return g_apic_base; }
unsigned int pic_get_apic_lint0(void) { return g_apic_lint0; }
unsigned int pic_get_apic_svr(void) { return g_apic_svr; }
unsigned int pic_get_apic_tpr(void) { return g_apic_tpr; }
int pic_is_apic_present(void) { return g_apic_present; }

static inline void wrmsr(unsigned int msr, unsigned int lo, unsigned int hi) {
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static inline void rdmsr(unsigned int msr, unsigned int *lo, unsigned int *hi) {
    __asm__ volatile ("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}

void pic_apic_eoi(void) {
    // ExtINT delivery mode passes 8259 PIC interrupts directly and does NOT
    // set an In-Service Register bit in the Local APIC (Intel SDM Section 10.8.5).
    // PIC EOI (pic_send_eoi) is all that is required.
}

static void apic_setup_virtual_wire_mode(void) {
    // Check CPUID for APIC support (EDX bit 9 of CPUID leaf 1)
    unsigned int eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
    if (!(edx & (1 << 9))) {
        return; // No APIC on CPU
    }

    // Read IA32_APIC_BASE MSR (0x1B)
    unsigned int lo = 0, hi = 0;
    rdmsr(0x1B, &lo, &hi);
    if (!(lo & (1 << 11))) {
        return; // APIC is hardware disabled; external PIC routes directly to CPU
    }

    if (lo & (1 << 10)) {
        // x2APIC Mode is active (standard on modern AMD Ryzen / Intel UEFI laptops):
        // In x2APIC mode, the MMIO interface (0xFEE00000) is DISABLED by hardware.
        // All APIC registers are accessed via MSRs (0x800-0x83F).
        // 1. Clear Task Priority Register (TPR, MSR 0x808)
        wrmsr(0x808, 0, 0);

        // 2. Enable Local APIC in software and set spurious vector to 0xFF (SVR, MSR 0x80F)
        wrmsr(0x80F, 0x1FF, 0);

        // 3. Mask unused LVTs
        wrmsr(0x832, 0x10000, 0); // Timer LVT
        wrmsr(0x833, 0x10000, 0); // Thermal LVT
        wrmsr(0x834, 0x10000, 0); // Perf LVT
        wrmsr(0x837, 0x10000, 0); // Error LVT

        // 4. Configure LINT0 for ExtINT mode (Virtual Wire Mode):
        // Delivery mode 111b (ExtINT = 0x700), edge-triggered, active-high, unmasked (bit 16=0)
        wrmsr(0x835, 0x00000700, 0);

        // 5. Configure LINT1 for NMI delivery mode:
        wrmsr(0x836, 0x00000400, 0);

        g_apic_present = 1;
        g_apic_base = 0x00000800; // Signifies x2APIC MSR architecture in HUD
        rdmsr(0x835, &g_apic_lint0, &hi);
        rdmsr(0x80F, &g_apic_svr, &hi);
        rdmsr(0x808, &g_apic_tpr, &hi);
    } else {
        // Legacy xAPIC Mode (MMIO interface at apic_base, standard in BIOS / QEMU / VMware):
        unsigned int apic_base = lo & 0xFFFFF000;
        if (apic_base == 0) apic_base = 0xFEE00000;

        volatile unsigned int *apic = (volatile unsigned int *)apic_base;

        // 1. Clear Task Priority Register (TPR, offset 0x080) to allow all interrupt priorities
        apic[0x080 / 4] = 0;

        // 2. Enable Local APIC in software and set spurious interrupt vector to 0xFF (offset 0x0F0)
        apic[0x0F0 / 4] = 0x1FF;

        // 3. Mask unused Local APIC internal timers/counters to avoid unexpected APIC interrupts
        apic[0x320 / 4] = 0x10000; // Timer LVT (bit 16 = masked)
        apic[0x330 / 4] = 0x10000; // Thermal LVT (bit 16 = masked)
        apic[0x340 / 4] = 0x10000; // Performance Counter LVT (bit 16 = masked)
        apic[0x370 / 4] = 0x10000; // Error LVT (bit 16 = masked)

        // 4. Configure LINT0 for ExtINT mode (Virtual Wire Mode):
        apic[0x350 / 4] = 0x00000700;

        // 5. Configure LINT1 for NMI delivery mode:
        apic[0x360 / 4] = 0x00000400;

        // 6. Clear any pending Local APIC interrupt
        apic[0x0B0 / 4] = 0;

        // Save state for live debug HUD
        g_apic_present = 1;
        g_apic_base = apic_base;
        g_apic_lint0 = apic[0x350 / 4];
        g_apic_svr = apic[0x0F0 / 4];
        g_apic_tpr = apic[0x080 / 4];
    }
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
