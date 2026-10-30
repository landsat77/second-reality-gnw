/*
 * Between the demo (C++, src/sr + sr_glue.cpp) and the console (C,
 * sr_main.c): the 70 Hz clock, the picture, the sound.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SR_REFRESH_HZ 70 /* VGA 320x200 / 320x400: 70 Hz (the LCD runs at 70 Hz too) */

/* --- console side (sr_main.c) --- */

/* VBLs (70 Hz) since start-up */
uint32_t sr_plat_vbl(void);
/* waits for that VBL (watchdog, sound, buttons, pause menu meanwhile) */
void sr_plat_wait_vbl(uint32_t vbl);
/* watchdog, sound, buttons, pause menu (cheap if called often) */
void sr_plat_service(void);

/* the picture: VGA memory (8 bits, `size` bytes) from `start`, w x h
 * (320x200, 320x400 or 640x350), the palette (0x00RRGGBB); if `ring`, the
 * lines wrap after `ring` bytes (the end scroller) */
void sr_plat_present(const uint8_t *vram, uint32_t size, uint32_t start, uint32_t ring, int w, int h,
                     const uint32_t *palette);

/* samples heard since sr_plat_audio_reset() (SR_AUDIO_RATE) */
uint32_t sr_plat_audio_pos(void);
void sr_plat_audio_reset(void);
/* tops the sound up (sr_audio_render() is called for the new samples) */
void sr_plat_audio_feed(void);

/* --- demo side (sr_glue.cpp) --- */

/* the next n samples of the music (mono) */
void sr_audio_render(int16_t *out, int n);

/* parts of the demo */
#define SR_PART_COUNT 19
const char *sr_part_name(int part);
/* runs the demo from that part; returns at the end */
void sr_demo_run(int part);
/* the part being played */
int sr_demo_part(void);
/* asks the demo to start again at that part (from the console's loop) */
void sr_demo_jump(int part);
/* called from sr_plat_service(): goes to the part asked by sr_demo_jump() */
void sr_demo_check_jump(void);

/* memory of the parts (sr_overlay.c / host: no overlays) */
void sr_overlay_load(int group);

#ifdef __cplusplus
}
#endif
