#ifndef SLOPOS_SCHEME_H
#define SLOPOS_SCHEME_H

#ifndef SCHEME_NO_STDLIB
#include <stddef.h>
#else
typedef unsigned long size_t;
#ifndef NULL
#define NULL ((void *)0)
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct Scheme;

typedef enum {
    T_NIL,
    T_BOOL,
    T_INT,
    T_CHAR,
    T_STRING,
    T_SYMBOL,
    T_PAIR,
    T_PRIMITIVE,
    T_CLOSURE
} CellType;

typedef struct Cell {
    CellType type;
    unsigned char mark;
    union {
        int i;
        int b;
        struct {
            struct Cell *car;
            struct Cell *cdr;
        } pair;
        struct {
            const char *name;
        } sym;
        struct {
            const char *data;
            size_t len;
        } str;
        struct {
            struct Cell *(*fn)(struct Scheme *sc, struct Cell *args);
        } prim;
        struct {
            struct Cell *params;
            struct Cell *body;
            struct Cell *env;
        } closure;
    } as;
} Cell;

typedef void (*scheme_putc_fn)(char c);
typedef void (*scheme_panic_fn)(const char *msg);
typedef int (*scheme_foreign_call_fn)(const char *name, int argc, const int *argv);
typedef int (*scheme_read_byte_fn)(void *user, int offset);
typedef int (*scheme_disk_size_fn)(void *user);
typedef int (*scheme_read_char_fn)(void *user);

typedef struct SchemePlatform {
    void *user;
    scheme_putc_fn putc;
    scheme_panic_fn panic;
    scheme_foreign_call_fn foreign_call;
    scheme_read_byte_fn read_byte;
    scheme_disk_size_fn disk_size;
    scheme_read_char_fn read_char;
} SchemePlatform;

typedef struct Scheme {
    Cell *heap;
    size_t heap_cells;
    Cell *free_list;

    char *sym_buf;
    size_t sym_buf_size;
    size_t sym_buf_used;

    char *str_buf;
    size_t str_buf_size;
    size_t str_buf_used;

    Cell *interned_syms;

    Cell *root_stack[256];
    size_t root_top;

    Cell *global_env;

    SchemePlatform platform;

    Cell nil_cell;
    Cell true_cell;
    Cell false_cell;
} Scheme;

typedef struct SchemeConfig {
    Cell *heap;
    size_t heap_cells;
    char *sym_buf;
    size_t sym_buf_size;
    char *str_buf;
    size_t str_buf_size;
    SchemePlatform platform;
} SchemeConfig;

void scheme_init(Scheme *sc, const SchemeConfig *cfg);
int scheme_eval_string(Scheme *sc, const char *input);

#ifdef __cplusplus
}
#endif

#endif
