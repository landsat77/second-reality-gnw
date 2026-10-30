#include "Blob.h"

#include <cstring>
#include <stdint.h>
#include <stdio.h>

#ifdef SR_GNW
// Game & Watch: the files are in the pack ("Blob::Data::_Blob_<hash>"),
// a table instead of std::unordered_map
#include "sr_pack.h"
#endif

namespace Blob
{
    struct BlobInfo
    {
        unsigned int hash;
        unsigned int size;
        const void * ptr;
        unsigned int offset;
    };

    namespace
    {
        BlobInfo blobs[] = {
            { 343536862, 6141 },
            { 324104981, 70718 },
            { 301510314, 8928 },
            { 301575852, 7684 },
            { 301641390, 2032 },
            { 305442322, 796 },
            { 307867470, 8 },
            { 307933000, 16081 },
            { 301772330, 832 },
            { 301837868, 1756 },
            { 301903406, 836 },
            { 301968928, 800 },
            { 302034466, 916 },
            { 302100004, 1292 },
            { 302165542, 1024 },
            { 302231096, 1260 },
            { 302296634, 4044 },
            { 305704594, 900 },
            { 301772332, 448 },
            { 301837870, 532 },
            { 301903400, 1404 },
            { 301968938, 796 },
            { 302034468, 564 },
            { 302100006, 800 },
            { 302165536, 448 },
            { 302231074, 1600 },
            { 302296636, 1432 },
            { 302362174, 796 },
            { 301837856, 756 },
            { 301903394, 1760 },
            { 301968932, 448 },
            { 302034470, 12672 },
            { 302100008, 3704 },
            { 302165546, 796 },
            { 302231084, 1252 },
            { 302296622, 2212 },
            { 302362160, 8968 },
            { 302427698, 796 },
            { 301903396, 1756 },
            { 301968934, 448 },
            { 302034464, 1572 },
            { 302100002, 796 },
            { 302165548, 4092 },
            { 302231086, 1048 },
            { 302296616, 3696 },
            { 302362154, 2716 },
            { 302427700, 3356 },
            { 302493238, 1900 },
            { 301968952, 1116 },
            { 302034490, 796 },
            { 302100028, 796 },
            { 308129742, 8 },
            { 308195272, 77069 }
        };
    }

    inline uint16_t rotl16(uint16_t x, unsigned r)
    {
        return (uint16_t)((x << (r & 15)) | (x >> ((16 - r) & 15)));
    }

    uint32_t gethash(const char * p)
    {
        uint16_t u1 = 0x1111;
        uint16_t u2 = 0x1111;

        for (const unsigned char * s = (const unsigned char *)p; *s; ++s)
        {
            uint16_t ax = (uint16_t)((uint16_t)*s & ~0x20u);

            u1 ^= ax;
            u1 = rotl16(u1, 1);
            u2 = (uint16_t)(u2 + ax);
        }

        return ((uint32_t)u2 << 16) | (uint32_t)u1;
    }

    Handle open(const char * filename)
    {
        unsigned int hash = gethash(filename);

        for (auto & currentBlob : blobs)
        {
            if (currentBlob.hash == hash)
            {
                if (!currentBlob.ptr)
                {
                    char name[48];
                    snprintf(name, sizeof(name), "Blob::Data::_Blob_%u", hash);
                    uint32_t size = 0;
                    currentBlob.ptr = sr_pack_find(name, &size);
                    if (!currentBlob.ptr || size < currentBlob.size) sr_fatal("missing in the pack (.srd):", name);
                }
                currentBlob.offset = 0;
                return reinterpret_cast<Handle>(&currentBlob);
            }
        }

        return nullptr;
    }

#ifdef SR_GNW
    const char * data(const char * filename)
    {
        BlobInfo * b = reinterpret_cast<BlobInfo *>(open(filename));
        return b ? reinterpret_cast<const char *>(b->ptr) : nullptr;
    }
#endif

    int read(void * buffer, int element_size, int element_count, Handle file)
    {
        BlobInfo * blob = reinterpret_cast<BlobInfo *>(file);

        size_t count = element_size * element_count;

        if (count > blob->size - blob->offset)
        {
            count = blob->size - blob->offset;
        }

        memcpy(buffer, reinterpret_cast<const unsigned char *>(blob->ptr) + blob->offset, count);

        blob->offset += static_cast<unsigned int>(count);

        return static_cast<int>(count);
    }

    void seek(Handle file, int offset, int origin)
    {
        BlobInfo * blob = reinterpret_cast<BlobInfo *>(file);

        switch (origin)
        {
            case SEEK_SET:
                blob->offset = offset;
                break;
            case SEEK_CUR:
                blob->offset += offset;
                break;
            case SEEK_END:
                blob->offset = blob->size - offset;
                break;
        }
    }

    int tell(Handle file)
    {
        return reinterpret_cast<BlobInfo *>(file)->offset;
    }
}