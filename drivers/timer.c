#include "timer.h"
#include "../cpu/irq.h"
#include "../cpu/ports.h"
#include "../sched/sched.h"

#define PIT_FREQUENCY   1193180
#define TIMER_HZ        100

static volatile uint32_t ticks = 0;

static void timer_callback(registers_t* regs) {
    (void)regs;
    ticks++;
    sched_tick();
}

void timer_install(void) {
    irq_install_handler(0, timer_callback);

    uint32_t divisor = PIT_FREQUENCY / TIMER_HZ;
    outb(0x43, 0x36);                               /* canal 0, lsb+msb, modo 3 */
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t timer_get_ticks(void) {
    return ticks;
}

void timer_sleep(uint32_t ms) {
    uint32_t end = ticks + (ms * TIMER_HZ / 1000) + 1;
    while (ticks < end)
        __asm__ volatile("hlt");
}
