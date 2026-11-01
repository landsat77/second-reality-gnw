/*
 * Second Reality (Future Crew, 1993) for Retro-Go SD (Game & Watch), as a
 * GWHB homebrew: /homebrews/SecondReality.bin with SecondReality.srd (the
 * demo's data, tools/sr_data.py + tools/sr_pack.py) beside it.
 *
 * - The pack is cached in the external flash and read in place.
 * - The LCD runs at 70 Hz like the VGA modes of the demo: one VBL of the
 *   demo = one LCD frame. The pictures (320x200, 320x400, 640x350) are
 *   scaled to 320x240 (the 4:3 picture of the PC screen).
 * - The music (st3play) is mixed ahead into a ring that the audio
 *   interrupt plays (pcm_attach); the demo follows what is heard.
 * - Pause menu: the part to play, loop; saves: the part being played.
 * - Buttons: GAME = next part, TIME = previous part; in the end scroller,
 *   A or B = back to the launcher.
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "main.h"
#include "common.h"
#include "gw_lcd.h"
#include "gw_audio.h"
#include "gw_malloc.h"
#include "rom_manager.h"
#include "odroid_system.h"
#include "appid.h"
#include "odroid_overlay.h"
#include "odroid_settings.h"
#include "rg_storage.h"
#include "gw_core_i18n.h"

#ifndef HOST_BUILD
#include "gw_core_bridge.h" /* last: rewrites ACTIVE_FILE, common_emu_state */
#else
#include "host_compat.h"
#include "host_sr.h"
#endif

#include "sr_platform.h"
#include "sr_pack.h"

/* LCD frames (VBLs): the firmware's frame_counter, incremented at each vsync */
#ifndef HOST_BUILD
static inline uint32_t sr_frame_counter(void)
{
    return *(volatile uint32_t *)&frame_counter;
}
#else
#define sr_frame_counter() lcd_get_frame_counter()
#endif

#define APP_ID APPID_HOMEBREW
#define RING_SIZE 16384  /* samples, power of 2 (32 KB of DTCM) */
#define RING_AHEAD 8192  /* ~186 ms at 44.1 kHz: the parts' start-ups */
#define FEED_CHUNK 512

extern bool sr_looping;

static odroid_gamepad_state_t joystick;

/* statistics (stats_part()) */
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004)
static uint64_t stat_idle_cycles, stat_total_cycles;
static uint32_t stat_last_cycles;
static void stat_count_cycles(void);
static uint32_t stat_vbl0, stat_presents, stat_longest, stat_last_present;
static int stat_part = -1;
static char stat_log_path[200];
static uint32_t audio_missed;
static uint32_t vbl_base;          /* LCD frames of the menus etc.: not demo time */
static uint32_t last_input_frame;
static int in_service;
static bool menu_paused;

/* ------------------------------------------------------------------ */
/* Options and saves                                                  */
/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t magic;
    uint8_t loop;
    uint8_t pad[3];
} sr_options_t;
#define SR_OPTIONS_MAGIC 0x53524F31 /* "SRO1" */
static sr_options_t options = { SR_OPTIONS_MAGIC, 0, { 0 } };
static char save_dir[160];
static char options_path[200];

static void options_load(void)
{
    FILE *f = fopen(options_path, "rb");
    if (!f)
        return;
    sr_options_t o;
    if (fread(&o, sizeof(o), 1, f) == 1 && o.magic == SR_OPTIONS_MAGIC)
        options = o;
    fclose(f);
    sr_looping = options.loop != 0;
}

static void options_save(void)
{
    FILE *f = fopen(options_path, "wb");
    if (!f)
        return;
    fwrite(&options, sizeof(options), 1, f);
    fclose(f);
}

/* a save = the part being played (the demo starts again from there) */
typedef struct {
    uint32_t magic;
    int32_t part;
    uint32_t pack_crc;
} sr_state_t;
#define SR_STATE_MAGIC 0x53525331 /* "SRS1" */

static bool load_state(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    sr_state_t s;
    bool ok = fread(&s, sizeof(s), 1, f) == 1 && s.magic == SR_STATE_MAGIC && s.part >= 0 &&
              s.part < SR_PART_COUNT;
    fclose(f);
    if (ok)
        sr_demo_jump(s.part);
    return ok;
}

