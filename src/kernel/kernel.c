#include "console.h"
#include "scheme/scheme.h"

static unsigned int factorial(unsigned int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

static inline void outw(unsigned short port, unsigned short value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static void acpi_shutdown(void) {
    /* QEMU ACPI poweroff */
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
}

static void scheme_putc(char c) {
    console_putc(c);
}

static void scheme_panic(const char *msg) {
    console_write("scheme panic: ");
    console_write(msg);
    console_write("\n");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

void kmain(void) {
    static const char hello_msg[] = "Hello from C!\n";
    static const char fact_msg[] = "factorial(5) = ";
    static const char newline_msg[] = "\n";
    static const char scheme_msg[] = "scheme factorial(5) = ";

    unsigned int n = 5;
    unsigned int result = factorial(n);

    console_init();
    console_write(hello_msg);
    console_write(fact_msg);
    console_write_dec(result);
    console_write(newline_msg);

    static Cell heap[4096];
    static char sym_buf[8192];
    Scheme sc;
    SchemeConfig cfg;
    cfg.heap = heap;
    cfg.heap_cells = sizeof(heap) / sizeof(heap[0]);
    cfg.sym_buf = sym_buf;
    cfg.sym_buf_size = sizeof(sym_buf);
    cfg.platform.putc = scheme_putc;
    cfg.platform.panic = scheme_panic;

    scheme_init(&sc, &cfg);
    console_write(scheme_msg);
    scheme_eval_string(&sc, "(begin (define (fact n) (if (< n 2) 1 (* n (fact (- n 1))))) (display (fact 5)) (newline))");

    acpi_shutdown();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
