/*
 * The memory of the parts: one group at a time (tools/sr_overlays.py).
 * Host build: no overlay (every part has its own variables), only the
 * high-water marks are not measured.
 */
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "sr_platform.h"

#ifndef HOST_BUILD
#include "gw_malloc.h"
#include "sr_overlays.h"
#include "gw_core_bridge.h"

static uint8_t *data_copy;
static int loaded = -1;

void sr_overlay_load(int group)
{
    static const uint32_t sizes[SR_GROUP_COUNT] = SR_GROUP_SIZES;
    const uint32_t data_size = (uint32_t)(__sr_parts_data_end - __sr_parts_data_start);

    if (!data_copy && data_size) {
        /* the parts' initial values, before any part runs */
        data_copy = ahb_malloc(data_size);
        if (data_copy)
            memcpy(data_copy, __sr_parts_data_start, data_size);
    }
    if (data_copy && loaded >= 0)
        memcpy(__sr_parts_data_start, data_copy, data_size);

    /* the group's variables: zero, as at the start of a program */
    uint32_t size = sizes[group];
    for (uint32_t done = 0; done < size; done += 16384) {
        uint32_t n = size - done < 16384 ? size - done : 16384;
        memset(__sr_ovl_start + done, 0, n);
        wdog_refresh();
    }
    loaded = group;
    printf("sr: part memory '%s' %lu bytes (area %lu, parts' data %lu)\n", sr_group_names[group],
           (unsigned long)size, (unsigned long)(__sr_ovl_end - __sr_ovl_start), (unsigned long)data_size);
}
#else
void sr_overlay_load(int group)
{
    (void)group;
}
#endif