static bool save_state(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    sr_state_t s = { SR_STATE_MAGIC, sr_demo_part() < 0 ? 0 : sr_demo_part(), sr_pack_crc() };
    bool ok = fwrite(&s, sizeof(s), 1, f) == 1;
    fclose(f);
    return ok;
}

/* ------------------------------------------------------------------ */
/* Picture                                                            */
/* ------------------------------------------------------------------ */
static sr_picture_t pic;
static bool have_pic;
static bool dirty;
static uint16_t pal565[256];

/* average of two RGB565 pixels */
#define AVG565(a, b) ((uint16_t)((((a) ^ (b)) & 0xF7DEu) >> 1) + ((a) & (b)))

/* line n of the picture */
static const uint8_t *line_at(int n)
{
    uint32_t off = pic.start + (uint32_t)(pic.dbl ? n >> 1 : n) * pic.stride;
    if (pic.ring)
        off %= pic.ring;
    if (off + (uint32_t)pic.w > pic.size)
        off = 0; /* (not seen: a part showing outside the VGA memory) */
    return pic.vram + off;
}

/* lines of the picture converted to RGB565 (two kept: a line is often used
 * by two lines of the LCD) */
static uint16_t line_rgb[2][640] __attribute__((aligned(4)));
static int line_n[2] = { -1, -1 };
static int line_next;

static const uint16_t *line_565(int n)
{
    if (line_n[0] == n)
        return line_rgb[0];
    if (line_n[1] == n)
        return line_rgb[1];
    int k = line_next;
    line_next ^= 1;
    line_n[k] = n;
    const uint8_t *s = line_at(n);
    uint32_t *d = (uint32_t *)line_rgb[k];
    const uint16_t *p = pal565;
    for (int i = 0; i < pic.w; i += 4, s += 4, d += 2) {
        d[0] = p[s[0]] | (uint32_t)p[s[1]] << 16;
        d[1] = p[s[2]] | (uint32_t)p[s[3]] << 16;
    }
    return line_rgb[k];
}

/* two RGB565 pixels at once */
#define AVG565x2(a, b) (((((a) ^ (b)) & 0xF7DEF7DEu) >> 1) + ((a) & (b)))

static void avg_line(const uint16_t *a, const uint16_t *b, uint16_t *d)
{
    const uint32_t *a32 = (const uint32_t *)a, *b32 = (const uint32_t *)b;
    uint32_t *d32 = (uint32_t *)d;
    for (int x = 0; x < 160; x++)
        d32[x] = AVG565x2(a32[x], b32[x]);
}

static void convert(uint16_t *fb)
{
    const int w = pic.w, h = pic.h;

    for (int i = 0; i < 256; i++) {
        uint32_t c = pic.palette[i];
        pal565[i] = (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F));
    }
    line_n[0] = line_n[1] = -1;

    if (w == 320 && h == 200) {
        /* 200 -> 240 lines: y * 5 / 6, the in-between lines averaged */
        for (int y = 0; y < 240; y++) {
            int p = y * 5;
            int frac = p % 6; /* sixths */
            int n = p / 6;
            uint16_t *d = fb + y * 320;
            if (frac >= 5 && n < 199)
                n++;
            if (frac <= 1 || frac >= 5 || n >= 199)
                memcpy(d, line_565(n), 640);
            else
                avg_line(line_565(n), line_565(n + 1), d);
        }
    } else if (w == 320 && h == 400) {
        /* 400 -> 240 lines: two lines averaged (y * 5 / 3 and the next) */
        for (int y = 0; y < 240; y++) {
            int n = (y * 5) / 3;
            avg_line(line_565(n), line_565(n + 1), fb + y * 320);
        }
    } else if (w == 640 && h == 350) {
        /* 640x350 -> 320x240: 2x2 pixels averaged (y * 35 / 24) */
        for (int y = 0; y < 240; y++) {
            int n = (y * 35) / 24;
            if (n > 348)
                n = 348;
            const uint16_t *a = line_565(n);
            const uint16_t *b = line_565(n + 1);
            uint16_t *d = fb + y * 320;
            for (int x = 0; x < 320; x++) {
                uint16_t c0 = AVG565(a[2 * x], a[2 * x + 1]);
                uint16_t c1 = AVG565(b[2 * x], b[2 * x + 1]);
                d[x] = AVG565(c0, c1);
            }
        }
    } else {
        memset(fb, 0, 320 * 240 * 2);
    }
}

