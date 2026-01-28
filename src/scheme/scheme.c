#include "scheme.h"

#define ROOT_STACK_MAX 256

typedef struct Cell *(*PrimFn)(struct Scheme *sc, struct Cell *args);

static Cell *scheme_nil(Scheme *sc) { return &sc->nil_cell; }
static Cell *scheme_true(Scheme *sc) { return &sc->true_cell; }
static Cell *scheme_false(Scheme *sc) { return &sc->false_cell; }

static void panic(Scheme *sc, const char *msg) {
    if (sc->platform.panic) {
        sc->platform.panic(msg);
    }
    for (;;) {
    }
}

static void putc_out(Scheme *sc, char c) {
    if (sc->platform.putc) {
        sc->platform.putc(c);
    }
}

static void write_str(Scheme *sc, const char *s) {
    while (*s) {
        putc_out(sc, *s++);
    }
}

// push_root: protect a cell from GC during intermediate allocations.
// Args: sc (interpreter state), v (cell to protect).
// Returns: none.
static void push_root(Scheme *sc, Cell *v) {
    if (sc->root_top >= ROOT_STACK_MAX) {
        panic(sc, "root stack overflow");
    }
    sc->root_stack[sc->root_top++] = v;
}

static void pop_roots(Scheme *sc, size_t n) {
    if (sc->root_top < n) {
        panic(sc, "root stack underflow");
    }
    sc->root_top -= n;
}

static Cell *alloc_cell(Scheme *sc);

static void mark_cell(Scheme *sc, Cell *c) {
    if (!c || c->mark) {
        return;
    }
    c->mark = 1;
    switch (c->type) {
        case T_PAIR:
            mark_cell(sc, c->as.pair.car);
            mark_cell(sc, c->as.pair.cdr);
            break;
        case T_CLOSURE:
            mark_cell(sc, c->as.closure.params);
            mark_cell(sc, c->as.closure.body);
            mark_cell(sc, c->as.closure.env);
            break;
        default:
            break;
    }
}

static void env_stack_push(Scheme *sc, Cell *env) {
    if (sc->env_top >= 256) {
        panic(sc, "env stack overflow");
    }
    sc->env_stack[sc->env_top++] = env;
}

static void env_stack_pop(Scheme *sc) {
    if (sc->env_top == 0) {
        panic(sc, "env stack underflow");
    }
    sc->env_top--;
}

// gc_collect: mark-and-sweep collector using global env, active envs, interned symbols, and root stack.
// Args: sc (interpreter state).
// Returns: none.
static void gc_collect(Scheme *sc) {
    size_t i;

    mark_cell(sc, sc->global_env);
    mark_cell(sc, sc->current_env);
    for (i = 0; i < sc->env_top; i++) {
        mark_cell(sc, sc->env_stack[i]);
    }
    mark_cell(sc, sc->interned_syms);
    for (i = 0; i < sc->root_top; i++) {
        mark_cell(sc, sc->root_stack[i]);
    }

    sc->free_list = NULL;
    for (i = 0; i < sc->heap_cells; i++) {
        Cell *c = &sc->heap[i];
        if (c->mark) {
            c->mark = 0;
        } else {
            c->type = T_PAIR;
            c->as.pair.cdr = sc->free_list;
            sc->free_list = c;
        }
    }
}

// alloc_cell: allocate a new cell from the freelist, collecting if needed.
// Args: sc (interpreter state).
// Returns: pointer to a newly allocated cell.
static Cell *alloc_cell(Scheme *sc) {
    if (!sc->free_list) {
        gc_collect(sc);
        if (!sc->free_list) {
            panic(sc, "out of memory");
        }
    }
    Cell *c = sc->free_list;
    sc->free_list = c->as.pair.cdr;
    c->mark = 0;
    return c;
}

static Cell *make_int(Scheme *sc, int v) {
    Cell *c = alloc_cell(sc);
    c->type = T_INT;
    c->as.i = v;
    return c;
}

static Cell *make_char(Scheme *sc, int v) {
    Cell *c = alloc_cell(sc);
    c->type = T_CHAR;
    c->as.i = v;
    return c;
}

static Cell *make_bool(Scheme *sc, int v) {
    return v ? scheme_true(sc) : scheme_false(sc);
}

static char *alloc_str_bytes(Scheme *sc, size_t len) {
    if (sc->str_buf_used + len + 1 > sc->str_buf_size) {
        panic(sc, "string buffer full");
    }
    char *p = sc->str_buf + sc->str_buf_used;
    sc->str_buf_used += len + 1;
    return p;
}

