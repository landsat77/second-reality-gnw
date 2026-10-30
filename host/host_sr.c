#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "host_compat.h"
#include "host_platform.h"
#include "host_sr.h"
#include "gw_lcd.h"
#include "gw_audio.h"
#include "sr_platform.h"


static int fast = -1;
static uint64_t vtime_us;   /* virtual time (SR_FAST) */
static uint64_t t0_ms;
static uint32_t rate = 44100;

static void init(void)
{
    if (fast >= 0)
        return;
    fast = getenv("SR_FAST") && atoi(getenv("SR_FAST"));
    t0_ms = host_platform_ticks_ms();
}

static uint64_t now_us(void)
{
    init();
    if (fast)
        return vtime_us;
    return (uint64_t)(host_platform_ticks_ms() - t0_ms) * 1000;
}

/* --- LCD at 70 Hz --- */

uint32_t lcd_get_frame_counter(void)
{
    return (uint32_t)(now_us() * SR_REFRESH_HZ / 1000000);
}

/* as on the console: a picture is shown at the next VBL, until then the
 * swap is pending */
static uint32_t swap_vbl = 0xFFFFFFFF;

void host_sr_swapped(void)
{
    swap_vbl = lcd_get_frame_counter();
}

uint32_t lcd_is_swap_pending(void)
{
    return lcd_get_frame_counter() == swap_vbl;
}

void host_sr_idle(void);

bool lcd_sleep_while_swap_pending(void)
{
    while (lcd_is_swap_pending())
        host_sr_idle();
    return true;
}

/* --- sound ring (the firmware's pcm_fill, driven by the clock) --- */

static int16_t *ring;
static int ring_mask;
static volatile uint16_t *ring_head, *ring_tail;
static int pcm_on, pcm_silent, pcm_vol = 255;
static uint32_t pcm_played;
static uint64_t pcm_done; /* samples of time already handled */
static FILE *wav;
static uint32_t wav_samples;

void pcm_attach(int16_t *r, int size, volatile uint16_t *head, volatile uint16_t *tail)
{
    ring = r;
    ring_mask = size - 1;
    ring_head = head;
    ring_tail = tail;
}

void pcm_audio_enable(int on) { pcm_on = on; }
void pcm_audio_set(int vol, int play)
{
    pcm_vol = vol;
    pcm_silent = !play;
}
void pcm_audio_setpos(uint32_t samples) { pcm_played = samples; }

static void wav_header(void)
{
    uint32_t data = wav_samples * 2;
    uint8_t h[44];
    memcpy(h, "RIFF", 4);
    uint32_t v = 36 + data;
    memcpy(h + 4, &v, 4);
    memcpy(h + 8, "WAVEfmt ", 8);
    v = 16;
    memcpy(h + 16, &v, 4);
    uint16_t s = 1;
    memcpy(h + 20, &s, 2);
    memcpy(h + 22, &s, 2);
    memcpy(h + 24, &rate, 4);
    v = rate * 2;
    memcpy(h + 28, &v, 4);
    s = 2;
    memcpy(h + 32, &s, 2);
    s = 16;
    memcpy(h + 34, &s, 2);
    memcpy(h + 36, "data", 4);
    memcpy(h + 40, &data, 4);
    fseek(wav, 0, SEEK_SET);
    fwrite(h, 1, 44, wav);
    fseek(wav, 0, SEEK_END);
}

static void pcm_run(void)
{
    uint64_t due = now_us() * rate / 1000000;
    if (!wav && getenv("SR_WAV")) {
        wav = fopen(getenv("SR_WAV"), "wb");
        if (wav)
            wav_header();
    }
    int16_t buf[1024];
    while (pcm_done < due) {
        int n = (int)(due - pcm_done > 1024 ? 1024 : due - pcm_done);
        pcm_done += n;
        if (!pcm_on || !ring)
            continue;
        if (pcm_silent) {
            memset(buf, 0, n * 2);
        } else {
            uint16_t tail = *ring_tail, head = *ring_head;
            for (int i = 0; i < n; i++) {
                int16_t s = 0;
                if (tail != head) {
                    s = ring[tail];
                    tail = (uint16_t)((tail + 1) & ring_mask);
                }
                buf[i] = (int16_t)((s * pcm_vol) >> 8);
            }
            *ring_tail = tail;
            pcm_played += n;
        }
        if (wav) {
            fwrite(buf, 2, n, wav);
            wav_samples += n;
        }
        if (!fast)
            host_platform_audio_queue(buf, n);
    }
}