static void present_now(void)
{
    uint32_t now = sr_plat_vbl();
    if (stat_presents && now - stat_last_present > stat_longest)
        stat_longest = now - stat_last_present;
    stat_last_present = now;
    stat_presents++;
    uint16_t *fb = lcd_get_active_buffer();
    convert(fb);
    common_ingame_overlay();
    lcd_swap();
    dirty = false;
}

void sr_plat_present(const sr_picture_t *picture)
{
    pic = *picture;
    have_pic = true;
    dirty = true;
#ifdef HOST_BUILD
    host_sr_on_present();
#endif
    /* the previous picture is not shown yet: this one at the next VBL */
    if (!lcd_is_swap_pending())
        present_now();
    sr_plat_service();
}

/* menu background */
static void repaint(void)
{
    lcd_sleep_while_swap_pending();
    if (have_pic)
        convert(lcd_get_active_buffer());
    common_ingame_overlay();
}

static void *screenshot(void)
{
    lcd_wait_for_vblank();
    if (have_pic) {
        convert(lcd_get_active_buffer());
        return lcd_get_active_buffer();
    }
    return lcd_get_inactive_buffer();
}

/* ------------------------------------------------------------------ */
/* Sound                                                              */
/* ------------------------------------------------------------------ */
static int16_t *ring;
static volatile uint16_t ring_head, ring_tail; /* written / read (interrupt) */
static uint32_t ring_written;                  /* samples written since the reset */

static void ring_attach(uint32_t pos)
{
    pcm_attach(ring, RING_SIZE, &ring_head, &ring_tail);
    pcm_audio_setpos(pos);
    pcm_audio_set(common_emu_sound_get_volume(), 1);
    pcm_audio_enable(1);
}

void sr_plat_audio_reset(void)
{
    /* a new song: from now on (what is in the ring is dropped); the sound
     * waits (silent, its position stopped) until the song's first samples
     * are in the ring: the song is still being loaded */
    pcm_audio_set(common_emu_sound_get_volume(), 0);
    ring_tail = ring_head;
    ring_written = 0;
    pcm_audio_setpos(0);
}

uint32_t sr_plat_audio_pos(void)
{
    sr_plat_audio_feed();
    uint32_t played = pcm_audio_pos();
    return played < ring_written ? played : ring_written;
}

void sr_plat_audio_feed(void)
{
    /* the interrupt played silence when the ring was empty */
    uint32_t played = pcm_audio_pos();
    if ((int32_t)(played - ring_written) > 0) {
        audio_missed += played - ring_written;
        ring_written = played;
    }
    int level = (ring_head - ring_tail) & (RING_SIZE - 1);
    int want = RING_AHEAD - level;
    while (want > 0) {
        int head = ring_head;
        int n = want;
        if (n > RING_SIZE - head)
            n = RING_SIZE - head; /* up to the end of the ring */
        if (n > FEED_CHUNK)
            n = FEED_CHUNK;
        sr_audio_render(ring + head, n);
        ring_head = (uint16_t)((head + n) & (RING_SIZE - 1));
        ring_written += n;
        want -= n;
    }
    bool muted = common_emu_sound_loop_is_muted();
    pcm_audio_set(common_emu_sound_get_volume(), (muted || menu_paused) ? 0 : 1);
}

/* after a sleep (power button): the sound again */
static void sleep_wake(void)
{
    odroid_audio_init(SR_AUDIO_RATE);
    audio_start_playing(SR_AUDIO_RATE / SR_REFRESH_HZ);
    ring_attach(pcm_audio_pos());
}