static Cell *make_string_len(Scheme *sc, const char *data, size_t len) {
    Cell *c = alloc_cell(sc);
    c->type = T_STRING;
    char *dst = alloc_str_bytes(sc, len);
    for (size_t i = 0; i < len; i++) {
        dst[i] = data[i];
    }
    dst[len] = '\0';
    c->as.str.data = dst;
    c->as.str.len = len;
    return c;
}

static Cell *cons(Scheme *sc, Cell *a, Cell *d) {
    Cell *c = alloc_cell(sc);
    c->type = T_PAIR;
    c->as.pair.car = a;
    c->as.pair.cdr = d;
    return c;
}

static Cell *make_prim(Scheme *sc, PrimFn fn) {
    Cell *c = alloc_cell(sc);
    c->type = T_PRIMITIVE;
    c->as.prim.fn = fn;
    return c;
}

static Cell *make_closure(Scheme *sc, Cell *params, Cell *body, Cell *env) {
    push_root(sc, params);
    push_root(sc, body);
    push_root(sc, env);
    Cell *c = alloc_cell(sc);
    pop_roots(sc, 3);
    c->type = T_CLOSURE;
    c->as.closure.params = params;
    c->as.closure.body = body;
    c->as.closure.env = env;
    return c;
}

static int is_nil(Scheme *sc, Cell *c) { return c == scheme_nil(sc); }

static Cell *car(Cell *c) { return c->as.pair.car; }
static Cell *cdr(Cell *c) { return c->as.pair.cdr; }

static int streq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static int streq_len(const char *a, const char *b, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return a[len] == '\0';
}

static const char *sym_alloc(Scheme *sc, const char *start, size_t len) {
    if (sc->sym_buf_used + len + 1 > sc->sym_buf_size) {
        panic(sc, "symbol buffer full");
    }
    char *dst = sc->sym_buf + sc->sym_buf_used;
    for (size_t i = 0; i < len; i++) {
        dst[i] = start[i];
    }
    dst[len] = '\0';
    sc->sym_buf_used += len + 1;
    return dst;
}

static Cell *intern_symbol_len(Scheme *sc, const char *start, size_t len) {
    Cell *p = sc->interned_syms;
    while (!is_nil(sc, p)) {
        Cell *sym = car(p);
        if (streq_len(sym->as.sym.name, start, len)) {
            return sym;
        }
        p = cdr(p);
    }

    const char *name = sym_alloc(sc, start, len);
    Cell *sym = alloc_cell(sc);
    sym->type = T_SYMBOL;
    sym->as.sym.name = name;

    push_root(sc, sym);
    sc->interned_syms = cons(sc, sym, sc->interned_syms);
    pop_roots(sc, 1);

    return sym;
}

static Cell *intern_symbol(Scheme *sc, const char *name) {
    size_t len = 0;
    while (name[len]) {
        len++;
    }
    return intern_symbol_len(sc, name, len);
}

static void skip_ws(const char **s) {
    while (**s) {
        if (**s == ';') {
            while (**s && **s != '\n') {
                (*s)++;
            }
        } else if (**s == ' ' || **s == '\t' || **s == '\r' || **s == '\n') {
            (*s)++;
        } else {
            break;
        }
    }
}

static int is_delim(char c) {
    return c == 0 || c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '(' || c == ')' || c == '"';
}

static Cell *read_expr(Scheme *sc, const char **s);
static int eval_string_in_env(Scheme *sc, const char *input, Cell *env);

static Cell *read_list(Scheme *sc, const char **s) {
    skip_ws(s);
    if (**s == ')') {
        (*s)++;
        return scheme_nil(sc);
    }

    Cell *head = NULL;
    Cell *tail = NULL;

    while (**s && **s != ')') {
        Cell *item = read_expr(sc, s);
        if (!item) {
            break;
        }

        if (!head) {
            push_root(sc, item);
            head = cons(sc, item, scheme_nil(sc));
            tail = head;
            pop_roots(sc, 1);
        } else {
            push_root(sc, item);
            Cell *node = cons(sc, item, scheme_nil(sc));
            tail->as.pair.cdr = node;
            tail = node;
            pop_roots(sc, 1);
        }
        skip_ws(s);
    }

    if (**s == ')') {
        (*s)++;
    }

    return head ? head : scheme_nil(sc);
}

static Cell *read_number(Scheme *sc, const char **s) {
    int sign = 1;
    int value = 0;
    if (**s == '-') {
        if ((*s)[1] < '0' || (*s)[1] > '9') {
            return NULL;
        }
        sign = -1;
        (*s)++;
    }
    if (**s < '0' || **s > '9') {
        return NULL;
    }
    while (**s >= '0' && **s <= '9') {
        value = value * 10 + (**s - '0');
        (*s)++;
    }
    return make_int(sc, sign * value);
}

