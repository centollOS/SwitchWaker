#!/usr/bin/env bash
# Record the pipelines the game uses on the Mac and merge them into Aurora's bundled pipeline cache,
# initial_pipeline_cache.db, for the Switch build to precompile at boot (docs/SWITCH_BUILD.md,
# "Pipeline precompile").
#
#   native/tools/gen_pipeline_cache.sh [--out DIR] [--no-build] [--no-sweep] [--no-prologue]
#                                      [--sweep-jobs N] [--sweep-frames N] [--sweep-only LIST]
#                                      [--disc PATH]
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

if [ "$do_build" = 1 ]; then
    ninja -C "$build" centollos > /dev/null || { echo "gen_pipeline_cache: build failed" >&2; exit 2; }
fi

mkdir -p "$out"
out="$(cd "$out" && pwd)"
stamp="$(date +%Y%m%d-%H%M%S)"
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
db="$out/initial_pipeline_cache.db"
tmp="$out/.initial_pipeline_cache.db.tmp"
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
for line in "${merged[@]}"; do
    read -r tier name _ <<< "$line"
    rc="$(cat "$runs/$tier-$name.rc" 2>/dev/null || echo '?')"
    if [ "$name" = boot-sweep ]; then
        merge_tier "$tier" "$name" "$rc" "$runs/$tier-$name"/*/cache/pipeline_cache.db
    else
        merge_tier "$tier" "$name" "$rc" "$runs/$tier-$name/cache/pipeline_cache.db"
    fi
done
priority_note="$(mark_priority "$tmp")" || exit 1
sqlite3 "$tmp" 'VACUUM;' || exit 1
mv -f "$tmp" "$db"
{
    echo "$priority_note"
    echo "rows by type (0 clear, 1 GX, 2 RmlUi) and config version:"
    sqlite3 "$db" 'SELECT type, config_version, COUNT(*), config_size FROM pipeline_cache GROUP BY 1, 2, 4;'
    echo "total $(sqlite3 "$db" 'SELECT COUNT(*) FROM pipeline_cache') rows, $(wc -c < "$db" | tr -d ' ') bytes: $db"
} >> "$report"
cat "$report"
