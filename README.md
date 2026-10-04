# centollOS

## What it is

centollOS is an unofficial native port of a 2002 GameCube adventure game (disc ID `GZLE01`, USA,
revision 0) to the Mac and the Switch (homebrew). The game is compiled from its decompilation
(the [`game/`](game/README.md) folder) and drawn with [Aurora](https://github.com/encounter/aurora);
there is no CPU emulation.

## What you need

- **Your own legally obtained copy of the disc**: `GZLE01` revision 0, as an uncompressed `.iso`.
  The build generates what it needs from it.
- **This repository contains no game assets, executables or data.** The binaries you build contain
  code generated from your disc: they are for your own use only; do not share them.
- **No release binaries are provided** for now, precisely because they carry headers generated
  from the disc compiled in.

## Build on the Mac (Apple silicon)

Requirements: Xcode command line tools, `brew install cmake ninja python` (Python 3.10 or newer)
and the network the first time.

```sh
git clone https://github.com/centollOS/tww centollos && cd centollos
export COS_DISC=/path/to/GZLE01.iso          # every script and the game read the disc from here
native/tools/fetch_recompcore.sh             # ref/recompcore: Dolphin's DSP HLE (audio)
native/tools/gen_assets.sh                   # build/native-mac/assets/GZLE01: headers from your disc
cmake -S native -B build/native-mac -G Ninja
ninja -C build/native-mac centollos          # build/native-mac/centollos
build/native-mac/centollos                   # a 960x720 window
```

Details, run options and tests: [native/README.md](native/README.md). Full regression:
`native/tools/regress.sh`.

## Build for the Switch

On top of the above: Docker Desktop or Podman. `scripts/switch/build_native.sh` produces
`build/switch-native/centollos.nro` (Mesa, Dawn and Aurora are built in a devkitPro container).
Full guide: [docs/SWITCH_BUILD.md](docs/SWITCH_BUILD.md).

## Install on the Switch

- Copy `centollos.nro` to `sdmc:/switch/centollos/` and your disc as
  `sdmc:/switch/centollos/GZLE01.iso` (`scripts/switch/push.sh` does it over USB/MTP).
- Open it from the Homebrew Menu in title mode (hold **R** while opening an installed game).
- Optional: a HOME-screen icon through the forwarder (`scripts/switch/build_forwarder.sh`, guide in
  [switch/forwarder/INSTALL.md](switch/forwarder/INSTALL.md)).
- If you used this port's earlier SD folder, move its data (save, settings, caches, HD textures) to
  `sdmc:/switch/centollos/`; the old path is no longer read.

## Options

- In-game options menu: **ZL+ZR+Minus** on the Switch, **F1** (or L+R+Z) on the Mac. Saved to
  `native/user/settings.ini`.
- `COS_*` variables (environment on the Mac, `sdmc:/switch/centollos/native/env.txt` on the
  Switch): [switch/native/env.example.txt](switch/native/env.example.txt) and
  [native/README.md](native/README.md).
- HD textures (optional, a Dolphin-format pack you supply): [docs/HD_TEXTURES.md](docs/HD_TEXTURES.md).
- 16:9 widescreen: [docs/WIDESCREEN.md](docs/WIDESCREEN.md).

## Status

Boots and plays on the Mac and the Switch, with sound, controllers, memory-card saves and an
options menu; testing so far covers mostly the opening island and a few other areas. The Switch
does not yet hold a stable 30 fps everywhere.
Known issues: [docs/NATIVE_PORT_PLAN.md](docs/NATIVE_PORT_PLAN.md), "Known bugs".

## License

- The project's own code: **MIT** ([LICENSE](LICENSE)).
- `game/` (the decompilation): **CC0-1.0** ([game/LICENSE](game/LICENSE)).
- `native/dsp_hle/` (audio, over Dolphin's DSP HLE): **GPL-2.0-or-later**
  ([native/dsp_hle/LICENSE](native/dsp_hle/LICENSE)). Every build links it today, so **the
  resulting binaries are GPL**.
- Other components: [THIRD_PARTY.md](THIRD_PARTY.md). Contributing: [CONTRIBUTING.md](CONTRIBUTING.md).

## Disclaimer

Independent project, not affiliated with or endorsed by any video game company. Trademarks
mentioned or alluded to belong to their respective owners. No game content is included or
distributed.

---

## Credits

- The community decompilation project and its contributors
  ([github.com/zeldaret/tww](https://github.com/zeldaret/tww)): the decompiled game.
- The fork the decompilation was imported from, which completes its remaining functions
  ([github.com/snrubrm/tww](https://github.com/snrubrm/tww)).
- [encounter/aurora](https://github.com/encounter/aurora): the GameCube SDK over WebGPU; Dawn
  (Google, and encounter's fork for the Switch).
- [Dusklight](https://github.com/TwilitRealm/dusklight) (TwilitRealm): the model for the SDK layer
  and CC0 helper code.
- [Dolphin](https://dolphin-emu.org): the DSP HLE.
- elliotttate's recompilation project
  ([github.com/elliotttate/Wind-Waker-Recomp](https://github.com/elliotttate/Wind-Waker-Recomp)) and
  [RecompCore](https://github.com/elliotttate/RecompCore): where this work started and where the
  DSP adapter comes from.
- [Mesa](https://mesa3d.org), [devkitPro](https://devkitpro.org),
  [libnx](https://github.com/switchbrew/libnx) and switch-mesa: the Switch toolchain and graphics.
- [nx-hbloader](https://github.com/switchbrew/nx-hbloader),
  [hacBrewPack](https://github.com/TooTallNate/hacBrewPack) and
  [hactool](https://github.com/SciresM/hactool): the HOME-menu forwarder.
- Authors: Pulpparty, depende3000, and the centollOS contributors.

Full list of third-party components and licenses: [THIRD_PARTY.md](THIRD_PARTY.md).