static Cell *read_symbol(Scheme *sc, const char **s) {
    const char *start = *s;
    size_t len = 0;
    while (!is_delim(**s)) {
        (*s)++;
        len++;
    }
    return intern_symbol_len(sc, start, len);
}

static Cell *read_string(Scheme *sc, const char **s) {
    (*s)++;
    const char *start = *s;
    size_t len = 0;
    while (**s && **s != '"') {
        (*s)++;
        len++;
    }
    if (**s == '"') {
        (*s)++;
    }
    return make_string_len(sc, start, len);
}

// read_expr: parse a single expression from the input cursor.
// Args: sc (interpreter state), s (pointer to input cursor).
// Returns: parsed expression cell, or NULL at end of input.
static Cell *read_expr(Scheme *sc, const char **s) {
    skip_ws(s);
    if (**s == 0) {
        return NULL;
    }
    if (**s == '(') {
        (*s)++;
        return read_list(sc, s);
    }
    if (**s == '\'') {
        (*s)++;
        Cell *expr = read_expr(sc, s);
        push_root(sc, expr);
        Cell *quote_sym = intern_symbol(sc, "quote");
        Cell *res = cons(sc, quote_sym, cons(sc, expr, scheme_nil(sc)));
        pop_roots(sc, 1);
        return res;
    }
    if (**s == '#') {
        (*s)++;
        if (**s == '\\') {
            (*s)++;
            const char *start = *s;
            size_t len = 0;
            while (**s && !is_delim(**s)) {
                (*s)++;
                len++;
            }
            if (len == 0) {
                return NULL;
            }
            if (len == 7 && streq_len("newline", start, len)) {
                return make_char(sc, '\n');
            }
            if (len == 6 && streq_len("return", start, len)) {
                return make_char(sc, '\r');
            }
            if (len == 1) {
                return make_char(sc, (unsigned char)start[0]);
            }
            panic(sc, "invalid character literal");
            return scheme_nil(sc);
        }
        if (**s == 't') {
            (*s)++;
            return scheme_true(sc);
        }
        if (**s == 'f') {
            (*s)++;
            return scheme_false(sc);
        }
    }
    if (**s == '"') {
        return read_string(sc, s);
    }

    Cell *num = read_number(sc, s);
    if (num) {
        return num;
    }
    return read_symbol(sc, s);
}

static Cell *env_lookup(Scheme *sc, Cell *env, Cell *sym) {
    while (!is_nil(sc, env)) {
        Cell *frame = car(env);
        while (!is_nil(sc, frame)) {
            Cell *binding = car(frame);
            if (car(binding) == sym) {
                return cdr(binding);
            }
            frame = cdr(frame);
        }
        env = cdr(env);
    }
    return NULL;
}

static void env_define(Scheme *sc, Cell *env, Cell *sym, Cell *val) {
    Cell *frame = car(env);
    push_root(sc, frame);
    push_root(sc, sym);
    push_root(sc, val);
    Cell *binding = cons(sc, sym, val);
    frame = cons(sc, binding, frame);
    env->as.pair.car = frame;
    pop_roots(sc, 3);
}

static int env_set(Scheme *sc, Cell *env, Cell *sym, Cell *val) {
    while (!is_nil(sc, env)) {
        Cell *frame = car(env);
        while (!is_nil(sc, frame)) {
            Cell *binding = car(frame);
            if (car(binding) == sym) {
                binding->as.pair.cdr = val;
                return 1;
            }
            frame = cdr(frame);
        }
        env = cdr(env);
    }
    return 0;
}

static Cell *eval(Scheme *sc, Cell *expr, Cell *env);

// eval_list: evaluate each element of a list in order.
// Args: sc (interpreter state), list (list of expressions), env (environment).
// Returns: list of evaluated values.
static Cell *eval_list(Scheme *sc, Cell *list, Cell *env) {
    if (is_nil(sc, list)) {
        return scheme_nil(sc);
    }
    Cell *head = NULL;
    Cell *tail = NULL;
    while (!is_nil(sc, list)) {
        int pushed_head = 0;
        if (head) {
            push_root(sc, head);
            pushed_head = 1;
        }
        Cell *val = eval(sc, car(list), env);
        push_root(sc, val);
        Cell *node = cons(sc, val, scheme_nil(sc));
        if (!head) {
            head = node;
            tail = node;
        } else {
            tail->as.pair.cdr = node;
            tail = node;
        }
        pop_roots(sc, pushed_head ? 2 : 1);
        list = cdr(list);
    }
    return head ? head : scheme_nil(sc);
}

