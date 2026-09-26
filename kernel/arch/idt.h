#ifndef IDT_H
#define IDT_H

typedef struct {
    unsigned short offset_low;
    unsigned short selector;
    unsigned char  zero;
    unsigned char  type_attr;
    unsigned short offset_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    unsigned short limit;
    unsigned int   base;
} __attribute__((packed)) idt_ptr_t;

typedef struct {
    unsigned int ds;
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;
    unsigned int int_no, err_code;
    unsigned int eip, cs, eflags, useresp, ss;
} __attribute__((packed)) registers_t;

typedef void (*isr_handler_t)(registers_t *);

void idt_init(void);
void idt_set_gate(unsigned char num, unsigned int base, unsigned short sel, unsigned char flags);
void register_interrupt_handler(unsigned char n, isr_handler_t handler);

#endif
