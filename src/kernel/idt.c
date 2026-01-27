#include "idt.h"
#include "ports.h"

#define IDT_SIZE 256

struct idt_entry {
    unsigned short offset_low;
    unsigned short selector;
    unsigned char zero;
    unsigned char flags;
    unsigned short offset_high;
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

extern void isr_timer_stub(void);

static struct idt_entry idt[IDT_SIZE];

static void idt_set_gate(int num, unsigned int base, unsigned short sel, unsigned char flags) {
    idt[num].offset_low = (unsigned short)(base & 0xFFFF);
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].flags = flags;
    idt[num].offset_high = (unsigned short)((base >> 16) & 0xFFFF);
}

void idt_init(void) {
    struct idt_ptr idtp;

    for (int i = 0; i < IDT_SIZE; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    idt_set_gate(32, (unsigned int)isr_timer_stub, 0x08, 0x8E);

    idtp.limit = (unsigned short)(sizeof(idt) - 1);
    idtp.base = (unsigned int)&idt;

    __asm__ volatile ("lidt %0" : : "m"(idtp));
}

void pic_remap(void) {
    unsigned char a1 = inb(0x21);
    unsigned char a2 = inb(0xA1);

    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    outb(0x21, a1);
    outb(0xA1, a2);
}

void pit_init(unsigned int hz) {
    unsigned int divisor = 1193180 / hz;
    outb(0x43, 0x36);
    outb(0x40, (unsigned char)(divisor & 0xFF));
    outb(0x40, (unsigned char)((divisor >> 8) & 0xFF));
}
