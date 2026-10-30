#include <string.h>

#include "sr_pack.h"

static const uint8_t *pack;
static uint32_t pack_size, pack_count;

static uint32_t rd32(const uint8_t *p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

extern "C" int sr_pack_set(const uint8_t *data, uint32_t size)
{
    if (size < 16 || memcmp(data, "SRD1", 4) || rd32(data + 8) != size)
        return -1;
    pack_count = rd32(data + 4);
    if (16 + 12 * (uint64_t)pack_count > size)
        return -1;
    pack = data;
    pack_size = size;
    return 0;
}

extern "C" uint32_t sr_pack_crc(void)
{
    return pack ? rd32(pack + 12) : 0;
}

extern "C" const void *sr_pack_find(const char *name, uint32_t *size)
{
    uint32_t lo = 0, hi = pack_count;
    while (lo < hi) { /* names are sorted */
        uint32_t mid = (lo + hi) / 2;
        const uint8_t *e = pack + 16 + 12 * mid;
        int c = strcmp(name, (const char *)pack + rd32(e));
        if (c == 0) {
            if (size)
                *size = rd32(e + 8);
            return pack + rd32(e + 4);
        }
        if (c < 0)
            hi = mid;
        else
            lo = mid + 1;
    }
    return 0;
}

extern "C" const void *sr_pack_data(const char *name, uint32_t size)
{
    uint32_t got = 0;
    const void *p = pack ? sr_pack_find(name, &got) : 0;
    if (!p)
        sr_fatal("missing in the pack (.srd):", name);
    if (got != size)
        sr_fatal("wrong size in the pack (.srd):", name);
    return p;
}

/* the dynamic initialisation of sr_flash_refs.cpp, moved out of .init_array */
typedef void (*sr_init_fn)(void);
extern "C" sr_init_fn __start_sr_init_array[] __attribute__((weak));
extern "C" sr_init_fn __stop_sr_init_array[] __attribute__((weak));

extern "C" void sr_bind_data(void)
{
    for (sr_init_fn *f = __start_sr_init_array; f < __stop_sr_init_array; f++)
        (*f)();
}

extern "C" void sr_hot_load(int group)
{
    for (const sr_hot_t *h = sr_hot_table; h->group >= 0; h++)
        if (h->group == group)
            memcpy(h->ram, sr_pack_data(h->name, h->size), h->size);
}
