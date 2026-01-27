#ifndef SLOPOS_THREAD_H
#define SLOPOS_THREAD_H

typedef void (*thread_fn)(void *arg);

void thread_init(void);
int thread_spawn(thread_fn fn, void *arg);
void thread_yield(void);
void thread_sleep(unsigned int ticks);
void scheduler_tick(void);
void thread_exit(void);
void timer_tick(void);
int thread_active_count(void);

#endif
