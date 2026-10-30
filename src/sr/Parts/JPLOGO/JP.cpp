#include "Parts/Common.h"

#include "JP_Data.h"
#include "JP_Zoom_Data.h"

namespace JPLogo
{
    struct AXState
    {
        uint8_t AL;
        uint8_t AH;
    };

    namespace
    {
        AXState g_ax = { 0, 0 };

        char rowdata1[200][186];
        char rowdata2[200][186];
        char * row[400];

        Palette pal2{};
        Palette palette{};
        char rowbuf[640]{};

        int framey1[200]{};
        int framey2[200]{};
        int framey1t[800]{};
        int framey2t[800]{};
        int lasty[400]{};
        int lasts[400]{};

        unsigned int lens_current_plane_mask = 0;

        unsigned char * lens_scanline_chunky = 0;
        unsigned char lens_scanline_plane_buffer[PLANAR_WIDTH]{};

        unsigned char * lens_scanline_plane_bufferptr = lens_scanline_plane_buffer;
    }

    void scrolly(int y)
    {
        Shim::setstartpixel(y * SCREEN_WIDTH);
    }

    void jl_set_ax_seed(uint8_t al, uint8_t ah)
    {
        g_ax.AL = al;
        g_ax.AH = ah;
    }

    void jl_set_plane_mask_seed(unsigned int m)
    {
        lens_current_plane_mask = m;
    }

    static inline void mov_al_mem(const uint8_t * src, size_t off)
    {
        g_ax.AL = src[off];
    }

    static inline void mov_ah_mem(const uint8_t * src, size_t off)
    {
        g_ax.AH = src[off];
    }

    static inline void xor_al_al()
    {
        g_ax.AL = 0;
    }

    static inline void xor_ah_ah()
    {
        g_ax.AH = 0;
    }

    static inline void store_ax(uint8_t * dst, size_t off)
    {
        *((uint16_t *)&dst[off]) = (uint16_t)(g_ax.AL | (g_ax.AH << 8));
    }

    static inline void store_al(uint8_t * dst, size_t off)
    {
        dst[off] = g_ax.AL;
    }

    static inline void copy_stride4(uint8_t * dst, const uint8_t * src, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
        {
            *dst = *src++;
            dst += 4;
        }
    }

    void lens_demux_scanlineC(unsigned int nextMask)
    {
        unsigned int oldMask = lens_current_plane_mask;

        if (oldMask & 1) copy_stride4(lens_scanline_chunky + 0, lens_scanline_plane_bufferptr, PLANAR_WIDTH);
        if (oldMask & 2) copy_stride4(lens_scanline_chunky + 1, lens_scanline_plane_bufferptr, PLANAR_WIDTH);
        if (oldMask & 4) copy_stride4(lens_scanline_chunky + 2, lens_scanline_plane_bufferptr, PLANAR_WIDTH);
        if (oldMask & 8) copy_stride4(lens_scanline_chunky + 3, lens_scanline_plane_bufferptr, PLANAR_WIDTH);

        lens_current_plane_mask = nextMask;
    }

    // The 54 unrolled zoom functions of Second Reality++ (zoom0 ... zoom244,
    // ~76 KB of code on the console) are a table of operations here
    // (JP_Zoom_Data, in the pack: tools/jp_zoom_gen.py), run by zoom_run().
    void zoom_run(int number, uint8_t * src, uint8_t * dst)
    {
        static int8_t index[256];
        static bool ready;
        if (!ready)
        {
            memset(index, -1, sizeof(index));
            for (int i = 0; i < 54; i++) index[Data::zoom_name[i]] = static_cast<int8_t>(i);
            ready = true;
        }
        const int i = index[number];
        const uint16_t * op = &Data::zoom_ops[Data::zoom_start[i]];
        const uint16_t * end = &Data::zoom_ops[Data::zoom_start[i + 1]];
        for (; op < end; op++)
        {
            const unsigned v = *op & 0xFFF;
            switch (*op >> 12)
            {
                case 0: store_ax(dst, v); break;
                case 1: mov_al_mem(src, v); break;
                case 2: mov_ah_mem(src, v); break;
                case 3: lens_demux_scanlineC(g_ax.AL); break;
                case 4: lens_demux_scanlineC(v); break;
                case 5: g_ax.AL = static_cast<uint8_t>(v); break;
                case 6: store_al(dst, v); break;
                case 7: dst[v] = g_ax.AH; break;
                case 8: xor_al_al(); break;
                default: xor_ah_ah(); break;
            }
        }
    }

    // ZoomTable of Second Reality++: the zoom function of each zoom / 2
    static const uint8_t ZoomTable[] = {
        0, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 142, 144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180, 182, 184, 186, 188, 190, 192, 194, 196, 198, 200, 202, 204, 206, 208, 210, 212, 214, 216, 218, 220, 222, 224, 226, 228, 230, 232, 234, 236, 238, 240, 242, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244, 244
    };


    void linezoomC(uint8_t * dst, uint8_t * src, int zoom)
    {
        if (zoom > 318)
        {
            zoom = 318;
        }

        jl_set_ax_seed(0, 0);
        jl_set_plane_mask_seed(0);

        if (size_t zoomIndex = zoom / 2; zoomIndex < COUNT(ZoomTable))
        {
            lens_demux_scanlineC(15);

            zoom_run(ZoomTable[zoomIndex], src, dst);

            lens_demux_scanlineC(0);
        }
    }

