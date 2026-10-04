#!/usr/bin/env bash
# Record the pipelines the game uses on the Mac and merge them into Aurora's bundled pipeline cache,
# initial_pipeline_cache.db, for the Switch build to precompile at boot (docs/SWITCH_BUILD.md,
# "Pipeline precompile").
#
#   native/tools/gen_pipeline_cache.sh [--out DIR] [--no-build] [--no-sweep] [--no-prologue]
#                                      [--sweep-jobs N] [--sweep-frames N] [--sweep-only LIST]
#                                      [--disc PATH] [--merge-from DB [--tier N]]...
#   native/tools/gen_pipeline_cache.sh --merge-only --merge-from DB [--tier N] [--merge-from ...]
#   native/tools/gen_pipeline_cache.sh --mark-priority DB
#
# Aurora's pipeline cache keeps pipeline *configurations* (GX TEV/blend/vertex-format state, clear
# and RmlUi pipeline keys), not compiled code, so the Mac's Metal runs record the same set the
# Switch's OpenGL ES backend needs. Each run below gets its own cache directory
# (COS_CACHE_PER_RUN=1: <run dir>/cache/pipeline_cache.db). The runs, in order of the tiers the
# merged file sorts by (Aurora queues its rows by first_frame_used; the tier is added in units of
# 10,000,000 frames, so the Switch compiles the boot path first):
#   0  file-select     logos, opening, title, file select (native/check/input/file-select.txt)
#   1  new-game        title -> name entry -> OPEN scene (native/check/input/new-game.txt)
#   2  outset-real     the new-game prologue to the player free in Outset (capped, ~7 min; --no-prologue
#                      skips it)
#   3  outset-control  debug boot into Outset, Aryll's lookout event, the player controllable
#   4  boot-sweep      every stage of the disc, --sweep-frames frames each (--no-sweep skips it)
# Tiers 0-3 run in parallel, then the sweep. A run that fails still contributes what it recorded
# (the report says so). The merge keeps one row per (type, hash), the lowest tier's frame.
#
# The file also gets a pipeline_priority table (type, hash, priority): priority 0 for the rows of
# tiers 0-3 (the boot path, logos to Outset: priority_tiers below), 1 for the rest. The Switch
# build's Aurora (switch/native/aurora/patches/0008) warms the priority-0 pipelines up first and the
# loading screen at boot waits for them (COS_PRECOMPILE=boot); unpatched Aurora ignores the table.
# --mark-priority DB only (re)writes that table in an existing file.
#
# --merge-from DB (repeatable) adds the rows of another Aurora pipeline cache, e.g. the console's
# own user/cache/pipeline_cache.db (scripts/switch/pull_pipeline_cache.sh fetches it) or the cache of
# any Mac run, as one more tier each: by default the tiers after the Mac runs' (one per file, in
# order), so they are priority 1 and the Switch builds them after the boot path and the stage sweep;
# --tier N right after a --merge-from picks its tier (below 4 makes its new rows priority 0). The
# file is read from a temporary copy (with its -journal, -wal and -shm files: sqlite rolls a hot
# journal back or replays a WAL on the copy, the original is never written). A row whose key, (type,
# hash), is already there keeps its earlier tier; only rows with the config version this build's
# Aurora writes for their type are taken (ClearPipelineConfigVersion, GXPipelineConfigVersion,
# RmlPipelineConfigVersion in build/native-mac/_deps/aurora-src; without that tree, the versions
# the bundled file already holds), and only rows whose config blob has its recorded size. The report
# counts, per file, the rows read, those of another config version or damaged (skipped), those
# already present and the new ones. Frames past 9,999,999 (a cache recorded over many sessions) are
# clamped so a row stays in its tier.
# --merge-only skips the Mac runs and merges the --merge-from files into the existing bundled file
# (<out>/initial_pipeline_cache.db; its tiers and rows are kept), default tiers after its highest.
#
# Output (default build/pipeline-cache/, gitignored): initial_pipeline_cache.db, report.txt and the
# runs. The file is derived from running the game with the player's own disc, so it is never
# committed or published: it holds no textures, geometry, text, audio or code, only Aurora's
# pipeline keys (per-material TEV stage/combiner selectors, vertex attribute formats, blend, depth
# and cull state as raw config structs), but they are recorded from the game's own materials.
# Copy it to the console with scripts/switch/push.sh --pipeline-cache (next to the NRO, where Aurora
# looks for it: sdmc:/switch/centollos/initial_pipeline_cache.db). On the Mac nothing reads
# it unless it is copied next to build/native-mac/centollos (Aurora's resources path).
set -u

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$script_dir/../.." && pwd)"
build="$repo/build/native-mac"
run="$script_dir/run.sh"

