// PC tool: the samples of the two songs, ready for st3play's mixer (the
// loader of src/sr/Music/st3play.cpp runs here, SR_ST3_EXPORT), written as
// pack entries "st3:<song>:<instrument>" (tools/sr_pack.py).
//
//   c++ -std=c++20 -DSR_GNW -DSR_ST3_EXPORT -Isrc/sr/Music -Isrc/port \
//       tools/sr_music.cpp src/sr/Music/st3play.cpp src/sr_data/Music/REALITY.FC.cpp
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "st3play.h"
#include "REALITY.FC.h"

extern int sr_st3_song;
static FILE * out;

void sr_st3_export_sample(int song, int ins, const void * data, uint32_t size)
{
    char name[32];
    snprintf(name, sizeof(name), "st3:%d:%d", song, ins);
    uint32_t l = (uint32_t)strlen(name);
    fwrite(&l, 4, 1, out);
    fwrite(name, 1, l, out);
    fwrite(&size, 4, 1, out);
    fwrite(data, 1, size, out);
}

// st3play's link to the audio output (not used here)
namespace AudioPlayer
{
    uint32_t getSamplePosition() { return 0; }
    bool openMixer(uint32_t, int32_t & samplesLeft, uint32_t & totalSampleCount)
    {
        samplesLeft = 0;
        totalSampleCount = 0;
        return true;
    }
    void closeMixer() {}
    void Update(bool) {}
}

extern "C" void sr_fatal(const char * message, const char * detail)
{
    fprintf(stderr, "%s %s\n", message, detail ? detail : "");
    exit(1);
}
extern "C" const void * sr_pack_data(const char * name, uint32_t) { sr_fatal("sr_pack_data", name); return 0; }

int main(int argc, char ** argv)
{
    if (argc != 2) return 2;
    out = fopen(argv[1], "wb");
    if (!out) return 1;
    for (int song = 0; song < 2; song++)
    {
        const unsigned int offset = ((const unsigned int *)Music::Data::reality_fc)[song];
        sr_st3_song = song;
        if (!st3play::PlaySong(Music::Data::reality_fc + offset, Music::Data::reality_fc_size, true, 44100, 0))
        {
            fprintf(stderr, "sr_music: cannot load song %d\n", song);
            return 1;
        }
        st3play::Close();
    }
    fclose(out);
    return 0;
}