    void doit()
    {
        int frame = 0;
        int a, b, c, y, xsc, y1 = 0, y2 = 0;

        if (!Shim::isDemoFirstPart())
        {
            while (!demo_wantstoquit() && Music::getPlusFlags() < 4)
            {
                AudioPlayer::Update(true);
            }
        }

        while (!demo_wantstoquit() && frame < 700)
        {
            c = demo_vsync();

            frame += c;
            if (frame > 511)
                c = 400;
            else
            {
                y1 = framey1t[frame] / 16;
                y2 = framey2t[frame] / 16;
            }

            xsc = (400 - (y2 - y1)) / 8;

            for (y = 0; y < 400; y++)
            {
                if (y < y1 || y >= y2)
                {
                    lens_scanline_chunky = Shim::vram + y * SCREEN_WIDTH;
                    linezoomC(lens_scanline_plane_buffer, nullptr, 0);
                }
                else
                {
                    b = (int32_t)(y - y1) * 400L / (int32_t)(y2 - y1);
                    a = 184 + (Common::Data::sin1024[b * 32 / 25] * xsc + 32) / 64;
                    a &= ~1;

                    if (lasty[y] != b || lasts[y] != a)
                    {
                        lens_scanline_chunky = Shim::vram + y * SCREEN_WIDTH;
                        linezoomC(lens_scanline_plane_buffer, (uint8_t *)row[b], a);
                        lasty[y] = b;
                        lasts[y] = a;
                    }
                }
            }

            demo_blit();
        }
    }

    void reset()
    {
        g_ax = { 0, 0 };

        CLEAR(rowdata1);
        CLEAR(rowdata2);
        CLEAR(row);

        CLEAR(pal2);
        CLEAR(palette);
        CLEAR(rowbuf);

        CLEAR(framey1);
        CLEAR(framey2);
        CLEAR(framey1t);
        CLEAR(framey2t);
        CLEAR(lasty);
        CLEAR(lasts);

        lens_current_plane_mask = 0;

        lens_scanline_chunky = 0;
        CLEAR(lens_scanline_plane_buffer);

        lens_scanline_plane_bufferptr = lens_scanline_plane_buffer;
    }

    void main()
    {
        reset();

        int frame, halt = 0;
        int a, b, c, d, y, la;
        int y1, y2, y1a, y2a, mika;

        Shim::clearScreen();

        for (a = 0; a < 200; a++)
            row[a] = rowdata1[a];

        for (a = 0; a < 200; a++)
            row[a + 200] = rowdata2[a];

        frame = 0;
        y = 0;
        y1 = 0;
        y1a = 500;
        y2 = 399 * 16;
        y2a = 500;
        mika = 1;

        for (frame = 0; frame < 200; frame++)
        {
            if (!halt)
            {
                y1 += y1a;
                y2 += y2a;

                y2a += 16;

                if (y2 > 400 * 16)
                {
                    y2 -= y2a;
                    y2a = -y2a * mika / 8;
                    if (mika < 4) mika += 3;
                }

                y1a += 16;

                la = a;
                a = (y2 - y1) - 400 * 16;
                if ((a & 0x8000) ^ (la & 0x8000))
                {
                    y1a = y1a * 7 / 8;
                }
                y1a += a / 8;
                y2a -= a / 8;
            }

            if (frame > 90)
            {
                if (y2 >= 399 * 16)
                {
                    y2 = 400 * 16;
                    halt = 1;
                }
                else
                    y2a = 8;
                y1 = y2 - 400 * 16;
            }

            framey1[frame] = y1;
            framey2[frame] = y2;
        }

        for (a = 0; a < 800; a++)
        {
            b = a / 4;
            c = a & 3;
            d = 3 - c;
            framey1t[a] = (framey1[b] * d + framey1[b + 1] * c) / 3;
            framey2t[a] = (framey2[b] * d + framey2[b + 1] * c) / 3;
        }

        Common::readp(palette, -1, (const char *)Data::jl_pic);

        palette[64 * 3 + 0] = 0;
        palette[64 * 3 + 1] = 0;
        palette[64 * 3 + 2] = 0;

        for (y = 0; y < 400; y++)
        {
            Common::readp(rowbuf, y, (const char *)Data::jl_pic);
            memcpy(row[y], rowbuf + 70, 184);
            row[y][184] = 65;
        }

        Common::setpalarea(palette, 0, PaletteColorCount);

        for (a = 0; a < 400; a++)
        {
            for (b = 0; b < 184; b++)
                if (row[a][b] == 0) row[a][b] = 64;
        }

        for (y = 0; y < 400; y++)
            lasty[y] = lasts[y] = -1;

        demo_vsync();

        scrolly(400);

        demo_vsync();

        for (y = 0; y < 400; y++)
        {
            lens_scanline_chunky = Shim::vram + y * SCREEN_WIDTH;
            linezoomC(lens_scanline_plane_buffer, (uint8_t *)row[y], 184);
        }

        a = 64;
        y = 400 * 64;

        while (y > 0)
        {
            y -= a;
            a += 6;

            if (y < 0) y = 0;

            scrolly(y / 64);

            demo_vsync();

            demo_blit();
        }

        demo_vsync();

        doit();

        scrolly(0);
    }
}
