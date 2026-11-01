/*
 * The demo's link to the console: what main.cpp, Graphics.cpp and
 * audioPlayer.cpp of Second Reality++ did (they are not used here).
 *
 * - demo_vsync(): waits for the next VBL (70 Hz) and returns the number of
 *   VBLs since the previous call, like the DOS demo's dis_waitb() (the
 *   parts that use it skip frames when the console is late).
 * - demo_blit(): shows the VGA memory (Shim::vram) on the LCD.
 * - AudioPlayer: the music (st3play) is mixed ahead into the console's
 *   sound ring; getSamplePosition() is what has been heard, so the parts
 *   follow what the player hears.
 */
#include <stdint.h>
#include <string.h>

#include "Parts/Common.h"
#include "Music/Music.h"
#include "Music/st3play.h"

#include "sr_platform.h"

#include "BeginTitleScreen_Data.h"
#include "CREDITS_MAIN_Data.h"

#define FORWARD_PART(X) \
    namespace X         \
    {                   \
        void main();    \
    }

FORWARD_PART(Alku)
FORWARD_PART(U2A)
FORWARD_PART(OUTTA)
FORWARD_PART(Beg)
FORWARD_PART(Glenz)
FORWARD_PART(Tunneli)
FORWARD_PART(KOE)
FORWARD_PART(Shutdown)
FORWARD_PART(Forest)
FORWARD_PART(Lens)
FORWARD_PART(PLZ)
FORWARD_PART(Dots)
FORWARD_PART(Water)
FORWARD_PART(Coman)
FORWARD_PART(JPLogo)
FORWARD_PART(U2E)
FORWARD_PART(End)
FORWARD_PART(Credits)
FORWARD_PART(EndScrl)

namespace
{
    // the memory groups of the parts (sr_overlay: parts of a group share
    // their variables; one group in memory at a time)
    enum Group
    {
        G_ALKU,
        G_VECTOR, // U2A, U2E and VISU (their 3D engine)
        G_PAM,
        G_BEG,
        G_GLENZ,
        G_TUNNELI,
        G_KOE,
        G_SHUTDOWN,
        G_FOREST,
        G_LENS,
        G_PLZ,
        G_DOTS,
        G_WATER,
        G_COMAN,
        G_JPLOGO,
        G_END,
        G_CREDITS,
        G_ENDSCRL,
    };

    struct PartInfo
    {
        const char * name;
        Music::Song music;
        unsigned char musicStartOrder;
        unsigned short width;
        unsigned short height;
        void (*part)();
        Group group;
    };

    // main.cpp of Second Reality++
    const PartInfo main_parts[SR_PART_COUNT] = {
        { "Alkutekstit I", Music::Song::Skaven, 0x00, SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT, Alku::main, G_ALKU },
        { "Alkutekstit II", Music::Song::Skaven, 0x0C, SCREEN_WIDTH, SCREEN_HEIGHT, U2A::main, G_VECTOR },
        { "Alkutekstit III", Music::Song::Skaven, 0x0D, SCREEN_WIDTH, SCREEN_HEIGHT, OUTTA::main, G_PAM },
        { "Logo", Music::Song::Skaven, 0x0E, SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT, Beg::main, G_BEG },
        { "Glenz", Music::Song::PurpleMotion, 0x00, SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT, Glenz::main, G_GLENZ },
        { "Dottitunneli", Music::Song::PurpleMotion, 0x0F, SCREEN_WIDTH, SCREEN_HEIGHT, Tunneli::main, G_TUNNELI },
        { "Techno", Music::Song::PurpleMotion, 0x14, SCREEN_WIDTH, SCREEN_HEIGHT, KOE::main, G_KOE },
        { "Panicfake", Music::Song::PurpleMotion, 0x27, SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT, Shutdown::main, G_SHUTDOWN },
        { "Vuori-Scrolli", Music::Song::PurpleMotion, 0x2A, SCREEN_WIDTH, SCREEN_HEIGHT, Forest::main, G_FOREST },
        { "Rotazoomer", Music::Song::PurpleMotion, 0x2F, SCREEN_WIDTH, SCREEN_HEIGHT, Lens::main, G_LENS },
        { "Plasmacube", Music::Song::PurpleMotion, 0x3E, SCREEN_WIDTH, SCREEN_HEIGHT, PLZ::main, G_PLZ },
        { "MiniVectorBalls", Music::Song::PurpleMotion, 0x4D, SCREEN_WIDTH, SCREEN_HEIGHT, Dots::main, G_DOTS },
        { "Peilipalloscroll", Music::Song::PurpleMotion, 0x58, SCREEN_WIDTH, SCREEN_HEIGHT, Water::main, G_WATER },
        { "3D-Sinusfield", Music::Song::PurpleMotion, 0x5E, SCREEN_WIDTH, SCREEN_HEIGHT, Coman::main, G_COMAN },
        { "Jellypic", Music::Song::PurpleMotion, 0x62, SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT, JPLogo::main, G_JPLOGO },
        { "Vector Part II", Music::Song::Skaven, 0x12, SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT, U2E::main, G_VECTOR },
        { "Endpictureflash", Music::Song::Skaven, 0x19, SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT, End::main, G_END },
        { "Credits/Greetings", Music::Song::Skaven, 0x1C, SCREEN_WIDTH, DOUBlE_SCREEN_HEIGHT, Credits::main, G_CREDITS },
        { "EndScrolling", Music::Song::Skaven, 0x2B, DOUBlE_SCREEN_WIDTH, 350, EndScrl::main, G_ENDSCRL },
    };

