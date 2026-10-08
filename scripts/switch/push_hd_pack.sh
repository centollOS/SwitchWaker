#!/usr/bin/env bash
# Copies a converted HD texture pack (scripts/hd/build_hd_pack.sh) to the Switch's SD card over
# USB (MTP), where the native port looks for it by default:
#   sdmc:/switch/switchwaker/native/user/hd_textures/{index.bin,data00.bin,...}
#
#   scripts/switch/push_hd_pack.sh [PACK_DIR] [--verify]      (default PACK_DIR: build/hd-pack-512)
#
# Every file is sent by name with switch_mtp push, which looks the remote folder and file up by
# name in its own session and replaces it: object IDs are never reused across sessions (the
# console's MTP server renumbers them after every delete). The data files go first and index.bin
# last, so an interrupted copy leaves at most an index that names unreadable textures (they fail to
# load and the original textures stay). index.bin is always read back and compared; --verify also
# reads back every data file (about 1 GiB each, slow). A data file left over from a bigger pack
# is harmless (index.bin says how many there are); delete it by name if space matters.
# Then turn it on in the game's options menu (Gráficos > Texturas HD).
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
pack="$root/build/hd-pack-512"
verify=0
for arg in "$@"; do
    case $arg in
        --verify) verify=1 ;;
        -h|--help) sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) pack=$arg ;;
    esac
done
remote=switch/switchwaker/native/user/hd_textures
[[ -s $pack/index.bin && -s $pack/data00.bin ]] || { echo "push_hd_pack: no pack in $pack" >&2; exit 1; }
count=$(python3 -c "import struct,sys; print(struct.unpack_from('<I', open(sys.argv[1],'rb').read(), 16)[0])" "$pack/index.bin")

tool_dir="$root/build/switch-tools"
tool="$tool_dir/switch_mtp"
source="$root/scripts/switch/switch_mtp.c"
if [[ ! -x $tool || $source -nt $tool ]]; then
    mkdir -p "$tool_dir"
    flags=$(pkg-config --cflags --libs libmtp) || { echo "push_hd_pack: libmtp not found (brew install libmtp)" >&2; exit 1; }
    # shellcheck disable=SC2086
    cc -O2 -Wall -o "$tool" "$source" $flags
fi

staging=$(mktemp -d)
trap 'rm -rf "$staging"' EXIT
check() { # local readback
    local want got
    want=$(shasum -a 256 "$1" | cut -d' ' -f1)
    got=$(shasum -a 256 "$2" | cut -d' ' -f1)
    [[ $want == "$got" ]] || { echo "push_hd_pack: read-back of $(basename "$1") does not match" >&2; exit 1; }
    echo "verified $(basename "$1") $want"
}
for ((i = 0; i < count; i++)); do
    f=$(printf '%s/data%02d.bin' "$pack" "$i")
    if [[ $verify == 1 ]]; then
        "$tool" push "$f" "$remote" "$staging/readback.bin"
        check "$f" "$staging/readback.bin"
        rm -f "$staging/readback.bin"
    else
        "$tool" push "$f" "$remote"
    fi
done
"$tool" push "$pack/index.bin" "$remote" "$staging/index.bin"
check "$pack/index.bin" "$staging/index.bin"
echo "push_hd_pack: $count data file(s) and index.bin in sdmc:/$remote ($(du -sh "$pack" | cut -f1))"
