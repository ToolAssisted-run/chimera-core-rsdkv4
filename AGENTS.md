# AGENTS.md - Retro Engine v4 core for Chimera

This repository builds the RSDKv4 decompilation (Retro Engine v4, the engine
of the 2013 remasters of Sonic 1 and Sonic 2) as a core for Chimera
(https://github.com/ToolAssisted-run/chimera). It is a GAME core
(`"kind": "game"`). It produces one file, `rsdkv4.chimeraCore`. The game's
data file is never in the repository or the package: the user provides it.

## Layout

- `extern/RSDKv4`: the decompilation, a submodule. Never edited in place.
- `extern/{SDL,ogg,vorbis}`: library submodules.
- `patches/`: numbered patches for `extern/RSDKv4`, applied by
  `waterbox/apply-patches.sh`.
- `waterbox/sources.mk`: the sources, defines and link wraps both builds
  share. `native.mk`, `guest.mk`: the two builds.
- `waterbox/rsdk-driver.cpp`: the machine (a frame, the sound, the picture).
  `rsdk-files.c`: every file the engine opens. `seams.c`: what SDL is
  answered with, OpenGL included (OSMesa). `setup-mesa.sh`: builds Mesa. `detmath.c`: the math functions. `game-state.cpp`: the
  memory domains and properties. `wbx-entry.c`: the guest's exports.
- `waterbox/waterbox.config`, `file_slots.json`, `default_keybinds.json`,
  `package-licenses.json`: the declarations.
- `waterbox/run-gate.sh`: the core's tests. `waterbox/tests/`: the test game
  generator, its tune and movie, the frontend tests.
- `build/`: every output. `tests/roms-local/`: game data, ignored by git.

## Commands, in order

```
git submodule update --init --recursive
cd waterbox
bash setup-mesa.sh -m <miniBox> && bash setup-mesa.sh -n
./run-gate.sh -m <miniBox> [-1 <Sonic 1 data>] [-2 <Sonic 2 data>]
./tests/run-frontend.sh --chimera-root <chimera>
./build-package.sh -m <miniBox>
```

`docs/BUILDING.md` has the requirements and the contract tests.

## Rules

- Upstream is changed only by a numbered patch. The decompilation's licence
  (LICENSE-RSDK.txt) covers the patches: keep it in the repository and the
  package, keep `RSDK_AUTOBUILD` defined, distribute no game asset.
- Native and sandbox must stay the same machine: anything that reads a
  clock, a thread, the C library's math or rand is answered by the core;
  both link the same Mesa, statically.
- Every new test is broken on purpose once and seen to fail.
- No game data in the repository, in CI or in an issue.
- Plain language and ASCII in documents. Commits: `type(scope): what`, a
  short body with the numbers, explicit paths staged.