    constexpr int PANICFAKE = 7;
    constexpr int CREDITS = 17;

    int g_width = SCREEN_WIDTH, g_height = SCREEN_HEIGHT;
    uint32_t g_lastVbl;
    int g_part = -1;
    int g_jump = -1; // asked by sr_demo_jump()
    bool g_quit;     // asked by sr_demo_quit()
    bool g_playing; // a song is loaded (st3play)
}

bool sr_looping = false; // after the credits: the demo again (main.cpp's L)

#ifdef HOST_BUILD
extern "C" void host_sr_vram_probe(void);
extern "C" uint8_t * sr_probe_vram_base(void)
{
    return Shim::vram;
}
#endif

/* --- Second Reality++'s platform functions --- */

void demo_changemode(int x, int y, float /*jsss*/)
{
    g_width = x;
    g_height = y;
    Shim::ringsize = 0;
    Shim::dispstride = 0;
    Common::reset();
}

void demo_blit()
{
    sr_picture_t p;
    p.vram = Shim::vram;
    p.size = Shim::VRAM_X * Shim::VRAM_Y;
    p.start = Shim::startpixel;
    p.stride = g_width;
    p.ring = Shim::ringsize;
    p.w = g_width;
    p.h = g_height;
    p.dbl = 0;
    p.palette = reinterpret_cast<const uint32_t *>(Shim::palette);
    if (Shim::dispstride)
    {
        // a part showing its own layout of the VGA memory (the shutdown)
        p.start = Shim::dispstart;
        p.stride = Shim::dispstride;
        p.dbl = Shim::dispdouble;
    }
    sr_plat_present(&p);
}

bool demo_wantstoquit()
{
    // a part asked from the pause menu: the current one ends (its loops
    // test this), then sr_demo_run() goes to the new one
    return g_jump >= 0 || g_quit;
}

int demo_vsync(bool updateAudio)
{
    if (updateAudio)
    {
        AudioPlayer::Update(true);
    }

    uint32_t now = sr_plat_vbl();
    if (now == g_lastVbl)
    {
        sr_plat_wait_vbl(g_lastVbl + 1);
        now = sr_plat_vbl();
    }
    uint32_t n = now - g_lastVbl;
    g_lastVbl = now;
    // VBLs since the previous call (DOS: dis_waitb), at most half a second
    return n > 35 ? 35 : static_cast<int>(n);
}

namespace AudioPlayer
{
    uint32_t getSamplePosition()
    {
        return sr_plat_audio_pos();
    }

    bool openMixer(uint32_t /*audioFreq*/, int32_t & samplesLeft, uint32_t & totalSampleCount)
    {
        samplesLeft = 0;
        totalSampleCount = 0;
        sr_plat_audio_reset();
        g_playing = true;
        return true;
    }

