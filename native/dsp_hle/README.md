# native/dsp_hle: Dolphin's DSP HLE (isolated, GPL-2.0-or-later)

This folder is the only part of centollOS under the GNU General Public License: its files are
GPL-2.0-or-later ([LICENSE](LICENSE), SPDX headers in each file). They adapt the DSP HLE backend
of an earlier recompilation project by elliotttate (see [THIRD_PARTY.md](../../THIRD_PARTY.md))
and compile Dolphin's DSP HLE sources from RecompCore (`native/cmake/dsp_hle.cmake`,
`native/tools/fetch_recompcore.sh`), which are GPL-2.0-or-later as well.

**Binaries built with it are GPL.** The build links this component (today every build does: there
is no build option without it yet, since the game's audio code needs a DSP) into the Mac
executable (`build/native-mac/switchwaker`) and the Switch NRO (`switchwaker.nro`); whoever
distributes such a binary must do so under GPL-2.0-or-later terms, with its corresponding source.
The rest of the repository stays under its own licenses (MIT for the project's code, CC0-1.0 for
`game/`); the GPL applies to this folder and to the combined binaries.

The goal is to replace it with a DSP implementation of the project's own, after which no build
needs it.