/* ------------------------------------------------------------------ */
/* Pause menu                                                         */
/* ------------------------------------------------------------------ */
static const gw_i18n_entry_t i18n_yes[] = {
    { "en", "Yes" }, { "fr", "Oui" }, { "es", "S\xc3\xad" }, { "de", "Ja" }, { "it", "S\xc3\xac" }, { "pt", "Sim" }, GW_I18N_END
};
static const gw_i18n_entry_t i18n_no[] = {
    { "en", "No" }, { "fr", "Non" }, { "es", "No" }, { "de", "Nein" }, { "it", "No" }, { "pt", "N\xc3\xa3o" }, GW_I18N_END
};
static const gw_i18n_entry_t i18n_part[] = {
    { "en", "Part" }, { "fr", "Partie" }, { "es", "Parte" }, { "de", "Teil" }, { "it", "Parte" }, { "pt", "Parte" }, GW_I18N_END
};
static const gw_i18n_entry_t i18n_loop[] = {
    { "en", "Loop" }, { "fr", "En boucle" }, { "es", "Bucle" }, { "de", "Schleife" }, { "it", "Ciclo" }, { "pt", "Repetir" }, GW_I18N_END
};
static const gw_i18n_entry_t i18n_no_pack[] = {
    { "en", "Data not found:" }, { "fr", "Donn\xc3\xa9" "es introuvables :" }, GW_I18N_END
};
static const gw_i18n_entry_t i18n_bad_pack[] = {
    { "en", "Not a valid .srd file:" }, { "fr", "Fichier .srd invalide :" }, GW_I18N_END
};
static const gw_i18n_entry_t i18n_no_flash[] = {
    { "en", "Pack too big for the flash:" }, { "fr", "Pack trop gros pour la flash :" }, GW_I18N_END
};

static char part_value[40], loop_value[12];
static int menu_part;

static void part_label(void)
{
    snprintf(part_value, sizeof(part_value), "%d %s", menu_part + 1, sr_part_name(menu_part));
}

static bool part_cb(odroid_dialog_choice_t *option, odroid_dialog_event_t event, uint32_t repeat)
{
    (void)option;
    (void)repeat;
    if (event == ODROID_DIALOG_PREV)
        menu_part = (menu_part + SR_PART_COUNT - 1) % SR_PART_COUNT;
    if (event == ODROID_DIALOG_NEXT)
        menu_part = (menu_part + 1) % SR_PART_COUNT;
    part_label();
    if (event == ODROID_DIALOG_ENTER) {
        sr_demo_jump(menu_part);
        return true; /* closes the menu */
    }
    return false;
}

static bool loop_cb(odroid_dialog_choice_t *option, odroid_dialog_event_t event, uint32_t repeat)
{
    (void)repeat;
    if (event == ODROID_DIALOG_PREV || event == ODROID_DIALOG_NEXT || event == ODROID_DIALOG_ENTER) {
        options.loop = !options.loop;
        sr_looping = options.loop != 0;
        options_save();
    }
    strncpy(option->value, gw_i18n(options.loop ? i18n_yes : i18n_no), 11);
    option->value[11] = '\0';
    return false;
}

static odroid_dialog_choice_t game_options[3];

static void setup_game_options(void)
{
    game_options[0] = (odroid_dialog_choice_t){ 300, gw_i18n(i18n_part), part_value, 1, &part_cb };
    game_options[1] = (odroid_dialog_choice_t){ 301, gw_i18n(i18n_loop), loop_value, 1, &loop_cb };
    game_options[2] = (odroid_dialog_choice_t)ODROID_DIALOG_CHOICE_LAST;
}

/* ------------------------------------------------------------------ */
/* Service: watchdog, sound, buttons, menu                            */
/* ------------------------------------------------------------------ */
static void run_input(void)
{
    static bool game_held, time_held;
    odroid_input_read_gamepad(&joystick);

    bool menu_key = joystick.values[ODROID_INPUT_VOLUME] != 0;
    if (menu_key) {
        /* the pause menu may open: the demo's time stops */
        menu_paused = true;
        pcm_audio_set(common_emu_sound_get_volume(), 0);
    }
    uint32_t f0 = sr_frame_counter();

    menu_part = sr_demo_part() < 0 ? 0 : sr_demo_part();
    part_label();
    strncpy(loop_value, gw_i18n(options.loop ? i18n_yes : i18n_no), 11);
    loop_value[11] = '\0';

    common_emu_input_loop(&joystick, game_options, &repaint);

    uint32_t spent = sr_frame_counter() - f0;
    if (menu_key || spent > 2) {
        /* back from the menu (or a sleep): the time spent there is not the demo's */
        vbl_base += spent;
        printf("sr: menu/sleep %lu VBL (menu key %d)\n", (unsigned long)spent, menu_key);
        menu_paused = false;
        ring_attach(pcm_audio_pos()); /* the sound was perhaps restarted */
        if (have_pic) {
            lcd_sleep_while_swap_pending();
            present_now();
        }
    }

    /* GAME = next part, TIME = previous part */
    bool game = joystick.values[ODROID_INPUT_START] != 0;
    bool time = joystick.values[ODROID_INPUT_SELECT] != 0;
    if (game && !game_held && sr_demo_part() >= 0)
        sr_demo_jump((sr_demo_part() + 1) % SR_PART_COUNT);
    if (time && !time_held && sr_demo_part() >= 0)
        sr_demo_jump((sr_demo_part() + SR_PART_COUNT - 1) % SR_PART_COUNT);
    game_held = game;
    time_held = time;

    /* the end scroller (it never ends): A or B = back to the launcher */
    if (sr_demo_part() == SR_PART_COUNT - 1 &&
        (joystick.values[ODROID_INPUT_A] || joystick.values[ODROID_INPUT_B]))
        sr_demo_quit();
}

