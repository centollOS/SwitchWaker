<!-- DRAFT: placeholders {{NAME}} and {{LICENSE}} until the project's name and license are decided.
     User-visible names (centollos.nro, sdmc:/switch/centollos/, COS_* variables) are the
     current ones; they change in the rename step (proposal section 7). -->

# {{NAME}}

[Español](#español) · [English](#english)

---

## Español

### Qué es

{{NAME}} es un port nativo y no oficial de un juego de GameCube de 2002, the game (USA, `GZLE01` revisión 0), a Mac y a Switch (homebrew). El juego se compila
desde su decompilación y se dibuja con [Aurora](https://github.com/encounter/aurora); no hay
emulación de la CPU.

### Qué necesitas

- **Tu propia copia del disco**, obtenida legalmente: `GZLE01` revisión 0, como `.iso` sin comprimir.
  El build genera lo que necesita a partir de ella.
- **Este repositorio no incluye ningún asset, ejecutable ni dato del juego**, y los binarios que
  compiles contienen código generado a partir de tu disco: son solo para tu uso.

### Compilar

- Mac (Apple Silicon): {{TODO: resumen de `native/README.md`, "Quick start (Mac)"}}
- Switch: {{TODO: resumen de `docs/SWITCH_BUILD.md`}}

### Instalar en la Switch

{{TODO: `centollos.nro` en `sdmc:/switch/centollos/`, el disco al lado; forwarder opcional
(`switch/forwarder/INSTALAR.md`).}}

### Opciones

{{TODO: `env.txt` (`switch/native/env.example.txt`), el menú de opciones, texturas HD
(`docs/HD_TEXTURES.md`), panorámico (`docs/WIDESCREEN.md`).}}

### Estado y problemas conocidos

{{TODO: desde `docs/NATIVE_PORT_PLAN.md`, "Known bugs".}}

### Licencia

{{LICENSE}}. El audio usa el DSP HLE de Dolphin (GPL-2.0-or-later, `native/dsp_hle/`); ver
[THIRD_PARTY.md](THIRD_PARTY.md).

Proyecto independiente: sin relación con the console maker ni respaldado por the console maker. Las marcas pertenecen
a sus dueños.

### Créditos

Ver [Credits](#credits).

---

## English

### What it is

{{NAME}} is an unofficial native port of a 2002 GameCube game, the game
(USA, `GZLE01` revision 0), to the Mac and the Switch (homebrew). The game is compiled from
its decompilation and drawn with [Aurora](https://github.com/encounter/aurora); there is no CPU
emulation.

### What you need

- **Your own legally obtained copy of the disc**: `GZLE01` revision 0, as an uncompressed `.iso`.
  The build generates what it needs from it.
- **This repository contains no game assets, executables or data**, and the binaries you build
  contain code generated from your disc: they are for your own use only.

### Build

- Mac (Apple silicon): {{TODO: summary of `native/README.md`, "Quick start (Mac)"}}
- Switch: {{TODO: summary of `docs/SWITCH_BUILD.md`}}

### Install on the Switch

{{TODO: `centollos.nro` in `sdmc:/switch/centollos/` with the disc next to it; optional
forwarder (`switch/forwarder/INSTALAR.md`).}}

### Options

{{TODO: `env.txt` (`switch/native/env.example.txt`), the options menu, HD textures
(`docs/HD_TEXTURES.md`), widescreen (`docs/WIDESCREEN.md`).}}

### Status and known issues

{{TODO: from `docs/NATIVE_PORT_PLAN.md`, "Known bugs".}}

### License

{{LICENSE}}. Audio uses Dolphin's DSP HLE (GPL-2.0-or-later, `native/dsp_hle/`); see
[THIRD_PARTY.md](THIRD_PARTY.md).

Independent project: not affiliated with or endorsed by the console maker. Trademarks belong to their owners.

---

## Credits

- [the upstream decompilation](https://github.com/zeldaret/tww) and its contributors: the decompilation.
- [the decompilation fork](https://github.com/snrubrm/tww): the fork the decompilation was imported from.
- [encounter/aurora](https://github.com/encounter/aurora): the GameCube SDK over WebGPU; Dawn
  (Google, and encounter's fork for the Switch).
- [Dusklight](https://github.com/TwilitRealm/dusklight) / TwilitRealm: the model for the SDK layer
  and CC0 helper code.
- [Dolphin](https://dolphin-emu.org): the DSP HLE.
- [the upstream recompilation project](https://github.com/elliotttate/Wind-Waker-Recomp) and
  [RecompCore](https://github.com/elliotttate/RecompCore): where this work started and where the
  DSP adapter comes from.
- [Mesa](https://mesa3d.org), [devkitPro](https://devkitpro.org),
  [libnx](https://github.com/switchbrew/libnx) and switch-mesa: the Switch toolchain and graphics.
- [nx-hbloader](https://github.com/switchbrew/nx-hbloader), hacBrewPack and
  [hactool](https://github.com/SciresM/hactool): the HOME-menu forwarder.
- Authors: Pulpparty, depende3000.

Full list of third-party components and licenses: [THIRD_PARTY.md](THIRD_PARTY.md).
