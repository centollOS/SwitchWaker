#!/usr/bin/env bash
# Pull the console's own pipeline cache over USB (MTP), so the pipelines used in play can be added to
# the bundled list (native/tools/gen_pipeline_cache.sh --merge-from; docs/SWITCH_BUILD.md,
# "Growing the list from the console").
#
#   scripts/switch/pull_pipeline_cache.sh [OUT]
#
# Copies sdmc:/switch/centollos/native/user/cache/pipeline_cache.db and, if there, its
# pipeline_cache.db-journal (the Switch build keeps a PERSIST rollback journal next to it: Aurora
# Switch patch 0006) and -wal, into a staging directory under the same names, then opens the copy
# with sqlite3 read-write: a journal whose header was zeroed at the last commit is ignored, a hot one
# (a commit cut short when the app was closed with HOME) is rolled back on the copy, as Aurora would
# at its next start. The result is written as one self-contained file (VACUUM INTO, no journal) to
# OUT, default build/pipeline-cache/console/<timestamp>.db; the raw files stay next to it in
# <timestamp>.raw/. The database is pulled twice, before and after its journal, and must match, so a
# copy taken while the app was writing is pulled again (close the app first: it writes its cache
# whenever a new pipeline is used). Checks the result (sqlite quick_check, Aurora schema 1) and prints
# its rows by type and config version.
#
# Enable USB file transfer on the console first (Horizon's own, haze or DBI), as for push.sh. Needs
# libmtp (macOS: brew install libmtp) and sqlite3; runs on the host. The file holds pipeline keys
# derived from the player's disc: it stays under build/, out of git.
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
remote_dir=switch/centollos/native/user/cache
name=pipeline_cache.db
tool_dir="$root/build/switch-tools"
tool="$tool_dir/switch_mtp"
source="$root/scripts/switch/switch_mtp.c"

command -v sqlite3 > /dev/null || { echo "pull_pipeline_cache: sqlite3 not found" >&2; exit 2; }
if [[ ! -x $tool || $source -nt $tool ]]; then
    mkdir -p "$tool_dir"
    if ! flags=$(pkg-config --cflags --libs libmtp 2>/dev/null); then
        echo "pull_pipeline_cache: libmtp not found (macOS: brew install libmtp)" >&2
        exit 1
    fi
    # shellcheck disable=SC2086  # pkg-config output is a word list
    cc -O2 -Wall -o "$tool" "$source" $flags
fi

stamp=$(date +%Y%m%d-%H%M%S)
out=${1:-$root/build/pipeline-cache/console/$stamp.db}
mkdir -p "$(dirname "$out")"
out="$(cd "$(dirname "$out")" && pwd)/$(basename "$out")"
[[ -e $out ]] && { echo "pull_pipeline_cache: $out already exists" >&2; exit 2; }
raw="${out%.db}.raw"
rm -rf "$raw"
mkdir -p "$raw"

# pull_one NAME [optional]: status 0 pulled, 3 not on the console (quietly if optional).
pull_one() {
    local status=0
    if [[ ${2:-} == optional ]]; then
        "$tool" pull "$remote_dir" "$1" "$raw/$1" > /dev/null 2>&1 || status=$?
    else
        "$tool" pull "$remote_dir" "$1" "$raw/$1" > /dev/null || status=$?
    fi
    [[ $status -eq 0 || $status -eq 3 ]] || exit "$status"
    return "$status"
}
sha() { shasum -a 256 "$1" | cut -d' ' -f1; }

consistent=0
for attempt in 1 2 3; do
    rm -f "$raw/$name" "$raw/$name-journal" "$raw/$name-wal" "$raw/$name.again"
    if ! pull_one "$name"; then
        echo "pull_pipeline_cache: no $remote_dir/$name on the console (the app has not run yet?)" >&2
        exit 3
    fi
    pull_one "$name-journal" optional || true
    pull_one "$name-wal" optional || true
    mv "$raw/$name" "$raw/$name.first"
    pull_one "$name" || { echo "pull_pipeline_cache: $remote_dir/$name went away while it was copied" >&2; exit 3; }
    if [[ $(sha "$raw/$name.first") == "$(sha "$raw/$name")" ]]; then
        rm -f "$raw/$name.first"
        consistent=1
        break
    fi
    echo "pull_pipeline_cache: $name changed while it was copied (attempt $attempt); close the app on the console" >&2
    rm -f "$raw/$name.first"
done
[[ $consistent -eq 1 ]] || { echo "pull_pipeline_cache: no consistent copy after 3 attempts" >&2; exit 1; }

journal="none"
if [[ -s $raw/$name-journal ]]; then
    # A rollback journal is hot when its header (the first 8 bytes, the magic) is not zeroed.
    if [[ $(head -c 8 "$raw/$name-journal" | od -An -tx1 | tr -d ' \n') == 0000000000000000 ]]; then
        journal="$(wc -c < "$raw/$name-journal" | tr -d ' ') bytes, not hot (ignored)"
    else
        journal="$(wc -c < "$raw/$name-journal" | tr -d ' ') bytes, hot (rolled back on the copy)"
    fi
fi
[[ -s $raw/$name-wal ]] && journal="$journal; -wal $(wc -c < "$raw/$name-wal" | tr -d ' ') bytes (replayed on the copy)"

# Recover on a working copy (the raw files are kept as pulled), then one clean file.
work="$raw/work"
mkdir -p "$work"
cp "$raw/$name" "$work/$name"
for ext in -journal -wal; do
    [[ -e $raw/$name$ext ]] && cp "$raw/$name$ext" "$work/$name$ext"
done
check=$(sqlite3 "$work/$name" 'PRAGMA quick_check;' 2>&1 | head -1) || true
if [[ $check != ok ]]; then
    echo "pull_pipeline_cache: the console's cache is damaged ($check); raw files in $raw" >&2
    exit 1
fi
sqlite3 "$work/$name" "VACUUM INTO '$out';"
rm -rf "$work"
schema=$(sqlite3 "$out" 'SELECT value FROM aurora_schema' 2>/dev/null | head -1) || true
if [[ $schema != 1 ]] || [[ $(sqlite3 "$out" 'PRAGMA quick_check;') != ok ]]; then
    echo "pull_pipeline_cache: $out is not an Aurora pipeline cache of schema 1" >&2
    exit 1
fi
echo "pulled $remote_dir/$name (journal: $journal)"
echo "rows by type (0 clear, 1 GX, 2 RmlUi) and config version:"
sqlite3 "$out" 'SELECT type, config_version, COUNT(*) FROM pipeline_cache GROUP BY 1, 2;'
echo "total $(sqlite3 "$out" 'SELECT COUNT(*) FROM pipeline_cache') rows: $out"
echo "merge it: native/tools/gen_pipeline_cache.sh --merge-only --merge-from $out"
