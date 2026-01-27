#ifndef SLOPOS_CONSOLE_H
#define SLOPOS_CONSOLE_H

void console_init(void);
void console_putc(char c);
void console_write(const char *s);
void console_write_dec(unsigned int value);

#endif
