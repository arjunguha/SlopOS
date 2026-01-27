#include "boot.h"
#include "console.h"
#include "idt.h"
#include "ports.h"
#include "scheme/scheme.h"
#include "thread.h"

static unsigned int factorial(unsigned int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

static void acpi_shutdown(void) {
    /* QEMU ACPI poweroff */
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
}

static int scheme_spawn_program(int program_id);

static unsigned char *ramdisk_base;
static unsigned int ramdisk_size;

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

static int scheme_foreign_call(const char *name, int argc, const int *argv) {
    if (name[0] == 'p' && name[1] == 'u' && name[2] == 't' && name[3] == 'c' && name[4] == '\0') {
        if (argc >= 1) {
            console_putc((char)argv[0]);
            return 0;
        }
        return -1;
    }
    if (name[0] == 's' && name[1] == 'h' && name[2] == 'u' && name[3] == 't' &&
        name[4] == 'd' && name[5] == 'o' && name[6] == 'w' && name[7] == 'n' && name[8] == '\0') {
        acpi_shutdown();
        return 0;
    }
    if (name[0] == 'y' && name[1] == 'i' && name[2] == 'e' && name[3] == 'l' && name[4] == 'd' && name[5] == '\0') {
        thread_yield();
        return 0;
    }
    if (name[0] == 's' && name[1] == 'l' && name[2] == 'e' && name[3] == 'e' && name[4] == 'p' && name[5] == '\0') {
        if (argc >= 1) {
            thread_sleep((unsigned int)argv[0]);
            return 0;
        }
        return -1;
    }
    if (name[0] == 's' && name[1] == 'p' && name[2] == 'a' && name[3] == 'w' && name[4] == 'n' && name[5] == '\0') {
        if (argc >= 1) {
            return scheme_spawn_program(argv[0]);
        }
        return -1;
    }
    return -1;
}

static int scheme_read_byte(void *user, int offset) {
    (void)user;
    if (offset < 0 || (unsigned int)offset >= ramdisk_size) {
        return -1;
    }
    return ramdisk_base[offset];
}

static int scheme_disk_size(void *user) {
    (void)user;
    return (int)ramdisk_size;
}

typedef struct SchemeThreadCtx {
    Scheme sc;
    Cell heap[2048];
    char sym_buf[4096];
    char str_buf[4096];
    const char *program;
    int active;
} SchemeThreadCtx;

enum { MAX_SCHEME_THREADS = 4 };
enum { SCHEME_PROGRAM_COUNT = 2 };

static const char *scheme_programs[SCHEME_PROGRAM_COUNT] = {
    "(begin "
    "  (define (loop n) "
    "    (if (< n 1) 0 "
    "        (begin (display 1) (newline) (foreign-call 'yield) (loop (- n 1))))) "
    "  (loop 5))",
    "(begin "
    "  (define (loop n) "
    "    (if (< n 1) 0 "
    "        (begin (display 2) (newline) (foreign-call 'yield) (loop (- n 1))))) "
    "  (loop 5))"
};

static SchemeThreadCtx scheme_threads[MAX_SCHEME_THREADS];

static void scheme_thread(void *arg) {
    SchemeThreadCtx *ctx = (SchemeThreadCtx *)arg;
    SchemeConfig cfg;
    cfg.heap = ctx->heap;
    cfg.heap_cells = sizeof(ctx->heap) / sizeof(ctx->heap[0]);
    cfg.sym_buf = ctx->sym_buf;
    cfg.sym_buf_size = sizeof(ctx->sym_buf);
    cfg.str_buf = ctx->str_buf;
    cfg.str_buf_size = sizeof(ctx->str_buf);
    cfg.platform.user = NULL;
    cfg.platform.putc = scheme_putc;
    cfg.platform.panic = scheme_panic;
    cfg.platform.foreign_call = scheme_foreign_call;
    cfg.platform.read_byte = scheme_read_byte;
    cfg.platform.disk_size = scheme_disk_size;

    scheme_init(&ctx->sc, &cfg);
    scheme_eval_string(&ctx->sc, ctx->program);
    ctx->active = 0;
    thread_exit();
}

static int scheme_spawn_program(int program_id) {
    if (program_id < 0 || program_id >= SCHEME_PROGRAM_COUNT) {
        return -1;
    }
    for (int i = 0; i < MAX_SCHEME_THREADS; i++) {
        if (!scheme_threads[i].active) {
            scheme_threads[i].active = 1;
            scheme_threads[i].program = scheme_programs[program_id];
            if (thread_spawn(scheme_thread, &scheme_threads[i]) < 0) {
                scheme_threads[i].active = 0;
                return -1;
            }
            return i;
        }
    }
    return -1;
}

void kmain(void) {
    static const char hello_msg[] = "Hello from C!\n";
    static const char fact_msg[] = "factorial(5) = ";
    static const char newline_msg[] = "\n";
    static const char scheme_msg[] = "scheme factorial(5) = ";

    unsigned int n = 5;
    unsigned int result = factorial(n);
    const BootInfo *info = boot_info();
    ramdisk_base = (unsigned char *)info->ramdisk_base;
    ramdisk_size = info->ramdisk_size;

    console_init();
    console_write(hello_msg);
    console_write(fact_msg);
    console_write_dec(result);
    console_write(newline_msg);

    thread_init();
    pic_remap();
    idt_init();
    pit_init(100);
    __asm__ volatile ("sti");

    static Cell heap[4096];
    static char sym_buf[8192];
    static char str_buf[8192];
    Scheme sc;
    SchemeConfig cfg;
    cfg.heap = heap;
    cfg.heap_cells = sizeof(heap) / sizeof(heap[0]);
    cfg.sym_buf = sym_buf;
    cfg.sym_buf_size = sizeof(sym_buf);
    cfg.str_buf = str_buf;
    cfg.str_buf_size = sizeof(str_buf);
    cfg.platform.user = NULL;
    cfg.platform.putc = scheme_putc;
    cfg.platform.panic = scheme_panic;
    cfg.platform.foreign_call = scheme_foreign_call;
    cfg.platform.read_byte = scheme_read_byte;
    cfg.platform.disk_size = scheme_disk_size;

    scheme_init(&sc, &cfg);
    console_write(scheme_msg);
    scheme_eval_string(&sc,
        "(begin "
        "  (define (u8 off) (disk-read-byte off)) "
        "  (define (u32 off) (+ (u8 off) (* 256 (u8 (+ off 1))) "
        "                          (* 65536 (u8 (+ off 2))) (* 16777216 (u8 (+ off 3))))) "
        "  (define (cadr x) (car (cdr x))) "
        "  (define (list a b) (cons a (cons b '()))) "
        "  (define dir-off (u32 12)) "
        "  (define dir-len (u32 16)) "
        "  (define dir-limit (+ dir-off dir-len)) "
        "  (define (find-file-loop name off) "
        "    (if (< off dir-limit) "
        "        (if (string=? (disk-read-cstring off 64) name) "
        "            (list (u32 (+ off 64)) (u32 (+ off 68))) "
        "            (find-file-loop name (+ off 76))) "
        "        #f)) "
        "  (define (find-file name) (find-file-loop name dir-off)) "
        "  (define (load name) "
        "    (begin "
        "      (define info (find-file name)) "
        "      (if info "
        "          (eval-string (disk-read-bytes (car info) (cadr info))) "
        "          (begin (display \"missing file: \") (display name) (newline))))) "
        "  (load \"init.scm\"))");

    while (thread_active_count() > 0) {
        thread_yield();
    }

    acpi_shutdown();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
