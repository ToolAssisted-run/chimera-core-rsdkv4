# Building and testing the Retro Engine v4 core

## Requirements

Linux, x86-64 (CI uses `ubuntu-latest`). gcc and g++, make, python3, meson,
ninja and python3-mako for Mesa, curl to fetch Mesa's tarball once, ffmpeg
only to remake the test tune, and a Chimera checkout with miniBox built
with its C++ guest toolchain:

```
git clone --recursive https://github.com/ToolAssisted-run/chimera.git <chimera>
mb=<chimera>/extern/chimera-common-minibox
meson setup "$mb/build/meson-linux" "$mb" && meson compile -C "$mb/build/meson-linux"
meson setup "$mb/build/meson-cpp" "$mb" -Dguest_cpp=true && meson compile -C "$mb/build/meson-cpp"
```

For the frontend tests, Chimera built with its window (its README, "Building";
`tools/ci-chimera.sh` does it). The window runs headless and needs no display.

## Sources

- `extern/RSDKv4`: the RSDKv4 decompilation, pinned, with its stb_image
  submodule. Never edited in place: `patches/` is applied by
  `waterbox/apply-patches.sh`, which every build runs first.
- `extern/SDL` (release-2.32.10), `extern/ogg` (v1.3.5), `extern/vorbis`
  (v1.3.7).

- Mesa 24.0.9, fetched by `waterbox/setup-mesa.sh` (pinned by SHA-256) and
  built twice into `build/mesa`: for the guest (`-m <miniBox>`) and for the
  host (`-n`). It takes a while once; later builds find it built.

```
git submodule update --init --recursive
cd waterbox && bash setup-mesa.sh -m <miniBox> && bash setup-mesa.sh -n
```

## Building

```
cd waterbox
make -f native.mk -j$(nproc) MB=<miniBox>   # build/native/run-native, run-wbx
make -f guest.mk -j$(nproc) MB=<miniBox>    # build/guest/core.wbx
./build-package.sh -m <miniBox>             # build/package/rsdkv4.chimeraCore
./build-package.sh -r <chimera>             # into <chimera>/build/Cores
```

The scripts find miniBox from `-m`, else `MINIBOX_DIR`, else
`~/chimera/extern/chimera-common-minibox`.

## Testing

```
./waterbox/run-gate.sh -m <miniBox>                       # the core's tests
./waterbox/run-gate.sh -m <miniBox> -1 <Sonic 1 data> -2 <Sonic 2 data>
./waterbox/tests/run-frontend.sh --chimera-root <chimera> # the frontend's
```

Then Chimera's contract tests against the package:

```
CHIMERA_CORES_DIR=<folder with the package> <chimera>/build/meson-linux/source/ui/test-packages
```

`run-gate.sh` without `-1` and `-2` needs no game: it plays the test game
that `waterbox/tests/make-test-game.py` writes. With them it also plays 4100
frames of each game's Android data from power-on through the start menu into
the first zone. Keep the data files outside the repository, or in
`tests/roms-local/`, which git ignores.

## What CI does

`.github/workflows/chimera.yml`: builds Chimera and miniBox, builds Mesa
(cached by setup-mesa.sh's hash), runs
`run-gate.sh` and `run-frontend.sh` on the test game, builds the package
stamped with the commit, runs the contract tests, and publishes through
Chimera's `publish-core.yml`.

## Troubleshooting

- "the engine did not start": the message says which - not a Retro Engine
  v4 data file, no `Data/Game/GameConfig.bin` in it, or no OpenGL.
- "no host Mesa" or "no guest Mesa": run `setup-mesa.sh -n` or `-m`.
- A change to SDL's, the codecs' or the engine's flags rebuilds everything:
  every object depends on the flags it was built with.
- Native and sandbox differ in sound only: a math function the codecs call
  is not wrapped (sources.mk MATH_FUNCS).