    void closeMixer()
    {
        g_playing = false;
    }

    void Update(bool /*yield*/)
    {
        sr_plat_service();
    }
}

/* --- for the console --- */

extern "C" void sr_audio_render(int16_t * out, int n)
{
    static int16_t stereo[MIX_BUF_SAMPLES * 2];
    while (n > 0)
    {
        int k = n > MIX_BUF_SAMPLES ? MIX_BUF_SAMPLES : n;
        if (g_playing)
        {
            st3play::FillAudioBuffer(stereo, k);
            for (int i = 0; i < k; i++)
            {
                out[i] = static_cast<int16_t>((stereo[2 * i] + stereo[2 * i + 1]) >> 1);
            }
        }
        else
        {
            memset(out, 0, k * sizeof(out[0]));
        }
        out += k;
        n -= k;
    }
}

extern "C" const char * sr_part_name(int part)
{
    return part >= 0 && part < SR_PART_COUNT ? main_parts[part].name : "";
}

extern "C" int sr_demo_part(void)
{
    return g_part;
}

extern "C" void sr_demo_jump(int part)
{
    if (part >= 0 && part < SR_PART_COUNT) g_jump = part;
}

extern "C" void sr_demo_quit(void)
{
    g_quit = true;
}

// main.cpp's mainLoop()
extern "C" void sr_demo_run(int start)
{
    Music::Song lastMusic = Music::Song::COUNT;

    g_quit = false;
    for (int index = start; index < SR_PART_COUNT && !g_quit; index++)
    {
        if (g_jump >= 0)
        {
            index = g_jump;
            g_jump = -1;
            Shim::startDemoHere();
            lastMusic = Music::Song::COUNT; // the music from that part
        }

        g_part = index;
        const PartInfo & p = main_parts[index];

        sr_plat_part_start(index);
        sr_overlay_load(p.group);
#ifdef HOST_BUILD
        host_sr_vram_probe();
#endif

        if (lastMusic != p.music)
        {
            lastMusic = p.music;
            // (the sound waits, silent, until the part first asks for it:
            // its start-up can be long, the music must not go on meanwhile)
            Music::start(p.music, p.musicStartOrder);
        }

        if (index <= 14)
        {
            // parts 0-13 show one page: the rest of the VGA memory is their
            // scratch memory (Shim::SCRATCH), clean for them and for part 14
            memset(Shim::vram + Shim::SCRATCH_OFFSET, 0, Shim::SCRATCH_SIZE);
        }

        demo_changemode(p.width, p.height);
        g_lastVbl = sr_plat_vbl();

        p.part();

        if (g_jump >= 0)
        {
            index--; // the loop goes to g_jump
            continue;
        }

        Shim::finishedDemoFirstPart();

        if (index == PANICFAKE)
        {
            // wait for music between the shutdown / forest parts
            while (!demo_wantstoquit() && Music::getPlusFlags() >= 0)
            {
                demo_vsync();
                demo_blit();
            }

            while (!demo_wantstoquit() && Music::getPlusFlags() < 0)
            {
                demo_vsync();
                demo_blit();
            }
        }

        if (sr_looping && index == CREDITS)
        {
            lastMusic = Music::Song::COUNT;
            index = -1;
        }
    }
    g_part = -1;
    g_jump = -1;
    g_quit = false;
    st3play::Close(); // silence (AudioPlayer::closeMixer)
}

/* --- the title screen (sr_main.c): the logo of the demo, the font of the
 * credits --- */

extern "C" void sr_title_logo(sr_picture_t * p)
{
    // as the Logo part (BeginTitleScreen.cpp): 320x400 and its palette
    Palette palette{};
    Shim::clearScreen();
    Common::readp(palette, -1, (const char *)Beg::Data::TitleScreenData);
    for (int y = 0; y < DOUBlE_SCREEN_HEIGHT; y++)
    {
        Common::readp((char *)(Shim::vram + (unsigned)y * 320U), y, (const char *)Beg::Data::TitleScreenData);
    }
    Common::setpalarea(palette, 0, 254);
    Shim::setpal(254, 0, 0, 0);
    Shim::setpal(255, 0, 0, 0);
    p->vram = Shim::vram;
    p->size = Shim::VRAM_X * Shim::VRAM_Y;
    p->start = 0;
    p->stride = SCREEN_WIDTH;
    p->ring = 0;
    p->w = SCREEN_WIDTH;
    p->h = DOUBlE_SCREEN_HEIGHT;
    p->dbl = 0;
    p->palette = reinterpret_cast<const uint32_t *>(Shim::palette);
}