// apply: apply a primitive or closure to evaluated arguments.
// Args: sc (interpreter state), fn (function), args (argument list).
// Returns: result cell.
static Cell *apply(Scheme *sc, Cell *fn, Cell *args) {
    if (fn->type == T_PRIMITIVE) {
        push_root(sc, fn);
        push_root(sc, args);
        Cell *result = fn->as.prim.fn(sc, args);
        pop_roots(sc, 2);
        return result;
    }
    if (fn->type == T_CLOSURE) {
        push_root(sc, fn);
        push_root(sc, args);
        Cell *new_env = cons(sc, scheme_nil(sc), fn->as.closure.env);
        Cell *prev_env = sc->current_env;
        sc->current_env = new_env;
        env_stack_push(sc, new_env);
        Cell *params = fn->as.closure.params;
        Cell *vals = args;
        while (!is_nil(sc, params) && !is_nil(sc, vals)) {
            env_define(sc, new_env, car(params), car(vals));
            params = cdr(params);
            vals = cdr(vals);
        }
        pop_roots(sc, 1);
        Cell *body = fn->as.closure.body;
        Cell *result = scheme_nil(sc);
        while (!is_nil(sc, body)) {
            result = eval(sc, car(body), new_env);
            body = cdr(body);
        }
        env_stack_pop(sc);
        sc->current_env = prev_env;
        pop_roots(sc, 1);
        return result;
    }
    panic(sc, "attempt to call non-function");
    return scheme_nil(sc);
}

static int is_symbol(Cell *c, const char *name) {
    return c->type == T_SYMBOL && streq(c->as.sym.name, name);
}

// eval: evaluate an expression in the given environment.
// Args: sc (interpreter state), expr (expression), env (environment).
// Returns: result cell.
static Cell *eval(Scheme *sc, Cell *expr, Cell *env) {
    sc->current_env = env;
    if (!expr) {
        return scheme_nil(sc);
    }
    switch (expr->type) {
        case T_INT:
        case T_BOOL:
        case T_STRING:
        case T_CHAR:
        case T_PRIMITIVE:
        case T_CLOSURE:
            return expr;
        case T_SYMBOL: {
            Cell *val = env_lookup(sc, env, expr);
            if (!val) {
                write_str(sc, "unbound symbol: ");
                write_str(sc, expr->as.sym.name);
                write_str(sc, "\n");
                panic(sc, "unbound symbol");
            }
            return val;
        }
        case T_PAIR: {
            Cell *op = car(expr);
            if (is_symbol(op, "quote")) {
                return car(cdr(expr));
            }
            if (is_symbol(op, "if")) {
                Cell *test = eval(sc, car(cdr(expr)), env);
                if (test != scheme_false(sc)) {
                    return eval(sc, car(cdr(cdr(expr))), env);
                }
                return eval(sc, car(cdr(cdr(cdr(expr)))), env);
            }
            if (is_symbol(op, "begin")) {
                Cell *seq = cdr(expr);
                Cell *result = scheme_nil(sc);
                while (!is_nil(sc, seq)) {
                    result = eval(sc, car(seq), env);
                    seq = cdr(seq);
                }
                return result;
            }
            if (is_symbol(op, "define")) {
                Cell *name = car(cdr(expr));
                if (name->type == T_PAIR) {
                    Cell *fname = car(name);
                    Cell *params = cdr(name);
                    Cell *body = cdr(cdr(expr));
                    Cell *closure = make_closure(sc, params, body, env);
                    env_define(sc, env, fname, closure);
                    return fname;
                } else {
                    Cell *value = eval(sc, car(cdr(cdr(expr))), env);
                    env_define(sc, env, name, value);
                    return name;
                }
            }
            if (is_symbol(op, "set!")) {
                Cell *name = car(cdr(expr));
                Cell *value = eval(sc, car(cdr(cdr(expr))), env);
                if (!env_set(sc, env, name, value)) {
                    panic(sc, "set!: unbound symbol");
                }
                return value;
            }
            if (is_symbol(op, "lambda")) {
                Cell *params = car(cdr(expr));
                Cell *body = cdr(cdr(expr));
                return make_closure(sc, params, body, env);
            }

            Cell *fn = eval(sc, op, env);
            Cell *args = eval_list(sc, cdr(expr), env);
            return apply(sc, fn, args);
        }
        default:
            return expr;
    }
}

static Cell *prim_add(Scheme *sc, Cell *args) {
    int sum = 0;
    while (!is_nil(sc, args)) {
        sum += car(args)->as.i;
        args = cdr(args);
    }
    return make_int(sc, sum);
}

static Cell *prim_sub(Scheme *sc, Cell *args) {
    if (is_nil(sc, args)) {
        return make_int(sc, 0);
    }
    int result = car(args)->as.i;
    args = cdr(args);
    if (is_nil(sc, args)) {
        return make_int(sc, -result);
    }
    while (!is_nil(sc, args)) {
        result -= car(args)->as.i;
        args = cdr(args);
    }
    return make_int(sc, result);
}

