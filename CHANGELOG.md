# Changelog

Release tags match a section heading (`## [vX.Y.Z] - date`): a release made
on github.com with that tag makes the CI build and attach
`SecondReality-vX.Y.Z.zip`.

## [v1.0.0] - 2026-10-30

First version: Second Reality (Future Crew, 1993), from Second Reality++
(XorJS, commit a4abc2a), as a GWHB homebrew for Retro-Go SD v2.0.0-rc1+.

- The whole demo, its 19 parts and the music (st3play), at 70 Hz like the
  PC (the LCD runs at 70 Hz). The 320x200, 320x400 and 640x350 pictures are
  scaled to the 320x240 LCD (lines averaged).
- The data (4 MB) in `SecondReality.srd`, beside the `.bin`, read in place
  from the external flash.
- Pause menu: go to a part, loop the demo. GAME = next part, TIME =
  previous part; A or B in the end scroller: back to the launcher. Saves: the part being played (the demo starts again from
  there, also with "Resume").
- `/data/secondreality/SecondReality/log.txt`: a line per part (pictures
  per second, idle time, missed sound) for the tests on the console.
