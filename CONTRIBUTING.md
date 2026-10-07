# Contributing

## License of contributions

Contributions are accepted under the project's license, MIT ([LICENSE](LICENSE)), except:

- `game/` (the decompiled game): changes stay under its CC0-1.0 ([game/LICENSE](game/LICENSE)).
  Keep the original code paths intact and put PC changes under `TARGET_PC`.
- Files that carry another SPDX header (`native/dsp_hle/`: GPL-2.0-or-later), whose changes stay
  under that license.
- Patches to third-party projects, which follow the license of the project they patch.
By contributing you agree to this for every commit you submit.

## Developer Certificate of Origin

Every commit must be signed off (`git commit -s`), which adds a line

    Signed-off-by: Your Name <you@example.com>

and certifies the [Developer Certificate of Origin 1.1](https://developercertificate.org/):

> By making a contribution to this project, I certify that:
>
> (a) The contribution was created in whole or in part by me and I have the right to submit it
> under the open source license indicated in the file; or
>
> (b) The contribution is based upon previous work that, to the best of my knowledge, is covered
> under an appropriate open source license and I have the right under that license to submit that
> work with modifications, whether created in whole or in part by me, under the same open source
> license (unless I am permitted to submit under a different license), as indicated in the file; or
>
> (c) The contribution was provided directly to me by some other person who certified (a), (b) or
> (c) and I have not modified it.
>
> (d) I understand and agree that this project and the contribution are public and that a record of
> the contribution (including all personal information I submit with it, including my sign-off) is
> maintained indefinitely and may be redistributed consistent with this project or the open source
> license(s) involved.

## No game data

Never commit anything derived from the disc: disc images, `main.dol`, RELs, extracted or generated
assets, screenshots of the game, pipeline caches made from it, or builds (`.nro`, `switchwaker`). Generated
files stay under `build/` and fetched sources under `ref/`, both ignored by git.

## Checks

Before sending a change: build the Mac target and run `native/tools/regress.sh`; for changes
under `switch/` or `scripts/switch/`, also build the NRO with `scripts/switch/build_native.sh`.

## Names

In the project's own files, names and commit messages, refer to the game as "the game" (or by its
disc ID, `GZLE01`), never by its title or by trademarks of its publisher. The game's own
identifiers (function, file and stage names the code has to use) are fine where the code needs
them, and nothing inside `game/` is renamed. Prefixes: `cos_`/`COS_` for code, CMake and
environment variables; `switchwaker` for files, directories and binaries.
