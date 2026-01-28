#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scheme/scheme.h"

static void host_putc(char c) {
    fputc(c, stdout);
}

static void host_panic(const char *msg) {
    fprintf(stderr, "scheme panic: %s\n", msg);
    exit(1);
}

static int host_read_char(void *user) {
    (void)user;
    return getchar();
}

static int host_foreign_call(const char *name, int argc, const int *argv) {
    if (strcmp(name, "putc") == 0) {
        if (argc >= 1) {
            fputc(argv[0], stdout);
            return 0;
        }
        return -1;
    }
    if (strcmp(name, "exit") == 0) {
        int code = argc >= 1 ? argv[0] : 0;
        exit(code);
    }
    return -1;
}

typedef struct HostDisk {
    unsigned char *data;
    size_t size;
} HostDisk;

static int host_read_byte(void *user, int offset) {
    HostDisk *disk = (HostDisk *)user;
    if (!disk || !disk->data || offset < 0 || (size_t)offset >= disk->size) {
        return -1;
    }
    return disk->data[offset];
}

static int host_disk_size(void *user) {
    HostDisk *disk = (HostDisk *)user;
    if (!disk) {
        return 0;
    }
    return (int)disk->size;
}

int main(int argc, char **argv) {
    const char *default_program =
        "(begin\n"
        "  (define (fact n) (if (< n 2) 1 (* n (fact (- n 1)))))\n"
        "  (display (fact 5))\n"
        "  (newline))\n";

    char *input = NULL;
    if (argc > 1) {
        FILE *f = fopen(argv[1], "rb");
        if (!f) {
            perror("fopen");
            return 1;
        }
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        input = (char *)malloc((size_t)size + 1);
        if (!input) {
            perror("malloc");
            fclose(f);
            return 1;
        }
        if (fread(input, 1, (size_t)size, f) != (size_t)size) {
            perror("fread");
            fclose(f);
            free(input);
            return 1;
        }
        input[size] = '\0';
        fclose(f);
    }

    const size_t heap_cells = 4096;
    const size_t sym_buf_size = 8192;
    const size_t str_buf_size = 8192;
    struct Cell *heap = (struct Cell *)calloc(heap_cells, sizeof(struct Cell));
    char *sym_buf = (char *)calloc(sym_buf_size, 1);
    char *str_buf = (char *)calloc(str_buf_size, 1);
    if (!heap || !sym_buf || !str_buf) {
        perror("calloc");
        return 1;
    }

    HostDisk disk = {0};
    if (argc > 2) {
        FILE *df = fopen(argv[2], "rb");
        if (!df) {
            perror("fopen");
            return 1;
        }
        fseek(df, 0, SEEK_END);
        long dsize = ftell(df);
        fseek(df, 0, SEEK_SET);
        disk.data = (unsigned char *)malloc((size_t)dsize);
        if (!disk.data) {
            perror("malloc");
            fclose(df);
            return 1;
        }
        if (fread(disk.data, 1, (size_t)dsize, df) != (size_t)dsize) {
            perror("fread");
            fclose(df);
            free(disk.data);
            return 1;
        }
        fclose(df);
        disk.size = (size_t)dsize;
    }

    Scheme sc;
    SchemeConfig cfg;
    cfg.heap = (struct Cell *)heap;
    cfg.heap_cells = heap_cells;
    cfg.sym_buf = sym_buf;
    cfg.sym_buf_size = sym_buf_size;
    cfg.str_buf = str_buf;
    cfg.str_buf_size = str_buf_size;
    cfg.platform.user = argc > 2 ? &disk : NULL;
    cfg.platform.putc = host_putc;
    cfg.platform.panic = host_panic;
    cfg.platform.foreign_call = host_foreign_call;
    cfg.platform.read_byte = host_read_byte;
    cfg.platform.disk_size = host_disk_size;
    cfg.platform.read_char = host_read_char;

    scheme_init(&sc, &cfg);
    scheme_eval_string(&sc, input ? input : default_program);

    free(input);
    free(disk.data);
    free(heap);
    free(sym_buf);
    free(str_buf);
    return 0;
}
