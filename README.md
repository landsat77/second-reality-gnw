# Second Reality for the Game & Watch (Retro-Go SD)

[Second Reality](https://en.wikipedia.org/wiki/Second_Reality) by Future
Crew (Assembly 1993) on the Nintendo Game & Watch, as a homebrew for
[Retro-Go SD](https://github.com/sylverb/game-and-watch-retro-go-sd)
(v2.0.0-rc1 or newer).

It is [Second Reality++](https://github.com/XorJS/SecondRealityPlusPlus)
(XorJS's C++ port of the demo, after Conspiracy's Win32 port of the source
code Future Crew released in 2013), made to fit the console: 1 MB of RAM
for the program, the demo and the screen.

Installation: [INSTALL.md](INSTALL.md) (English / français). The release
zip has everything (the demo's data are public domain, like its code).

## How

- **Data**: the demo's arrays (pictures, fonts, tables, the two songs) are
  in `src/sr_data` as in Second Reality++. `tools/sr_data.py` compiles them
  on the PC and writes them to the pack (`SecondReality.srd`, read in place
  in the external flash); the code sees each array as a reference bound at
  start-up (`build/gen/*.h`). The samples of the music come ready for the
  mixer (`tools/sr_music.cpp` runs st3play's loader on the PC).
- **Memory**: the parts were separate programs: their variables share one
  area, cleared when a part starts (`tools/sr_overlays.py`, a linker
  OVERLAY). Some parts use the VGA memory they do not show (Alku, Techno,
  Panicfake); a few tables are copied from the pack into the part's memory
  when it starts. The code of the jelly zoom is a table of operations
  (`tools/jp_zoom_gen.py`, 76 KB of code less).
- **Picture**: the LCD runs at 70 Hz, the VGA refresh rate of the demo; one
  VBL of the demo is one LCD frame. 320x200, 320x400 and 640x350 are scaled
  to 320x240 (lines averaged, RGB565).
- **Sound**: st3play mixes the music ahead into the firmware's sound ring;
  the demo follows what is heard (it syncs on the music's position).

## Build

    sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib libsdl2-dev python3-pil
    make            # build/sd/homebrews/SecondReality.bin + .srd
    make host       # PC version: ./SecondReality_host build/sd/homebrews/SecondReality.bin
    make check      # the PC version plays 700 s of the demo, pictures compared

`tests/qemu` runs the console's code (same compiler and flags) on QEMU's
Cortex-M7 (`make -C tests/qemu run`): crashes, ARM-only differences and an
estimate of the CPU time per part.

The PC version takes environment variables for tests (`host/host_sr.h`):
`SR_FAST=1` (virtual time), `SR_SECONDS`, `SR_JUMP="vbl:part"`, `SR_SHOTS`,
`SR_HASH`, `SR_WAV`, `SR_CPU=1` (CPU time per part).

## Credits

- Second Reality: Future Crew — code Psi, Trug, Wildfire; graphics
  Marvel, Pixel; music Purple Motion, Skaven.
- Win32 port: Gargaj / Conspiracy. Second Reality++: XorJS.
- st3play: Olav "8bitbubsy" Sørensen.
- Retro-Go SD and its SDK: sylverb and contributors.

License: [LICENSE](LICENSE) (the Unlicense; the SDK is MIT).
