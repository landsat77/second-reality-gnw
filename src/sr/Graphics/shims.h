#pragma once

#include <stddef.h>

constexpr const float Default_JSSS = 0.80f;

#include "Blob/Blob.h"

void demo_blit();
void demo_changemode(int x, int y, float jsss = Default_JSSS);

namespace Shim
{
    constexpr const size_t VRAM_X = 640;
#ifdef SR_GNW
    // the VGA's 256 KB (two 320x400 pages), and the end scroller's ring of
    // 401 lines of 640 (+1 line: the picture is filtered over 2 lines)
    constexpr const size_t VRAM_Y = 402;
    // the end scroller: the lines of the picture wrap after `ringsize` bytes
    extern unsigned int ringsize;
    // if dispstride: the lines shown are at dispstart + n * dispstride (each
    // twice if dispdouble) instead of startpixel + n * width
    extern unsigned int dispstart, dispstride, dispdouble;

    // The VGA memory above 128000 bytes (one 320x400 page) is not used by
    // parts 0-13 (320x200 / 320x400, one page): their big buffers can live
    // there (SR_SCRATCH). sr_demo_run() zeroes it before these parts.
    constexpr const size_t SCRATCH_OFFSET = 128000;
    constexpr const size_t SCRATCH_SIZE = VRAM_X * VRAM_Y - SCRATCH_OFFSET;
#define SR_SCRATCH(type, offset) (*reinterpret_cast<type *>(Shim::vram + Shim::SCRATCH_OFFSET + (offset)))
#define SR_SCRATCH_CHECK(type, offset) \
    static_assert((offset) + sizeof(type) <= Shim::SCRATCH_SIZE, "SR_SCRATCH: too big")
#else
    constexpr const size_t VRAM_Y = 960;
#endif

    extern unsigned int palette[];
    extern unsigned int startpixel;
    extern unsigned char vram[];

    void clearScreen();

    void setpal(int idx, unsigned char r, unsigned char g, unsigned char b);
    void outp(int reg, unsigned int value);
    unsigned char inp(int reg);
    void setstartpixel(int reg);

    bool isDemoFirstPart();
    void finishedDemoFirstPart();
}