void sr_plat_service(void)
{
    if (in_service)
        return;
    in_service = 1;

    wdog_refresh();
    stat_count_cycles();
#ifdef HOST_BUILD
    host_sr_service();
#endif
    sr_plat_audio_feed();

    /* a picture that waited for the previous one */
    if (dirty && !lcd_is_swap_pending())
        present_now();

    uint32_t frame = sr_frame_counter();
    if (frame != last_input_frame) {
        last_input_frame = frame;
        lcd_sleep_while_swap_pending(); /* common_emu_input_loop() wants no swap pending */
        run_input();
    }
    in_service = 0;
}

uint32_t sr_plat_vbl(void)
{
    return sr_frame_counter() - vbl_base;
}

void sr_plat_wait_vbl(uint32_t vbl)
{
    while ((int32_t)(sr_plat_vbl() - vbl) < 0) {
        sr_plat_service();
#ifdef HOST_BUILD
        host_sr_idle();
#else
        uint32_t c0 = DWT_CYCCNT;
        __WFI(); /* the LCD, the audio DMA or SysTick wake us up */
        stat_idle_cycles += DWT_CYCCNT - c0;
#endif
    }
}

/* ------------------------------------------------------------------ */
/* Statistics: a line per part in <saves>/log.txt                     */
/* ------------------------------------------------------------------ */
/* the cycle counter wraps every 15 s: counted in 64 bits at each service */
static void stat_count_cycles(void)
{
#ifndef HOST_BUILD
    uint32_t now = DWT_CYCCNT;
    stat_total_cycles += (uint32_t)(now - stat_last_cycles);
    stat_last_cycles = now;
#endif
}

static void stats_part(int part)
{
    uint32_t vbls = sr_plat_vbl() - stat_vbl0;
    if (stat_part >= 0 && vbls > 0) {
        char line[200];
        uint32_t secs10 = vbls * 10 / SR_REFRESH_HZ;
#ifndef HOST_BUILD
        stat_count_cycles();
        uint32_t idle = stat_total_cycles ? (uint32_t)(stat_idle_cycles * 100 / stat_total_cycles) : 0;
#else
        uint32_t idle = 0;
#endif
        snprintf(line, sizeof(line), "part %2d %-18s %3lu.%lu s, %lu pictures (%lu/s), longest frame %lu VBL, idle %lu%%, sound missed %lu\n",
                 stat_part, sr_part_name(stat_part), (unsigned long)(secs10 / 10), (unsigned long)(secs10 % 10),
                 (unsigned long)stat_presents, (unsigned long)(stat_presents * SR_REFRESH_HZ / vbls),
                 (unsigned long)stat_longest, (unsigned long)idle, (unsigned long)audio_missed);
        printf("sr: %s", line);
        /* a line at a time (no fflush in the firmware's ABI) */
        FILE *f = stat_log_path[0] ? fopen(stat_log_path, "a") : NULL;
        if (f) {
            fputs(line, f);
            fclose(f);
        }
    }
    stat_part = part;
    stat_vbl0 = sr_plat_vbl();
    stat_presents = 0;
    stat_longest = 0;
    stat_count_cycles();
    stat_idle_cycles = 0;
    stat_total_cycles = 0;
    audio_missed = 0;
}

void sr_plat_part_start(int part)
{
    stats_part(part);
}