static Cell *prim_mul(Scheme *sc, Cell *args) {
    int result = 1;
    while (!is_nil(sc, args)) {
        result *= car(args)->as.i;
        args = cdr(args);
    }
    return make_int(sc, result);
}

static Cell *prim_lt(Scheme *sc, Cell *args) {
    int a = car(args)->as.i;
    int b = car(cdr(args))->as.i;
    return make_bool(sc, a < b);
}

static Cell *prim_num_eq(Scheme *sc, Cell *args) {
    int a = car(args)->as.i;
    int b = car(cdr(args))->as.i;
    return make_bool(sc, a == b);
}

static Cell *prim_quotient(Scheme *sc, Cell *args) {
    int a = car(args)->as.i;
    int b = car(cdr(args))->as.i;
    if (b == 0) {
        panic(sc, "quotient: divide by zero");
    }
    return make_int(sc, a / b);
}

static Cell *prim_modulo(Scheme *sc, Cell *args) {
    int a = car(args)->as.i;
    int b = car(cdr(args))->as.i;
    if (b == 0) {
        panic(sc, "modulo: divide by zero");
    }
    int r = a % b;
    if (r < 0) {
        r += (b < 0) ? -b : b;
    }
    return make_int(sc, r);
}

static Cell *prim_cons(Scheme *sc, Cell *args) {
    Cell *a = car(args);
    Cell *d = car(cdr(args));
    return cons(sc, a, d);
}

static Cell *prim_car(Scheme *sc, Cell *args) {
    return car(car(args));
}

static Cell *prim_cdr(Scheme *sc, Cell *args) {
    return cdr(car(args));
}

static Cell *prim_nullp(Scheme *sc, Cell *args) {
    return make_bool(sc, is_nil(sc, car(args)));
}

static Cell *prim_pairp(Scheme *sc, Cell *args) {
    return make_bool(sc, car(args)->type == T_PAIR);
}

static Cell *prim_eqp(Scheme *sc, Cell *args) {
    return make_bool(sc, car(args) == car(cdr(args)));
}

static Cell *prim_string_len(Scheme *sc, Cell *args) {
    Cell *v = car(args);
    if (v->type != T_STRING) {
        panic(sc, "string-length: expected string");
    }
    return make_int(sc, (int)v->as.str.len);
}

static Cell *prim_string_ref(Scheme *sc, Cell *args) {
    Cell *s = car(args);
    Cell *i = car(cdr(args));
    if (s->type != T_STRING || i->type != T_INT) {
        panic(sc, "string-ref: expected string and int");
    }
    if (i->as.i < 0 || (size_t)i->as.i >= s->as.str.len) {
        panic(sc, "string-ref: index out of range");
    }
    return make_char(sc, (unsigned char)s->as.str.data[i->as.i]);
}

static Cell *prim_string_eq(Scheme *sc, Cell *args) {
    Cell *a = car(args);
    Cell *b = car(cdr(args));
    if (a->type != T_STRING || b->type != T_STRING) {
        panic(sc, "string=?: expected strings");
    }
    if (a->as.str.len != b->as.str.len) {
        return scheme_false(sc);
    }
    for (size_t i = 0; i < a->as.str.len; i++) {
        if (a->as.str.data[i] != b->as.str.data[i]) {
            return scheme_false(sc);
        }
    }
    return scheme_true(sc);
}

static Cell *prim_char_eq(Scheme *sc, Cell *args) {
    Cell *a = car(args);
    Cell *b = car(cdr(args));
    if (a->type != T_CHAR || b->type != T_CHAR) {
        panic(sc, "char=?: expected chars");
    }
    return make_bool(sc, a->as.i == b->as.i);
}

static Cell *prim_list_alloc(Scheme *sc, Cell *args) {
    Cell *ncell = car(args);
    if (ncell->type != T_INT) {
        panic(sc, "list-alloc: expected int");
    }
    int n = ncell->as.i;
    if (n < 0) {
        panic(sc, "list-alloc: negative length");
    }
    Cell *list = scheme_nil(sc);
    for (int i = n - 1; i >= 0; i--) {
        push_root(sc, list);
        Cell *val = make_int(sc, i);
        push_root(sc, val);
        list = cons(sc, val, list);
        pop_roots(sc, 2);
    }
    return list;
}

static Cell *prim_list_to_string(Scheme *sc, Cell *args) {
    Cell *list = car(args);
    size_t len = 0;
    Cell *p = list;
    while (!is_nil(sc, p)) {
        Cell *ch = car(p);
        if (ch->type != T_CHAR) {
            panic(sc, "list->string: expected list of chars");
        }
        len++;
        p = cdr(p);
    }
    char *buf = alloc_str_bytes(sc, len);
    p = list;
    for (size_t i = 0; i < len; i++) {
        buf[i] = (char)car(p)->as.i;
        p = cdr(p);
    }
    buf[len] = '\0';
    Cell *c = alloc_cell(sc);
    c->type = T_STRING;
    c->as.str.data = buf;
    c->as.str.len = len;
    return c;
}

