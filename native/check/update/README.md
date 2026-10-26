# Update test release

A fake release `v9.9.9` for the update smoke tests (`native/check/input/menu-update*.txt`, the script's
`#update-release` line; issues #27/#28, `docs/AUTO_UPDATE_PLAN.md`): `latest.json` is a GitHub
`releases/latest` answer whose asset URLs are files next to it, and the zip holds the three files of a
real release (`switch/switchwaker/`) with one line of text each. The European zip is a copy of the USA
one under its own name, so the smokes pass on both builds.

Made with:

```sh
mkdir -p t/switch/switchwaker
printf 'fixture nro v9.9.9\n' > t/switch/switchwaker/switchwaker.nro
printf 'fixture pipeline cache\n' > t/switch/switchwaker/initial_pipeline_cache.db
printf 'fixture dksh cache\n' > t/switch/switchwaker/initial_dksh_cache.bin
touch -t 202610230000 t/switch t/switch/switchwaker t/switch/switchwaker/*
(cd t && zip -qrX ../SwitchWaker-v9.9.9-switch.zip switch)
shasum -a 256 SwitchWaker-v9.9.9-switch.zip > SwitchWaker-v9.9.9-switch.zip.sha256
cp SwitchWaker-v9.9.9-switch.zip SwitchWaker-v9.9.9-switch-eur.zip
shasum -a 256 SwitchWaker-v9.9.9-switch-eur.zip > SwitchWaker-v9.9.9-switch-eur.zip.sha256
```

then the zips' sizes into `latest.json`.