out="$repo/build/pipeline-cache"
do_build=1
sweep=1
prologue=1
sweep_jobs=4
sweep_frames=600
sweep_only=""
disc_args=()
mark_only=""
merge_only=0
merge_files=()  # --merge-from files, in order
merge_tiers=()  # their --tier ("" = the next free tier)
# Tiers below this are the boot path: priority 0 in pipeline_priority (see above).
priority_tiers=4
while [ $# -gt 0 ]; do
    case "$1" in
        --out) out="$2"; shift 2 ;;
        --no-build) do_build=0; shift ;;
        --no-sweep) sweep=0; shift ;;
        --no-prologue) prologue=0; shift ;;
        --sweep-jobs) sweep_jobs="$2"; shift 2 ;;
        --sweep-frames) sweep_frames="$2"; shift 2 ;;
        --sweep-only) sweep_only="$2"; shift 2 ;;
        --disc) disc_args=(--disc "$2"); shift 2 ;;
        --mark-priority) mark_only="$2"; shift 2 ;;
        --merge-from) merge_files+=("$2"); merge_tiers+=(""); shift 2 ;;
        --tier)
            [ ${#merge_files[@]} -gt 0 ] || { echo "gen_pipeline_cache: --tier must follow a --merge-from" >&2; exit 2; }
            case "$2" in ''|*[!0-9]*) echo "gen_pipeline_cache: --tier needs a number" >&2; exit 2 ;; esac
            merge_tiers[${#merge_tiers[@]}-1]="$2"; shift 2 ;;
        --merge-only) merge_only=1; shift ;;
        -h|--help) sed -n '2,/^set -u/p' "${BASH_SOURCE[0]}" | sed '$d' | sed 's/^# \{0,1\}//'; exit 2 ;;
        *) echo "gen_pipeline_cache: unknown option $1" >&2; exit 2 ;;
    esac
done
command -v sqlite3 > /dev/null || { echo "gen_pipeline_cache: sqlite3 not found" >&2; exit 2; }

# pipeline_priority: priority 0 for the rows first used on the boot path (tier < priority_tiers).
mark_priority() { # db
    sqlite3 "$1" "DROP TABLE IF EXISTS pipeline_priority;
CREATE TABLE pipeline_priority (
  type INTEGER NOT NULL,
  hash INTEGER NOT NULL,
  priority INTEGER NOT NULL,
  PRIMARY KEY (type, hash)
);
INSERT INTO pipeline_priority (type, hash, priority)
  SELECT type, hash, CASE WHEN first_frame_used < $priority_tiers * 10000000 THEN 0 ELSE 1 END
  FROM pipeline_cache;" || return 1
    echo "pipeline_priority: $(sqlite3 "$1" 'SELECT COUNT(*) FROM pipeline_priority WHERE priority = 0') of $(sqlite3 "$1" 'SELECT COUNT(*) FROM pipeline_priority') rows priority 0 (tiers 0-$((priority_tiers - 1)))"
}
if [ -n "$mark_only" ]; then
    [ -s "$mark_only" ] || { echo "gen_pipeline_cache: no file $mark_only" >&2; exit 2; }
    mark_priority "$mark_only" || exit 1
    sqlite3 "$mark_only" 'VACUUM;' || exit 1
    exit 0
fi

