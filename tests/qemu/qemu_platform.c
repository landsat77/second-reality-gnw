/* host_platform.h for QEMU (no SDL): the clock is the CMSDK timer 0
 * (25 MHz), nothing is shown or heard. With -icount shift=2 one instruction
 * takes 4 ns (250 MIPS, a guess for the console's 280 MHz Cortex-M7). */
#include <stdint.h>
#include <string.h>
#include "host_platform.h"

#define TIMER0 ((volatile uint32_t *)0x40000000)

static uint64_t ticks;
static uint32_t last;
static int started;

uint64_t host_platform_ticks_us(void)
{
    if (!started) {
        TIMER0[2] = 0xFFFFFFFF; /* RELOAD */
        TIMER0[1] = 0xFFFFFFFF; /* VALUE */
        TIMER0[0] = 1;          /* CTRL: enable */
        last = 0xFFFFFFFF;
        started = 1;
    }
    uint32_t v = TIMER0[1];
    ticks += (uint32_t)(last - v);
    last = v;
    return ticks / 25;
}

int host_platform_init(const char *title, int scale) { (void)title; (void)scale; host_platform_ticks_us(); return 0; }
void host_platform_shutdown(void) {}
int host_platform_poll(host_pad_t *pad) { (void)pad; return 1; }
void host_platform_present_rgb565(const uint16_t *fb, int width, int height) { (void)fb; (void)width; (void)height; }
void host_platform_delay_ms(uint32_t ms)
{
    uint64_t end = host_platform_ticks_us() + ms * 1000ull;
    while (host_platform_ticks_us() < end)
        ;
}
uint32_t host_platform_ticks_ms(void) { return (uint32_t)(host_platform_ticks_us() / 1000); }
int host_platform_audio_start(int rate, int half_len) { (void)rate; (void)half_len; return 0; }
void host_platform_audio_queue(const int16_t *mono, int samples) { (void)mono; (void)samples; }
void host_platform_audio_stop(void) {}
int host_platform_audio_queued_samples(void) { return 0; }

/* no file system calls of POSIX in newlib: what host_*.c use */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
int mkdir(const char *path, mode_t mode) { (void)path; (void)mode; return 0; }

/* the environment of the tests: qemu_env.txt (NAME=value lines), read
 * through semihosting before main() */
__attribute__((constructor)) static void read_env(void)
{
    FILE *f = fopen("qemu_env.txt", "r");
    if (!f)
        return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = 0;
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#')
            continue;
        *eq = 0;
        setenv(line, eq + 1, 1);
    }
    fclose(f);
}