// prim_eval_string: evaluate a string in the global environment.
// Args: sc (interpreter state), args (string cell).
// Returns: int cell with number of expressions evaluated.
static Cell *prim_eval_string(Scheme *sc, Cell *args) {
    Cell *s = car(args);
    if (s->type != T_STRING) {
        panic(sc, "eval-string: expected string");
    }
    int count = eval_string_in_env(sc, s->as.str.data, sc->global_env);
    return make_int(sc, count);
}

// prim_eval_scoped: evaluate a string in a fresh environment with given bindings.
// Args: sc (interpreter state), args (alist, string).
// Returns: int cell with number of expressions evaluated.
static Cell *prim_eval_scoped(Scheme *sc, Cell *args) {
    Cell *alist = car(args);
    Cell *code = car(cdr(args));
    if (code->type != T_STRING) {
        panic(sc, "eval-scoped: expected string");
    }

    Cell *env = cons(sc, scheme_nil(sc), scheme_nil(sc));
    push_root(sc, alist);
    push_root(sc, code);
    push_root(sc, env);
    while (!is_nil(sc, alist)) {
        Cell *binding = car(alist);
        if (binding->type != T_PAIR) {
            panic(sc, "eval-scoped: invalid binding");
        }
        Cell *sym = car(binding);
        Cell *val = cdr(binding);
        if (sym->type != T_SYMBOL) {
            panic(sc, "eval-scoped: binding name must be symbol");
        }
        env_define(sc, env, sym, val);
        alist = cdr(alist);
    }

    int count = eval_string_in_env(sc, code->as.str.data, env);
    pop_roots(sc, 3);
    return make_int(sc, count);
}

static int platform_read_byte(Scheme *sc, int offset) {
    if (!sc->platform.read_byte) {
        panic(sc, "disk-read-byte: not supported");
    }
    return sc->platform.read_byte(sc->platform.user, offset);
}

static int platform_disk_size(Scheme *sc) {
    if (!sc->platform.disk_size) {
        panic(sc, "disk-size: not supported");
    }
    return sc->platform.disk_size(sc->platform.user);
}

static int platform_read_char(Scheme *sc) {
    if (!sc->platform.read_char) {
        panic(sc, "read-char: not supported");
    }
    return sc->platform.read_char(sc->platform.user);
}

static int platform_write_bytes(Scheme *sc, int offset, const char *data, int len) {
    if (!sc->platform.write_bytes) {
        panic(sc, "disk-write-bytes: not supported");
    }
    return sc->platform.write_bytes(sc->platform.user, offset, data, len);
}

static int platform_spawn_thread(Scheme *sc, const char *code) {
    if (!sc->platform.spawn_thread) {
        panic(sc, "spawn-thread: not supported");
    }
    return sc->platform.spawn_thread(sc->platform.user, code);
}

static Cell *prim_disk_read_byte(Scheme *sc, Cell *args) {
    Cell *off = car(args);
    if (off->type != T_INT) {
        panic(sc, "disk-read-byte: expected int");
    }
    return make_int(sc, platform_read_byte(sc, off->as.i));
}

static Cell *prim_disk_size(Scheme *sc, Cell *args) {
    (void)args;
    return make_int(sc, platform_disk_size(sc));
}

static Cell *prim_disk_read_bytes(Scheme *sc, Cell *args) {
    Cell *off = car(args);
    Cell *len = car(cdr(args));
    if (off->type != T_INT || len->type != T_INT) {
        panic(sc, "disk-read-bytes: expected int int");
    }
    if (len->as.i < 0) {
        panic(sc, "disk-read-bytes: negative length");
    }
    size_t n = (size_t)len->as.i;
    char *buf = alloc_str_bytes(sc, n);
    for (size_t i = 0; i < n; i++) {
        int v = platform_read_byte(sc, off->as.i + (int)i);
        if (v < 0) {
            buf[i] = 0;
        } else {
            buf[i] = (char)v;
        }
    }
    buf[n] = '\0';
    Cell *c = alloc_cell(sc);
    c->type = T_STRING;
    c->as.str.data = buf;
    c->as.str.len = n;
    return c;
}

