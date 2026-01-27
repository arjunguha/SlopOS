#include <stdio.h>
#include <stdlib.h>

#include "scheme/scheme.h"

static void host_putc(char c) {
    fputc(c, stdout);
}

static void host_panic(const char *msg) {
    fprintf(stderr, "scheme panic: %s\n", msg);
    exit(1);
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
    struct Cell *heap = (struct Cell *)calloc(heap_cells, sizeof(struct Cell));
    char *sym_buf = (char *)calloc(sym_buf_size, 1);
    if (!heap || !sym_buf) {
        perror("calloc");
        return 1;
    }

    Scheme sc;
    SchemeConfig cfg;
    cfg.heap = (struct Cell *)heap;
    cfg.heap_cells = heap_cells;
    cfg.sym_buf = sym_buf;
    cfg.sym_buf_size = sym_buf_size;
    cfg.platform.putc = host_putc;
    cfg.platform.panic = host_panic;

    scheme_init(&sc, &cfg);
    scheme_eval_string(&sc, input ? input : default_program);

    free(input);
    free(heap);
    free(sym_buf);
    return 0;
}
