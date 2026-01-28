#include "boot.h"
#include "console.h"
#include "floppy.h"
#include "ata.h"
#include "idt.h"
#include "mem.h"
#include "ports.h"
#include "scheme/scheme.h"
#include "thread.h"

static void acpi_shutdown(void) {
    outb(0xF4, 0x00);
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static int scheme_spawn_program(void *user, const char *code);

static unsigned char *ramdisk_base;
static unsigned int ramdisk_size;
static unsigned int ramdisk_lba;

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

static int scheme_read_char(void *user) {
    (void)user;
    while (!console_has_input()) {
        thread_yield();
    }
    return (unsigned char)console_getc();
}

static int scheme_write_bytes(void *user, int offset, const char *data, int len) {
    (void)user;
    if (offset < 0 || len < 0) {
        return -1;
    }
    unsigned int end = (unsigned int)offset + (unsigned int)len;
    if (end > ramdisk_size) {
        return -1;
    }
    for (int i = 0; i < len; i++) {
        ramdisk_base[offset + i] = (unsigned char)data[i];
    }
    unsigned int start_sector = (unsigned int)offset / 512;
    unsigned int end_sector = (end - 1) / 512;
    static unsigned char sector_buf[512];
    for (unsigned int s = start_sector; s <= end_sector; s++) {
        unsigned int base = s * 512;
        for (unsigned int i = 0; i < 512; i++) {
            sector_buf[i] = ramdisk_base[base + i];
        }
        if (ata_write_sector_lba(s, sector_buf) < 0) {
            return -1;
        }
    }
    return len;
}

static unsigned int read_u32_le(const unsigned char *p) {
    return (unsigned int)p[0] |
           ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

enum { SCHEME_HEAP_CELLS = 16384 };
enum { SCHEME_SYM_BUF = 16384 };
enum { SCHEME_STR_BUF = 65536 };
typedef struct SchemeThreadCtx {
    Scheme sc;
    Cell *heap;
    char *sym_buf;
    char *str_buf;
    const char *program;
    int active;
} SchemeThreadCtx;

enum { MAX_SCHEME_THREADS = 4 };

static SchemeThreadCtx scheme_threads[MAX_SCHEME_THREADS];

static unsigned int str_len(const char *s) {
    unsigned int n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

static int scheme_thread_alloc(SchemeThreadCtx *ctx) {
    if (!ctx->heap) {
        ctx->heap = (Cell *)kmalloc(sizeof(Cell) * SCHEME_HEAP_CELLS);
        ctx->sym_buf = (char *)kmalloc(SCHEME_SYM_BUF);
        ctx->str_buf = (char *)kmalloc(SCHEME_STR_BUF);
    }
    if (!ctx->heap || !ctx->sym_buf || !ctx->str_buf) {
        console_write("scheme_thread_alloc: out of memory\n");
        return -1;
    }
    return 0;
}

static void scheme_thread(void *arg) {
    SchemeThreadCtx *ctx = (SchemeThreadCtx *)arg;
    SchemeConfig cfg;
    cfg.heap = ctx->heap;
    cfg.heap_cells = SCHEME_HEAP_CELLS;
    cfg.sym_buf = ctx->sym_buf;
    cfg.sym_buf_size = SCHEME_SYM_BUF;
    cfg.str_buf = ctx->str_buf;
    cfg.str_buf_size = SCHEME_STR_BUF;
    cfg.platform.user = NULL;
    cfg.platform.putc = scheme_putc;
    cfg.platform.panic = scheme_panic;
    cfg.platform.foreign_call = scheme_foreign_call;
    cfg.platform.read_byte = scheme_read_byte;
    cfg.platform.disk_size = scheme_disk_size;
    cfg.platform.read_char = scheme_read_char;
    cfg.platform.write_bytes = scheme_write_bytes;
    cfg.platform.spawn_thread = scheme_spawn_program;

    scheme_init(&ctx->sc, &cfg);
    scheme_eval_string(&ctx->sc, ctx->program);
    ctx->active = 0;
    thread_exit();
}

static int scheme_spawn_program(void *user, const char *code) {
    (void)user;
    if (!code) {
        return -1;
    }
    for (int i = 0; i < MAX_SCHEME_THREADS; i++) {
        if (!scheme_threads[i].active) {
            scheme_threads[i].active = 1;
            if (scheme_thread_alloc(&scheme_threads[i]) < 0) {
                scheme_threads[i].active = 0;
                return -1;
            }
            unsigned int len = str_len(code);
            char *copy = (char *)kmalloc(len + 1);
            if (!copy) {
                scheme_threads[i].active = 0;
                return -1;
            }
            for (unsigned int j = 0; j <= len; j++) {
                copy[j] = code[j];
            }
            scheme_threads[i].program = copy;
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
    static const char boot_msg[] = "SlopOS booting...\n";
    extern char __kernel_end;
    const BootInfo *info = boot_info();
    ramdisk_base = (unsigned char *)info->ramdisk_base;
    ramdisk_size = info->ramdisk_size;
    ramdisk_lba = info->ramdisk_lba;

    console_init();
    console_write(boot_msg);

    mem_init(info, (unsigned int)&__kernel_end);

    thread_init();
    pic_remap();
    outb(0x21, 0xFE);
    outb(0xA1, 0xFF);
    idt_init();
    pit_init(100);
    __asm__ volatile ("sti");

    // Main Scheme instance that runs boot.scm out of the ramdisk.
    Scheme sc;
    SchemeConfig cfg;
    Cell *heap = (Cell *)kmalloc(sizeof(Cell) * SCHEME_HEAP_CELLS);
    char *sym_buf = (char *)kmalloc(SCHEME_SYM_BUF);
    char *str_buf = (char *)kmalloc(SCHEME_STR_BUF);
    if (!heap || !sym_buf || !str_buf) {
        console_write("kernel: scheme heap alloc failed\n");
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }
    cfg.heap = heap;
    cfg.heap_cells = SCHEME_HEAP_CELLS;
    cfg.sym_buf = sym_buf;
    cfg.sym_buf_size = SCHEME_SYM_BUF;
    cfg.str_buf = str_buf;
    cfg.str_buf_size = SCHEME_STR_BUF;
    cfg.platform.user = NULL;
    cfg.platform.putc = scheme_putc;
    cfg.platform.panic = scheme_panic;
    cfg.platform.foreign_call = scheme_foreign_call;
    cfg.platform.read_byte = scheme_read_byte;
    cfg.platform.disk_size = scheme_disk_size;
    cfg.platform.read_char = scheme_read_char;
    cfg.platform.write_bytes = scheme_write_bytes;
    cfg.platform.spawn_thread = scheme_spawn_program;

    scheme_init(&sc, &cfg);
    static char boot_buf[4096];
    unsigned int boot_len = read_u32_le(ramdisk_base);
    if (boot_len >= sizeof(boot_buf)) {
        scheme_panic("boot.scm too large");
    }
    // Copy boot.scm into a null-terminated buffer for the interpreter.
    for (unsigned int i = 0; i < boot_len; i++) {
        boot_buf[i] = (char)ramdisk_base[8 + i];
    }
    boot_buf[boot_len] = '\0';
    scheme_eval_string(&sc, boot_buf);

    // Wait for cooperative Scheme threads to finish, then power off.
    while (thread_active_count() > 0) {
        thread_yield();
    }

    acpi_shutdown();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
