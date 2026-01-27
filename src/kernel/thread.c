#include "thread.h"
#include "ports.h"

#define MAX_THREADS 8
#define STACK_SIZE 4096

typedef enum {
    THREAD_UNUSED,
    THREAD_RUNNABLE,
    THREAD_SLEEPING
} thread_state;

typedef struct Thread {
    unsigned int *esp;
    thread_state state;
    unsigned int sleep_ticks;
    thread_fn fn;
    void *arg;
} Thread;

static Thread threads[MAX_THREADS];
static int current_thread = 0;

extern void context_switch(unsigned int **old_esp, unsigned int *new_esp);
extern void thread_start(void);

static void schedule_next(void);

void thread_init(void) {
    for (int i = 0; i < MAX_THREADS; i++) {
        threads[i].esp = 0;
        threads[i].state = THREAD_UNUSED;
        threads[i].sleep_ticks = 0;
        threads[i].fn = 0;
        threads[i].arg = 0;
    }
    threads[0].state = THREAD_RUNNABLE;
    threads[0].esp = 0;
    current_thread = 0;
}

int thread_spawn(thread_fn fn, void *arg) {
    static unsigned char stacks[MAX_THREADS][STACK_SIZE];

    for (int i = 1; i < MAX_THREADS; i++) {
        if (threads[i].state == THREAD_UNUSED) {
            unsigned int *stack_top = (unsigned int *)(stacks[i] + STACK_SIZE);

            *(--stack_top) = (unsigned int)thread_start; /* return addr */
            *(--stack_top) = 0; /* saved ebp */
            *(--stack_top) = 0; /* saved ebx */
            *(--stack_top) = 0; /* saved esi */
            *(--stack_top) = 0; /* saved edi */

            threads[i].esp = stack_top;
            threads[i].state = THREAD_RUNNABLE;
            threads[i].sleep_ticks = 0;
            threads[i].fn = fn;
            threads[i].arg = arg;
            return i;
        }
    }
    return -1;
}

void thread_yield(void) {
    schedule_next();
}

void thread_sleep(unsigned int ticks) {
    threads[current_thread].sleep_ticks = ticks;
    threads[current_thread].state = THREAD_SLEEPING;
    schedule_next();
}

void scheduler_tick(void) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i].state == THREAD_SLEEPING && threads[i].sleep_ticks > 0) {
            threads[i].sleep_ticks--;
            if (threads[i].sleep_ticks == 0) {
                threads[i].state = THREAD_RUNNABLE;
            }
        }
    }
}

void timer_tick(void) {
    scheduler_tick();
    outb(0x20, 0x20);
}

void thread_exit(void) {
    threads[current_thread].state = THREAD_UNUSED;
    schedule_next();
    for (;;) {
    }
}

void thread_start(void) {
    Thread *t = &threads[current_thread];
    if (t->fn) {
        t->fn(t->arg);
    }
    thread_exit();
}

int thread_active_count(void) {
    int count = 0;
    for (int i = 1; i < MAX_THREADS; i++) {
        if (threads[i].state != THREAD_UNUSED) {
            count++;
        }
    }
    return count;
}

static void schedule_next(void) {
    int next = current_thread;
    for (int i = 0; i < MAX_THREADS; i++) {
        next = (next + 1) % MAX_THREADS;
        if (threads[next].state == THREAD_RUNNABLE) {
            if (next == current_thread) {
                return;
            }
            int prev = current_thread;
            current_thread = next;
            context_switch(&threads[prev].esp, threads[next].esp);
            return;
        }
    }
}
