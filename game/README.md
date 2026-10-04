# game/: the decompiled game

This folder is the decompiled source code of the game centollOS builds (disc ID `GZLE01`, USA
revision 0). It is not the project's own code and keeps its own license, CC0-1.0
([LICENSE](LICENSE)).

## Origin

- Imported from <https://github.com/snrubrm/tww> at commit `b09eebc` (recorded in
  [UPSTREAM](UPSTREAM), which `native/tools/gen_assets.sh` reads), a fork of the community
  decompilation project <https://github.com/zeldaret/tww> that completes its remaining functions.
- The import commit holds `src/`, `include/` and `LICENSE` exactly as published there. Later
  commits change some files for the native build, always under `#if TARGET_PC` / `#ifdef
  TARGET_PC` blocks (or in lines that refer to the native port's own headers and variables); the
  original GameCube code paths are kept.

Credit for this code goes to the contributors of those projects. Names, strings and identifiers
in it are the game's own and are left as they are.

## No game data

Only source code is here. The asset headers some files include (`assets/...`) are generated from
the player's own disc at build time under `build/` and are never committed.
