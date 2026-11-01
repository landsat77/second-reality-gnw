/*
 * Host build of Second Reality: the console's clock, LCD frame counter and
 * sound ring (pcm_*), in real time (SDL window) or, with SR_FAST=1, in a
 * virtual time that only moves when the demo waits (deterministic runs).
 *
 * Environment (tests):
 *   SR_FAST=1          virtual time, no pacing (and no window: set
 *                      SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy)
 *   SR_SECONDS=n       quit after n seconds (demo time)
 *   SR_START=n         first part (0..18)
 *   SR_HASH=file       a line per picture shown: VBL, part, CRC-32 of the LCD
 *   SR_SHOTS=dir       PPM pictures of the LCD, every SR_SHOT_EVERY VBLs
 *                      (default 70), or at SR_SHOT_AT="vbl,vbl,..."
 *   SR_WAV=file        the sound (WAV, mono)
 *   SR_JUMP="vbl:part,..." go to a part (as from the pause menu)
 *   SR_TITLE=1         the title screen (else the demo starts at once)
 *   SR_KEYS="vbl:keys,..." buttons held from that VBL (U D L R A B G=GAME
 *                      T=TIME, - = none) on the title screen
 */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
void host_sr_on_present(void);
void host_sr_idle(void);
void host_sr_service(void);
struct odroid_gamepad_state;
void host_sr_keys(void *joystick);
#ifdef __cplusplus
}
#endif
