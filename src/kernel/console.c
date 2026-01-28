#include "console.h"

typedef unsigned char u8;
typedef unsigned short u16;

static inline void outb(u16 port, u8 val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void serial_write(u8 c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0) {
    }
    outb(0x3F8, c);
}

int console_has_input(void) {
    return (inb(0x3F8 + 5) & 0x01) != 0;
}

char console_getc(void) {
    while (!console_has_input()) {
    }
    return (char)inb(0x3F8);
}

void console_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}

void console_putc(char c) {
    if (c == '\n') {
        serial_write('\r');
    }
    serial_write((u8)c);
}

void console_write(const char *s) {
    for (; *s; s++) {
        console_putc(*s);
    }
}

void console_write_dec(unsigned int value) {
    char buf[11];
    int i = 0;

    if (value == 0) {
        console_write("0");
        return;
    }

    while (value > 0 && i < (int)sizeof(buf) - 1) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        char c[2];
        c[0] = buf[--i];
        c[1] = '\0';
        console_write(c);
    }
}