static Cell *prim_disk_read_cstring(Scheme *sc, Cell *args) {
    Cell *off = car(args);
    Cell *maxlen = car(cdr(args));
    if (off->type != T_INT || maxlen->type != T_INT) {
        panic(sc, "disk-read-cstring: expected int int");
    }
    if (maxlen->as.i < 0) {
        panic(sc, "disk-read-cstring: negative length");
    }
    size_t n = (size_t)maxlen->as.i;
    char *buf = alloc_str_bytes(sc, n);
    size_t actual = 0;
    for (; actual < n; actual++) {
        int v = platform_read_byte(sc, off->as.i + (int)actual);
        if (v <= 0) {
            break;
        }
        buf[actual] = (char)v;
    }
    buf[actual] = '\0';
    Cell *c = alloc_cell(sc);
    c->type = T_STRING;
    c->as.str.data = buf;
    c->as.str.len = actual;
    return c;
}

// prim_disk_write_bytes: write a string to disk at an absolute offset.
// Args: sc (interpreter state), args (offset int, string).
// Returns: int cell with bytes written.
static Cell *prim_disk_write_bytes(Scheme *sc, Cell *args) {
    Cell *off = car(args);
    Cell *data = car(cdr(args));
    if (off->type != T_INT || data->type != T_STRING) {
        panic(sc, "disk-write-bytes: expected int string");
    }
    int written = platform_write_bytes(sc, off->as.i, data->as.str.data, (int)data->as.str.len);
    return make_int(sc, written);
}

// prim_spawn_thread: spawn a new Scheme thread to eval a string.
// Args: sc (interpreter state), args (code string).
// Returns: int cell with thread id or -1.
static Cell *prim_spawn_thread(Scheme *sc, Cell *args) {
    Cell *code = car(args);
    if (code->type != T_STRING) {
        panic(sc, "spawn-thread: expected string");
    }
    int tid = platform_spawn_thread(sc, code->as.str.data);
    return make_int(sc, tid);
}

static Cell *prim_char_to_int(Scheme *sc, Cell *args) {
    Cell *c = car(args);
    if (c->type != T_CHAR) {
        panic(sc, "char->int: expected char");
    }
    return make_int(sc, c->as.i);
}

static Cell *prim_int_to_char(Scheme *sc, Cell *args) {
    Cell *v = car(args);
    if (v->type != T_INT) {
        panic(sc, "int->char: expected int");
    }
    return make_char(sc, v->as.i & 0xFF);
}

// prim_read_char: read a single character from the platform.
// Args: sc (interpreter state), args (ignored).
// Returns: one-character string.
static Cell *prim_read_char(Scheme *sc, Cell *args) {
    (void)args;
    int ch = platform_read_char(sc);
    return make_char(sc, ch);
}

// prim_yield: yield the current thread (cooperative scheduling).
// Args: sc (interpreter state), args (ignored).
// Returns: int cell (0).
static Cell *prim_yield(Scheme *sc, Cell *args) {
    (void)args;
    if (sc->platform.foreign_call) {
        sc->platform.foreign_call("yield", 0, NULL);
    }
    return make_int(sc, 0);
}

static Cell *prim_display(Scheme *sc, Cell *args) {
    Cell *v = car(args);
    if (v->type == T_INT) {
        int n = v->as.i;
        char buf[12];
        int i = 0;
        if (n == 0) {
            putc_out(sc, '0');
            return scheme_nil(sc);
        }
        if (n < 0) {
            putc_out(sc, '-');
            n = -n;
        }
        while (n > 0 && i < 11) {
            buf[i++] = (char)('0' + (n % 10));
            n /= 10;
        }
        while (i > 0) {
            putc_out(sc, buf[--i]);
        }
    } else if (v->type == T_SYMBOL) {
        const char *p = v->as.sym.name;
        while (*p) {
            putc_out(sc, *p++);
        }
    } else if (v->type == T_STRING) {
        const char *p = v->as.str.data;
        size_t len = v->as.str.len;
        for (size_t i = 0; i < len; i++) {
            putc_out(sc, p[i]);
        }
    } else if (v->type == T_CHAR) {
        putc_out(sc, (char)v->as.i);
    } else if (is_nil(sc, v)) {
        putc_out(sc, '(');
        putc_out(sc, ')');
    } else if (v == scheme_true(sc)) {
        putc_out(sc, '#');
        putc_out(sc, 't');
    } else if (v == scheme_false(sc)) {
        putc_out(sc, '#');
        putc_out(sc, 'f');
    }
    return scheme_nil(sc);
}

static Cell *prim_newline(Scheme *sc, Cell *args) {
    (void)args;
    putc_out(sc, '\n');
    return scheme_nil(sc);
}

