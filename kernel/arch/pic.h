#ifndef PIC_H
#define PIC_H

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIC_EOI      0x20

void pic_init(void);
void pic_send_eoi(unsigned char irq);
void pic_mask_irq(unsigned char irq);
void pic_unmask_irq(unsigned char irq);

unsigned int pic_get_apic_base(void);
unsigned int pic_get_apic_lint0(void);
unsigned int pic_get_apic_svr(void);
unsigned int pic_get_apic_tpr(void);
int          pic_is_apic_present(void);
void         pic_apic_eoi(void);

#endif
