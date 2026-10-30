/* Cortex-M7 start for QEMU's mps2-an500: vectors, FPU on, newlib's _start
 * (rdimon: semihosting for printf / files / exit). */
#include <stdint.h>

extern void _start(void);
extern uint32_t __stack_top;

static void fault(void)
{
    /* semihosting SYS_WRITE0 then SYS_EXIT */
    static const char msg[] = "qemu: FAULT\n";
    register int r0 __asm__("r0") = 4;
    register const char *r1 __asm__("r1") = msg;
    __asm__ volatile("bkpt 0xab" : : "r"(r0), "r"(r1) : "memory");
    volatile uint32_t *scb = (volatile uint32_t *)0xE000ED28; /* CFSR */
    (void)scb;
    for (;;) {
        register int a0 __asm__("r0") = 0x18;
        register int a1 __asm__("r1") = 0x20024;
        __asm__ volatile("bkpt 0xab" : : "r"(a0), "r"(a1) : "memory");
    }
}

void Reset_Handler(void)
{
    *(volatile uint32_t *)0xE000ED88 |= 0xF << 20; /* CPACR: CP10, CP11 */
    __asm__ volatile("dsb; isb");
    _start();
}

__attribute__((section(".vectors"), used)) const void *const vectors[16] = {
    &__stack_top, (void *)Reset_Handler, (void *)fault, (void *)fault, (void *)fault, (void *)fault, (void *)fault,
};
