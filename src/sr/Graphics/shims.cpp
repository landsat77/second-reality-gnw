#define _CRT_SECURE_NO_WARNINGS

#include "shims.h"

#include "Parts/Common.h"

namespace Shim
{
    unsigned char vram[VRAM_X * VRAM_Y] = { 0 };
    unsigned int palette[PaletteColorCount] = { 0 };
    unsigned int startpixel = 0;
#ifdef SR_GNW
    unsigned int ringsize = 0;
#endif

    namespace
    {
        int paletteReadIndex = 0;
        int paletteReadComponent = 0;
        int paletteIndex = 0;
        int paletteComponent = 0;

        bool isFirstPart = 1;
    }

#ifdef HOST_BUILD
    extern "C" int host_sr_vram_clear(void);
#endif

    void clearScreen()
    {
#ifdef HOST_BUILD
        if (host_sr_vram_clear()) return; // SR_VRAM_PROBE
#endif
        CLEAR(vram);
    }

    void setpal(int idx, unsigned char r, unsigned char g, unsigned char b)
    {
        palette[idx] = (b << 2) | (g << 10) | (r << 18);
    }

    void outp(int reg, unsigned int value)
    {
        switch (reg)
        {
            case 0x3c7: {
                paletteReadIndex = value;
            }
            break;
            case 0x3c8: {
                paletteIndex = value & 0xFF;
                paletteComponent = 0;
            }
            break;
            case 0x3c9: {
                unsigned char * pal8 = (unsigned char *)palette;
                pal8[paletteIndex * 4 + (2 - paletteComponent)] = static_cast<unsigned char>(value << 2);
                paletteComponent++;
                if (paletteComponent == 3)
                {
                    paletteIndex++;
                    paletteComponent = 0;
                }
                if (paletteIndex >= PaletteColorCount)
                {
                    paletteIndex = 0;
                }
            }
            break;
        }
    }

    unsigned char inp(int reg)
    {
        unsigned char result = 0;

        switch (reg)
        {
            case 0x3c9: {
                unsigned char * pal8 = (unsigned char *)palette;
                result = pal8[paletteReadIndex * 4 + (2 - paletteReadComponent)] >> 2;

                paletteReadComponent++;

                if (paletteReadComponent == 3)
                {
                    paletteReadIndex++;
                    paletteReadComponent = 0;
                }

                if (paletteReadIndex > PaletteColorCount)
                {
                    paletteReadIndex = 0;
                }
            }
            break;
        }

        return result;
    }

    void setstartpixel(int reg)
    {
        startpixel = reg;
    }

    bool isDemoFirstPart()
    {
        return isFirstPart;
    }

    void finishedDemoFirstPart()
    {
        isFirstPart = false;
    }
}