namespace
{
    // the letters of Credits::Data::credits_font (as credits_init())
    constexpr int FONT_H = 32;
    int font_p[256], font_w[256];

    void font_init()
    {
        static bool ready;
        if (ready) return;
        ready = true;
        const char * order = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/?!:,.\"()+-";
        int x = 0, y;
        for (; x < 1500 && *order;)
        {
            while (x < 1500)
            {
                for (y = 0; y < FONT_H; y++)
                    if (Credits::Data::credits_font[y][x]) break;
                if (y != FONT_H) break;
                x++;
            }
            const int b = x;
            while (x < 1500)
            {
                for (y = 0; y < FONT_H; y++)
                    if (Credits::Data::credits_font[y][x]) break;
                if (y == FONT_H) break;
                x++;
            }
            font_p[static_cast<unsigned char>(*order)] = b;
            font_w[static_cast<unsigned char>(*order)] = x - b;
            order++;
        }
        font_p[32] = 1500 - 32;
        font_w[32] = 8;
    }

    inline unsigned char font_upper(char c)
    {
        return static_cast<unsigned char>(c >= 'a' && c <= 'z' ? c - 32 : c);
    }
}

// width in the font (pixels, scale 1)
static int title_width(const char * t)
{
    int w = 0;
    for (; *t; t++) w += font_w[font_upper(*t)] + 2;
    return w > 0 ? w - 2 : 0;
}

/* text centred on cx, top y, scaled by sx / 256 and sy / 256 (smaller
 * only), blended over the LCD (the font is 0..9: from transparent to
 * `color`; each pixel takes the strongest point of the area it covers) */
extern "C" int sr_title_text(uint16_t * fb, int cx, int y0, const char * text, int sx, int sy, uint16_t color)
{
    font_init();
    static uint8_t line[FONT_H][640];
    int tw = title_width(text);
    if (tw > 640) tw = 640;
    memset(line, 0, sizeof(line));
    int x = 0;
    for (const char * t = text; *t && x < 640; t++)
    {
        const unsigned char c = font_upper(*t);
        for (int k = 0; k < font_w[c] && x + k < 640; k++)
            for (int y = 0; y < FONT_H; y++)
                line[y][x + k] = static_cast<uint8_t>(Credits::Data::credits_font[y][font_p[c] + k]);
        x += font_w[c] + 2;
    }
    const int w = (tw * sx + 255) / 256;
    const int h = (FONT_H * sy + 255) / 256;
    const int x0 = cx - w / 2;
    const int cr = color >> 11, cg = (color >> 5) & 63, cb = color & 31;
    for (int dy = 0; dy < h; dy++)
    {
        const int y = y0 + dy;
        if (y < 0 || y >= 240) continue;
        const int fy0 = dy * 256 / sy, fy1 = (dy + 1) * 256 / sy;
        for (int dx = 0; dx < w; dx++)
        {
            const int px = x0 + dx;
            if (px < 0 || px >= 320) continue;
            const int fx0 = dx * 256 / sx, fx1 = (dx + 1) * 256 / sx;
            int a = 0;
            for (int fy = fy0; fy < fy1 && fy < FONT_H; fy++)
                for (int fx = fx0; fx < fx1 && fx < tw; fx++)
                    if (line[fy][fx] > a) a = line[fy][fx];
            if (!a) continue;
            uint16_t & d = fb[y * 320 + px];
            const int r = ((d >> 11) * (9 - a) + cr * a) / 9;
            const int g = (((d >> 5) & 63) * (9 - a) + cg * a) / 9;
            const int b = ((d & 31) * (9 - a) + cb * a) / 9;
            d = static_cast<uint16_t>(r << 11 | g << 5 | b);
        }
    }
    return w;
}