static Cell *prim_foreign_call(Scheme *sc, Cell *args) {
    if (is_nil(sc, args)) {
        panic(sc, "foreign-call: missing symbol");
    }
    Cell *name_cell = car(args);
    if (name_cell->type != T_SYMBOL) {
        panic(sc, "foreign-call: name must be symbol");
    }
    if (!sc->platform.foreign_call) {
        panic(sc, "foreign-call: not supported");
    }

    int argv[8];
    int argc = 0;
    args = cdr(args);
    while (!is_nil(sc, args)) {
        if (argc >= (int)(sizeof(argv) / sizeof(argv[0]))) {
            panic(sc, "foreign-call: too many args");
        }
        Cell *v = car(args);
        if (v->type != T_INT) {
            panic(sc, "foreign-call: args must be int");
        }
        argv[argc++] = v->as.i;
        args = cdr(args);
    }

    int ret = sc->platform.foreign_call(name_cell->as.sym.name, argc, argv);
    return make_int(sc, ret);
}

static void add_prim(Scheme *sc, const char *name, PrimFn fn) {
    Cell *sym = intern_symbol(sc, name);
    Cell *prim = make_prim(sc, fn);
    env_define(sc, sc->global_env, sym, prim);
}

// scheme_init: initialize interpreter state, heap, buffers, and primitives.
// Args: sc (interpreter state), cfg (configuration pointers/sizes).
// Returns: none.
void scheme_init(Scheme *sc, const SchemeConfig *cfg) {
    sc->heap = cfg->heap;
    sc->heap_cells = cfg->heap_cells;
    sc->sym_buf = cfg->sym_buf;
    sc->sym_buf_size = cfg->sym_buf_size;
    sc->sym_buf_used = 0;
    sc->str_buf = cfg->str_buf;
    sc->str_buf_size = cfg->str_buf_size;
    sc->str_buf_used = 0;
    sc->platform = cfg->platform;
    sc->root_top = 0;
    sc->env_top = 0;

    sc->nil_cell.type = T_NIL;
    sc->true_cell.type = T_BOOL;
    sc->true_cell.as.b = 1;
    sc->false_cell.type = T_BOOL;
    sc->false_cell.as.b = 0;

    sc->free_list = NULL;
    for (size_t i = 0; i < sc->heap_cells; i++) {
        Cell *c = &sc->heap[i];
        c->type = T_PAIR;
        c->as.pair.cdr = sc->free_list;
        sc->free_list = c;
    }

    sc->interned_syms = scheme_nil(sc);
    sc->global_env = cons(sc, scheme_nil(sc), scheme_nil(sc));
    sc->current_env = sc->global_env;

    add_prim(sc, "+", prim_add);
    add_prim(sc, "-", prim_sub);
    add_prim(sc, "*", prim_mul);
    add_prim(sc, "<", prim_lt);
    add_prim(sc, "=", prim_num_eq);
    add_prim(sc, "quotient", prim_quotient);
    add_prim(sc, "modulo", prim_modulo);
    add_prim(sc, "cons", prim_cons);
    add_prim(sc, "car", prim_car);
    add_prim(sc, "cdr", prim_cdr);
    add_prim(sc, "null?", prim_nullp);
    add_prim(sc, "pair?", prim_pairp);
    add_prim(sc, "eq?", prim_eqp);
    add_prim(sc, "string-length", prim_string_len);
    add_prim(sc, "string-ref", prim_string_ref);
    add_prim(sc, "string=?", prim_string_eq);
    add_prim(sc, "char=?", prim_char_eq);
    add_prim(sc, "char->int", prim_char_to_int);
    add_prim(sc, "int->char", prim_int_to_char);
    add_prim(sc, "list-alloc", prim_list_alloc);
    add_prim(sc, "list->string", prim_list_to_string);
    add_prim(sc, "eval-string", prim_eval_string);
    add_prim(sc, "eval-scoped", prim_eval_scoped);
    add_prim(sc, "disk-read-byte", prim_disk_read_byte);
    add_prim(sc, "disk-read-bytes", prim_disk_read_bytes);
    add_prim(sc, "disk-read-cstring", prim_disk_read_cstring);
    add_prim(sc, "disk-write-bytes", prim_disk_write_bytes);
    add_prim(sc, "disk-size", prim_disk_size);
    add_prim(sc, "read-char", prim_read_char);
    add_prim(sc, "spawn-thread", prim_spawn_thread);
    add_prim(sc, "yield", prim_yield);
    add_prim(sc, "display", prim_display);
    add_prim(sc, "newline", prim_newline);
    add_prim(sc, "foreign-call", prim_foreign_call);
}

int scheme_eval_string(Scheme *sc, const char *input) {
    return eval_string_in_env(sc, input, sc->global_env);
}

static int eval_string_in_env(Scheme *sc, const char *input, Cell *env) {
    const char *p = input;
    int count = 0;
    while (1) {
        Cell *expr = read_expr(sc, &p);
        if (!expr) {
            break;
        }
        push_root(sc, expr);
        eval(sc, expr, env);
        pop_roots(sc, 1);
        count++;
    }
    return count;
}
