# Retro Engine v4 (Sonic 1 and 2, 2013) for Chimera

This repository builds the RSDKv4 decompilation, a decompilation of
Christian Whitehead's Retro Engine v4, as a game core for
[Chimera](https://github.com/ToolAssisted-run/chimera), a frontend for
tool-assisted speedruns. Retro Engine v4 is the engine of the 2013
remasters of Sonic the Hedgehog and Sonic the Hedgehog 2. The core plays
them frame by frame, the same every time, with savestates, rewinding and
movies, menus included.

The build makes one file, `rsdkv4.chimeraCore`. Put it in Chimera's `Cores`
folder.

## What you bring

The game's data file, as the project's firmware. This package contains none
of the games.

- Sonic the Hedgehog: the Android release (checked) has it in its APK as
  `assets/Data.rsdk.xmf`. SHA-1 `3B49570C26B469B05BD01EF43995310E7066989E`.
- Sonic the Hedgehog 2: the Android release (checked), the same place.
  SHA-1 `8C7B963B3591C27E43DC27CD25B3C07735BE575C`.

Other releases' `Data.rsdk` files are the same kind of file; none was tried.
A file of your own can be used in the original's place, and the project
then records its hash.

## Settings

The game (Sonic 1 or 2), language, screen width (320 to 424 pixels, 424 by
default), Skip Start Menu, the picture's scale (1 or 2) and the random seed.
Each is recorded in the project.

## How it draws

The stages are drawn by the engine's software renderer; the remasters'
menus, 3D title and touch controls only by its OpenGL renderer. The core
carries its own OpenGL for that, Mesa's software rasteriser, so every
machine draws the same picture and no graphics card is used.

## What it does not do yet

- Sonic 2's online two-player mode: Chimera has no network code.
- Mods (the decompilation's mod loader) are off.
- The downloadable content is off in every build, as the decompilation's
  licence requires.

[docs/PLAN.md](docs/PLAN.md) says what was decided and measured,
[docs/BUILDING.md](docs/BUILDING.md) how to build and test it.

## Licence

This repository's own files are MIT. The engine is the RSDKv4 decompilation
(by Rubberduckycooly and st&times;tic, of the Retro Engine by Christian
"Taxman" Whitehead), under its own licence, in
[LICENSE-RSDK.txt](LICENSE-RSDK.txt): no use for profit, credit to its
authors, downloadable content off, no game assets distributed. A package
built from here is under those terms. [LICENSE](LICENSE) says what each part
is under.
