# Plan and design log - Retro Engine v4 core

Dated entries; the newest decisions are at the end of each section.

## The request (2026-10-17)

Chimera issue 295 asked for three cores: the Retro Engine v3, v4 and v5
decompilations. The owner asked for each to be analysed and built if it is
technically and legally sound, and for the work to stay local: nothing
pushed, no comment on the issue. One core per engine (the owner's choice).
This one plays the 2013 remasters of Sonic 1 and Sonic 2; the owner supplied
their Android APKs for testing. The Sonic CD core (chimera-core-rsdkv3) was
built first, and this one follows its design; its docs/PLAN.md has the parts
that are the same.

## Feasibility

- **Licence.** The decompilation's licence is the v3 one, word for word
  (LICENSE-RSDK.txt): copying, changing and distributing for public or
  private use, with the downloadable content off by default in any build
  given out, no use for profit, credit, no game assets, the licence with
  modified source. The core defines `RSDK_AUTOBUILD`, declares
  `commercialUse: no`, credits the authors and carries the licence. Mesa is
  MIT, stb_image MIT or public domain.
- **One thread on one clock.** As v3: the engine's main loop is one frame a
  pass (RunFrame, patch 0001). The networking code (Sonic 2's online
  two-player mode, the only other thread) is not built.
- **The picture.** The stages are drawn in software; the remasters' menus,
  3D title, pause menu and touch controls (the native objects) only through
  the engine's OpenGL 1.x renderer, and upstream's build notes say the menus
  are unplayable without it. So the core carries an OpenGL: Mesa 24.0.9's
  softpipe behind OSMesa, built for the guest and for the host from one
  pinned tarball (setup-mesa.sh, the touchHLE core's), plain C with no JIT,
  so every machine draws the same picture.
- **What a runner can test.** A test game written from scratch
  (waterbox/tests/make-test-game.py), in the v4 pack format with its files
  stored unencrypted, which the format allows; it is played with Skip Start
  Menu, having no menus. Its frames go through the OpenGL path too (the
  software screen drawn as a texture).

## How the core is built (2026-10-17)

As the v3 core (real SDL 2 in its minimal configuration; the files from
memory; the math functions, rand and the clocks answered by the core), and:

- **OpenGL** (seams.c). SDL's window is made without OpenGL (the dummy
  driver has none); SDL_GL_CreateContext makes an OSMesa context on a buffer
  in guest memory, sized as the window; SDL_GL_SwapWindow finishes the frame
  and the driver reads the picture from the buffer. GLEW is replaced by a
  header (compat/GL/glew.h): every GL function is linked in.
- **Math.** Mesa calls more of them than the engine; the wraps cover sin,
  cos, tan, the arc functions, exp, log, pow, exp2, log2, log10 and their
  float forms, for every object of the link, Mesa's included. The native
  reference links Mesa statically for the same reason.
- **Debug info** is stripped from the guest at link (Mesa's build keeps it):
  the guest is 22 MB, the package 5.9 MB.
- **The data files** are firmware by game: Sonic1.rsdk and Sonic2.rsdk, the
  game setting choosing (the System in Chimera).

## Measured (2026-10-17)

- Sonic 1 and Sonic 2, Android data: the SEGA and Christian Whitehead
  screens, the 3D title with its spinning box, the start menu (Start Game,
  save select, player select), the games' own SEGA screen and title, and the
  first zone, played with the pad. Pictured frame by frame.
- Each game, 4100 frames from power-on into the first zone: native =
  sandbox (picture, sound, frame lengths, lag, memory); rerecord (a save and
  a load before every frame) = straight; a session (a state saved at frame
  3690, loaded into a new process) = straight. State: 29 MB, most of it the
  OpenGL's textures.
- Speed: 1400 frames of the 3D title in 13 s in the sandbox (about 107
  frames a second); 4100 frames with a save and a load before each, 80 s.
- The test game: the core's tests (equivalence, rerecord, session, turbo,
  input, saves, pokes, the seed, the screen width, the render scale, the
  game choice, five refusals, the clock, the firmware names) pass; the
  frontend tests 9; contract tests 20.
- Each new test was broken on purpose and seen to fail: turbo (the OpenGL
  picture converted whatever the turbo), the seed, the render scale, the
  game choice and the firmware names. The tests shared with the v3 core
  were broken in that core.

## Not done

- Other releases of the data (Sonic Origins, iOS, the Android Forever
  versions): not tried.
- Mods: the mod loader is off. Sonic 2's online two-player mode: not built.
- Windows: the package has not been run on Windows yet.
- Lag: a frame counts as reading the controls unless it is a stage's load
  frame; the menus read the pad every frame.
