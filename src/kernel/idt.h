#ifndef SLOPOS_IDT_H
#define SLOPOS_IDT_H

void idt_init(void);
void pic_remap(void);
void pit_init(unsigned int hz);

#endif