uint32_t pcm_audio_pos(void)
{
    pcm_run();
    return pcm_played;
}

/* --- tests --- */

static bool in_list(const char *list, long value)
{
    for (const char *p = list; p && *p;) {
        if (atol(p) == value)
            return true;
        p = strchr(p, ',');
        if (p)
            p++;
    }
    return false;
}

static void check_end(void)
{
    const char *s = getenv("SR_SECONDS");
    if (s && now_us() >= (uint64_t)(atof(s) * 1e6)) {
        if (wav) {
            wav_header();
            fclose(wav);
        }
        printf("host: %s s, end\n", s);
        exit(0);
    }
}

void host_sr_service(void)
{
    init();
    if (fast)
        vtime_us += 50; /* the demo's work takes time too (polling loops) */
    pcm_run();
    host_poll_events();

    /* SR_JUMP="vbl:part,..." */
    static const char *jump;
    if (!jump)
        jump = getenv("SR_JUMP") ? getenv("SR_JUMP") : "";
    if (*jump) {
        long at = atol(jump);
        if ((long)lcd_get_frame_counter() >= at) {
            const char *c = strchr(jump, ':');
            if (c)
                sr_demo_jump(atoi(c + 1));
            const char *n = strchr(jump, ',');
            jump = n ? n + 1 : "";
        }
    }
    check_end();
}

void host_sr_idle(void)
{
    init();
    if (fast) {
        /* to the next VBL */
        uint64_t f = now_us() * SR_REFRESH_HZ / 1000000 + 1;
        uint64_t t = (f * 1000000 + SR_REFRESH_HZ - 1) / SR_REFRESH_HZ;
        if (t > vtime_us)
            vtime_us = t;
    } else {
        host_platform_delay_ms(1);
    }
    pcm_run();
    check_end();
}

void host_sr_on_present(void)
{
    init();
    if (fast)
        vtime_us += 1000;
}

/* the picture is on the LCD (called by lcd_swap) */
void host_sr_shown(const uint16_t *fb)
{
    static FILE *hash;
    static long last_shot = -1;
    long vbl = (long)lcd_get_frame_counter();
    if (!hash && getenv("SR_HASH"))
        hash = fopen(getenv("SR_HASH"), "w");
    if (hash) {
        fprintf(hash, "%ld %d %08x\n", vbl, sr_demo_part(),
                (unsigned)crc32_le(0, (const uint8_t *)fb, 320 * 240 * 2));
        fflush(hash);
    }
    const char *dir = getenv("SR_SHOTS");
    if (dir) {
        long every = getenv("SR_SHOT_EVERY") ? atol(getenv("SR_SHOT_EVERY")) : 70;
        bool take;
        if (getenv("SR_SHOT_AT"))
            take = in_list(getenv("SR_SHOT_AT"), vbl) && vbl != last_shot;
        else
            take = every > 0 && vbl / every != last_shot / every;
        if (take) {
            last_shot = vbl;
            char path[300];
            snprintf(path, sizeof(path), "%s/shot_%06ld.ppm", dir, vbl);
            FILE *f = fopen(path, "wb");
            if (f) {
                fprintf(f, "P6\n320 240\n255\n");
                for (int i = 0; i < 320 * 240; i++) {
                    uint8_t rgb[3] = { (uint8_t)((fb[i] >> 11) << 3), (uint8_t)(((fb[i] >> 5) & 63) << 2),
                                       (uint8_t)((fb[i] & 31) << 3) };
                    fwrite(rgb, 1, 3, f);
                }
                fclose(f);
            }
        }
    }
}

/* --- firmware functions not in host_emu.c --- */
#include <sys/stat.h>
bool rg_storage_mkdir(const char *dir)
{
    char path[512];
    snprintf(path, sizeof(path), "host_saves/%s", dir);
    for (char *p = path + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(path, 0755);
            *p = '/';
        }
    }
    mkdir(path, 0755);
    return true;
}

void odroid_system_switch_app(int app)
{
    (void)app;
    printf("host: back to the launcher\n");
    exit(0);
}
