#ifndef ARCH_PIC_H
#define ARCH_PIC_H
void pic_init(void);
void pic_eoi(int irq);
void pic_mask(int irq);
void pic_unmask(int irq);
#endif
