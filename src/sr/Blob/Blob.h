#pragma once

namespace Blob
{
    using Handle = void *;

    Handle open(const char * filename);

    int read(void * buffer, int element_size, int element_count, Handle file);
    void seek(Handle file, int offset, int origin);
    int tell(Handle file);

#ifdef SR_GNW
    // the file in place (the pack), or nullptr
    const char * data(const char * filename);
#endif
}