/* ------------------------------------------------------------------ */
/* Entry                                                              */
/* ------------------------------------------------------------------ */
static void show_error(const char *msg, const char *detail)
{
    printf("sr: %s %s\n", msg, detail ? detail : "");
#ifdef HOST_BUILD
    exit(1);
#endif
    odroid_dialog_choice_t choices[] = {
        { 0, msg, "", -1, NULL },
        { 0, detail ? detail : "", "", -1, NULL },
        ODROID_DIALOG_CHOICE_SEPARATOR,
        { 1, "OK", "", 1, NULL },
        ODROID_DIALOG_CHOICE_LAST,
    };
    pcm_audio_enable(0);
    lcd_set_refresh_rate(60);
    lcd_clear_buffers();
    odroid_overlay_dialog("Second Reality", choices, 3, NULL, 0);
    odroid_system_switch_app(0);
    while (1)
        ;
}

void sr_fatal(const char *message, const char *detail)
{
    show_error(message, detail);
}

/* ".../SecondReality.bin" -> ".../SecondReality.srd" */
static void pack_path_for(const char *bin_path, char *out, size_t out_size)
{
    snprintf(out, out_size, "%s", bin_path);
    char *slash = strrchr(out, '/');
    char *dot = strrchr(out, '.');
    if (dot && (!slash || dot > slash))
        *dot = '\0';
    size_t len = strlen(out);
    snprintf(out + len, out_size - len, ".srd");
}

void app_main(uint8_t load_state_flag, uint8_t start_paused, int8_t save_slot)
{
#ifndef HOST_BUILD
    gw_core_bridge_init();
    /* cycle counter for the statistics */
    *(volatile uint32_t *)0xE000EDFC |= 1u << 24; /* DEMCR.TRCENA */
    *(volatile uint32_t *)0xE0001000 |= 1u;       /* DWT_CTRL.CYCCNTENA */
#endif
    (void)start_paused;
    printf("Second Reality start\n");

    odroid_system_init(APP_ID, SR_AUDIO_RATE);
    odroid_system_emu_init(&load_state, &save_state, &screenshot, NULL, &sleep_wake, NULL, NULL);
    setup_game_options();

    static char pack_path[256];
    pack_path_for(ACTIVE_FILE->path, pack_path, sizeof(pack_path));
    const char *pack_name = strrchr(pack_path, '/');
    pack_name = pack_name ? pack_name + 1 : pack_path;

    {
        char name[64];
        snprintf(name, sizeof(name), "%s", pack_name);
        char *dot = strrchr(name, '.');
        if (dot)
            *dot = '\0';
        rg_storage_mkdir(ODROID_BASE_PATH_SAVES "/secondreality");
        snprintf(save_dir, sizeof(save_dir), ODROID_BASE_PATH_SAVES "/secondreality/%s", name);
        rg_storage_mkdir(save_dir);
        snprintf(options_path, sizeof(options_path), "%s/gnw_options.bin", save_dir);
        options_load();
        snprintf(stat_log_path, sizeof(stat_log_path), "%s/log.txt", save_dir);
        FILE *f = fopen(stat_log_path, "w");
        if (f) {
            fprintf(f, "Second Reality %s\n", SR_VERSION);
            fclose(f);
        }
    }

    FILE *probe = fopen(pack_path, "rb");
    if (!probe)
        show_error(gw_i18n(i18n_no_pack), pack_name);
    fclose(probe);
    uint32_t pack_size = 0;
    uint8_t *pack = odroid_overlay_cache_file_in_flash(pack_path, &pack_size, false);
    if (!pack)
        show_error(gw_i18n(i18n_no_flash), pack_name);
    if (sr_pack_set(pack, pack_size))
        show_error(gw_i18n(i18n_bad_pack), pack_name);
    sr_bind_data();

    ring = dtc_calloc(RING_SIZE, sizeof(int16_t));
    if (!ring)
        show_error("Out of memory", "sound ring");

    lcd_set_refresh_rate(SR_REFRESH_HZ);
    lcd_clear_buffers();
    audio_start_playing(SR_AUDIO_RATE / SR_REFRESH_HZ);
    ring_attach(0);
    vbl_base = sr_frame_counter();
    last_input_frame = vbl_base;

    if (load_state_flag)
        odroid_system_emu_load_state(save_slot);

    sr_demo_run(0);
    stats_part(-1);

    /* the end: back to the launcher */
    pcm_audio_enable(0);
    lcd_set_refresh_rate(60);
    printf("Second Reality end (audio missed %lu samples)\n", (unsigned long)audio_missed);
    odroid_system_switch_app(0);
}
