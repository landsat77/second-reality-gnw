/*
 * The pack (SecondReality.srd, beside the .bin): the demo's big arrays and
 * the music's samples, read in place (cached in the external flash).
 *
 * Format "SRD1" (little endian, tools/sr_pack.py):
 *   u32 magic, u32 count, u32 total size, u32 crc32 of the rest
 *   count x { u32 name offset, u32 data offset, u32 size }, sorted by name
 *   names (0-terminated), then the data (each 32-byte aligned)
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 0 if ok */
int sr_pack_set(const uint8_t *data, uint32_t size);
uint32_t sr_pack_crc(void);
/* an entry, or NULL */
const void *sr_pack_find(const char *name, uint32_t *size);
/* an entry of this size, or a fatal error */
const void *sr_pack_data(const char *name, uint32_t size);
/* binds the references of build/gen/sr_flash_refs.cpp (after sr_pack_set) */
void sr_bind_data(void);

/* tables of the parts copied to RAM when the part starts (build/gen/sr_hot.cpp) */
typedef struct {
    int group;
    void *ram;
    const char *name;
    uint32_t size;
} sr_hot_t;
extern const sr_hot_t sr_hot_table[];
void sr_hot_load(int group);

void sr_fatal(const char *message, const char *detail);

#ifdef __cplusplus
}
#endif