if [ "$merge_only" = 1 ] && [ ${#merge_files[@]} -eq 0 ]; then
    echo "gen_pipeline_cache: --merge-only needs at least one --merge-from DB" >&2; exit 2
fi
for f in ${merge_files[@]+"${merge_files[@]}"}; do
    [ -s "$f" ] || { echo "gen_pipeline_cache: no file $f (--merge-from)" >&2; exit 2; }
done

mkdir -p "$out"
out="$(cd "$out" && pwd)"
stamp="$(date +%Y%m%d-%H%M%S)"
db="$out/initial_pipeline_cache.db"
tmp="$out/.initial_pipeline_cache.db.tmp"
scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT

# The config version this build's Aurora writes for each type (0 clear, 1 GX, 2 RmlUi), as
# "(type, version), ..." for SQL; empty if the Aurora tree is not there.
aurora_versions() { # aurora source dir
    local dir="$1" t v name file list=""
    for t in "0 ClearPipelineConfigVersion lib/gfx/clear.hpp" "1 GXPipelineConfigVersion lib/gx/pipeline.hpp" \
             "2 RmlPipelineConfigVersion lib/rmlui/pipeline.hpp"; do
        read -r v name file <<< "$t"
        [ -f "$dir/$file" ] || continue
        local n
        n=$(sed -n "s/.*constexpr uint32_t $name = \([0-9][0-9]*\);.*/\1/p" "$dir/$file" | head -1)
        [ -n "$n" ] && list="${list:+$list, }($v, $n)"
    done
    echo "$list"
}
versions="$(aurora_versions "$build/_deps/aurora-src")"
versions_from="$build/_deps/aurora-src"
switch_versions="$(aurora_versions "$repo/build/switch-native/aurora-switch")"
if [ -n "$versions" ] && [ -n "$switch_versions" ] && [ "$versions" != "$switch_versions" ]; then
    echo "gen_pipeline_cache: warning: the Mac build's Aurora writes config versions $versions, the Switch build's $switch_versions; rebuild both" >&2
fi

if [ "$merge_only" = 1 ]; then
    [ -s "$db" ] || { echo "gen_pipeline_cache: --merge-only: no $db to merge into" >&2; exit 2; }
    rm -f "$tmp"
    cp "$db" "$tmp" || exit 1
    if [ -z "$versions" ]; then
        versions="$(sqlite3 "$tmp" "SELECT group_concat('(' || type || ', ' || v || ')', ', ') FROM
  (SELECT type, MAX(config_version) AS v FROM pipeline_cache GROUP BY type)")"
        versions_from="$db"
    fi
    report="$out/report-merge-$stamp.txt"
    {
        echo "gen_pipeline_cache --merge-only $stamp ($(git -C "$repo" rev-parse --short HEAD 2>/dev/null || echo '?')): into $db"
        echo "before: $(sqlite3 "$tmp" 'SELECT COUNT(*) FROM pipeline_cache') rows"
        printf '%-4s %-16s %-6s %-8s %-8s %s\n' tier run exit runs rows new
    } > "$report"
    next_tier=$(( $(sqlite3 "$tmp" 'SELECT COALESCE(MAX(first_frame_used) / 10000000, -1) FROM pipeline_cache') + 1 ))
else
if [ "$do_build" = 1 ]; then
    ninja -C "$build" centollos > /dev/null || { echo "gen_pipeline_cache: build failed" >&2; exit 2; }
fi

runs="$out/runs-$stamp"
mkdir -p "$runs"
export COS_CACHE_PER_RUN=1

# tier name target options...
tiers=(
    "0 file-select file-select --uncapped --input native/check/input/file-select.txt"
    "1 new-game new-game --uncapped --input native/check/input/new-game.txt"
)
# The prologue runs capped: uncapped, the PLAY scene waits ~190 s for the prologue's streamed BGM
# and the input script's timing no longer matches (docs/NATIVE_PORT_PLAN.md, M14).
[ "$prologue" = 1 ] && tiers+=("2 outset-real outset-real --timeout 480 --input native/check/input/new-game.txt")
tiers+=("3 outset-control outset-control --uncapped --stage sea:44:206 --input native/check/input/outset-control.txt")

pids=()
for line in "${tiers[@]}"; do
    read -r tier name target opts <<< "$line"
    # shellcheck disable=SC2086  # opts is a word list
    (cd "$repo" && "$run" "$target" $opts ${disc_args[@]+"${disc_args[@]}"} --quiet \
        --run-dir "$runs/$tier-$name" > "$runs/$tier-$name.out" 2>&1
     echo $? > "$runs/$tier-$name.rc") &
    pids+=($!)
    echo "gen_pipeline_cache: tier $tier $name started"
done
for pid in "${pids[@]}"; do wait "$pid"; done

if [ "$sweep" = 1 ]; then
    echo "gen_pipeline_cache: tier 4 boot-sweep started ($sweep_frames frames per stage, $sweep_jobs at a time)"
    sweep_args=(--jobs "$sweep_jobs" --frames "$sweep_frames" --out "$runs/4-boot-sweep")
    [ -n "$sweep_only" ] && sweep_args+=(--only "$sweep_only")
    (cd "$repo" && "$run" boot-sweep "${sweep_args[@]}" ${disc_args[@]+"${disc_args[@]}"} \
        > "$runs/4-boot-sweep.out" 2>&1
     echo $? > "$runs/4-boot-sweep.rc")
fi

# --- merge ------------------------------------------------------------------------------------
# Aurora's schema (lib/gfx/pipeline_cache.cpp, PipelineCacheSchema 1); the seed reader needs
# aurora_schema = 1 and the pipeline_cache columns.
rm -f "$tmp"
sqlite3 "$tmp" <<'SQL' || exit 1
CREATE TABLE aurora_schema(value INTEGER);
INSERT INTO aurora_schema VALUES (1);
CREATE TABLE pipeline_cache (
  type INTEGER NOT NULL,
  hash INTEGER NOT NULL,
  config_version INTEGER NOT NULL,
  config_size INTEGER NOT NULL,
  config BLOB NOT NULL,
  first_frame_used INTEGER NOT NULL,
  PRIMARY KEY (type, hash)
);
CREATE INDEX pipeline_cache_load_order_idx ON pipeline_cache(type, config_version, first_frame_used);
SQL

report="$out/report.txt"
{
    echo "gen_pipeline_cache $stamp ($(git -C "$repo" rev-parse --short HEAD 2>/dev/null || echo '?'))"
    printf '%-4s %-16s %-6s %-8s %-8s %s\n' tier run exit runs rows new
} > "$report"
merge_tier() { # tier name rc dbs...
    local tier="$1" name="$2" rc="$3"; shift 3
    local before after rows=0 n=0 f
    before=$(sqlite3 "$tmp" 'SELECT COUNT(*) FROM pipeline_cache')
    for f in "$@"; do
        [ -s "$f" ] || continue
        n=$((n + 1))
        # The source is opened read-write so that a WAL left by a run that did not close it is read.
        rows=$((rows + $(sqlite3 "$f" 'SELECT COUNT(*) FROM pipeline_cache' 2>/dev/null || echo 0)))
        sqlite3 "$tmp" "ATTACH '$f' AS src;
INSERT INTO pipeline_cache (type, hash, config_version, config_size, config, first_frame_used)
  SELECT type, hash, config_version, config_size, config, first_frame_used + $tier * 10000000
  FROM src.pipeline_cache WHERE true
  ON CONFLICT(type, hash) DO UPDATE SET
    first_frame_used = MIN(pipeline_cache.first_frame_used, excluded.first_frame_used);" || exit 1
    done
    after=$(sqlite3 "$tmp" 'SELECT COUNT(*) FROM pipeline_cache')
    printf '%-4s %-16s %-6s %-8s %-8s %s\n' "$tier" "$name" "$rc" "$n" "$rows" $((after - before)) >> "$report"
}
merged=("${tiers[@]}")
[ "$sweep" = 1 ] && merged+=("4 boot-sweep boot-sweep")
max_tier=-1
for line in "${merged[@]}"; do
    read -r tier name _ <<< "$line"
    [ "$tier" -gt "$max_tier" ] && max_tier=$tier
    rc="$(cat "$runs/$tier-$name.rc" 2>/dev/null || echo '?')"
    if [ "$name" = boot-sweep ]; then
        merge_tier "$tier" "$name" "$rc" "$runs/$tier-$name"/*/cache/pipeline_cache.db
    else
        merge_tier "$tier" "$name" "$rc" "$runs/$tier-$name/cache/pipeline_cache.db"
    fi
done
next_tier=$((max_tier + 1))
[ -z "$versions" ] && versions="$(sqlite3 "$tmp" "SELECT group_concat('(' || type || ', ' || v || ')', ', ') FROM
  (SELECT type, MAX(config_version) AS v FROM pipeline_cache GROUP BY type)")" && versions_from="the Mac runs"
fi

# --- --merge-from -------------------------------------------------------------------------------
# merge_file tier db: one more tier from another pipeline cache (see --merge-from above).
merge_file() {
    local tier="$1" src="$2" copy ext schema rows wrong bad dup before after
    copy="$scratch/src-$tier.db"
    rm -f "$copy" "$copy"-journal "$copy"-wal "$copy"-shm
    cp "$src" "$copy" || return 1
    for ext in -journal -wal -shm; do
        [ -e "$src$ext" ] && { cp "$src$ext" "$copy$ext" || return 1; }
    done
    # Opened read-write: a hot journal is rolled back, a WAL replayed (on the copy).
    if ! sqlite3 "$copy" 'PRAGMA quick_check;' > "$scratch/check.txt" 2>&1 || [ "$(head -1 "$scratch/check.txt")" != ok ]; then
        echo "gen_pipeline_cache: $src is damaged ($(head -1 "$scratch/check.txt")); skipped" >&2
        printf '%-4s %-16s %s\n' "$tier" "merge" "damaged, skipped: $src" >> "$report"
        return 0
    fi
    schema=$(sqlite3 "$copy" 'SELECT value FROM aurora_schema' 2>/dev/null | head -1)
    if [ "$schema" != 1 ] || ! sqlite3 "$copy" 'SELECT type, hash, config_version, config_size, config, first_frame_used FROM pipeline_cache LIMIT 0' > /dev/null 2>&1; then
        echo "gen_pipeline_cache: $src is not an Aurora pipeline cache of schema 1 (aurora_schema ${schema:-missing}); skipped" >&2
        printf '%-4s %-16s %s\n' "$tier" "merge" "not schema 1, skipped: $src" >> "$report"
        return 0
    fi
    rows=$(sqlite3 "$copy" 'SELECT COUNT(*) FROM pipeline_cache')
    wrong=$(sqlite3 "$copy" "SELECT COUNT(*) FROM pipeline_cache WHERE (type, config_version) NOT IN (VALUES $versions)")
    bad=$(sqlite3 "$copy" "SELECT COUNT(*) FROM pipeline_cache WHERE (type, config_version) IN (VALUES $versions)
  AND (length(config) != config_size OR typeof(config) != 'blob')")
    before=$(sqlite3 "$tmp" 'SELECT COUNT(*) FROM pipeline_cache')
    sqlite3 "$tmp" "ATTACH '$copy' AS src;
SELECT COUNT(*) FROM src.pipeline_cache s JOIN pipeline_cache d USING (type, hash)
  WHERE (s.type, s.config_version) IN (VALUES $versions) AND length(s.config) = s.config_size;
INSERT INTO pipeline_cache (type, hash, config_version, config_size, config, first_frame_used)
  SELECT type, hash, config_version, config_size, config,
         MIN(MAX(first_frame_used, 0), 9999999) + $tier * 10000000
  FROM src.pipeline_cache
  WHERE (type, config_version) IN (VALUES $versions) AND length(config) = config_size
    AND typeof(config) = 'blob'
  ON CONFLICT(type, hash) DO UPDATE SET
    first_frame_used = MIN(pipeline_cache.first_frame_used, excluded.first_frame_used);" > "$scratch/dup.txt" || return 1
    dup=$(cat "$scratch/dup.txt")
    after=$(sqlite3 "$tmp" 'SELECT COUNT(*) FROM pipeline_cache')
    printf '%-4s %-16s %-6s %-8s %-8s %s\n' "$tier" "merge" "-" 1 "$rows" $((after - before)) >> "$report"
    echo "     $src: $rows rows, $wrong of another config version (skipped)$( [ "$bad" -gt 0 ] && echo ", $bad damaged (skipped)"), $dup already present, $((after - before)) new" >> "$report"
    if [ "$wrong" -gt 0 ]; then
        sqlite3 "$copy" "SELECT '       skipped: type ' || type || ' config version ' || config_version || ': ' || COUNT(*) || ' rows'
  FROM pipeline_cache WHERE (type, config_version) NOT IN (VALUES $versions) GROUP BY type, config_version" >> "$report"
    fi
}
if [ ${#merge_files[@]} -gt 0 ]; then
    echo "--merge-from (config versions (type, version): $versions, from $versions_from):" >> "$report"
    i=0
    while [ $i -lt ${#merge_files[@]} ]; do
        t="${merge_tiers[$i]}"
        if [ -z "$t" ]; then
            t=$next_tier
        fi
        [ "$t" -ge "$next_tier" ] && next_tier=$((t + 1))
        merge_file "$t" "${merge_files[$i]}" || { echo "gen_pipeline_cache: merging ${merge_files[$i]} failed" >&2; exit 1; }
        i=$((i + 1))
    done
fi

priority_note="$(mark_priority "$tmp")" || exit 1
sqlite3 "$tmp" 'VACUUM;' || exit 1
mv -f "$tmp" "$db"
{
    echo "$priority_note"
    echo "rows by tier:"
    sqlite3 "$db" 'SELECT first_frame_used / 10000000, COUNT(*) FROM pipeline_cache GROUP BY 1;'
    echo "rows by type (0 clear, 1 GX, 2 RmlUi) and config version:"
    sqlite3 "$db" 'SELECT type, config_version, COUNT(*), config_size FROM pipeline_cache GROUP BY 1, 2, 4;'
    echo "total $(sqlite3 "$db" 'SELECT COUNT(*) FROM pipeline_cache') rows, $(wc -c < "$db" | tr -d ' ') bytes: $db"
} >> "$report"
cat "$report"
