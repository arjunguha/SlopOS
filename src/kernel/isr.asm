BITS 32
GLOBAL isr_timer_stub
EXTERN timer_tick

isr_timer_stub:
    pusha
    call timer_tick
    popa
    iretd
