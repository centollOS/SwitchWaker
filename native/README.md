# Native port: the game built from its decompilation

`game/` holds the source of the game decompilation: `src/`, `include/` and its license,
imported unchanged from the decompilation fork at `b09eebc` and changed only
by later commits in this repository. That fork builds on the work of the
the upstream decompilation contributors and completes the remaining functions
with AI assistance; it is not part of upstream. Both are CC0-1.0 (`game/LICENSE`).

At the import, the fork's own build (`configure.py`, Metrowerks compilers) reproduced the
player's GZLE01 revision 0 `main.dol` and all 415 RELs byte for byte (`416 files OK`, SHA-1
checked): every function has source, two units are "Equivalent" rather than byte-matching.

The source contains no game data. Some files include `assets/...` headers that the fork's build
generates from the player's disc; this port generates them the same way at build time, outside
the repository, under `build/`.

The goal is a native build of the game on Aurora: on the Mac first, then the Switch, following the
approach of [Dusklight](https://github.com/TwilitRealm/dusklight) (CC0), whose
SDK-over-Aurora layer and static REL linking are the reference. Why: the translated build runs at
about 6-8 percent speed on the Switch (`docs/SWITCH_IMPLEMENTATION_CHECKLIST.md`), and native code
costs about 0.9 host instructions per guest instruction against 27 for the translation.
