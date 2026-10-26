#!/usr/bin/env bash
# run_batches.sh <listfile> <out.tsv>: run seed-batch lines one after the other (step R9.3).
#
# Each list line is "<name><TAB><tool> <args...>" (blank lines and lines starting with # skipped).
# <tool> is a script in native/tools/rando (check_seeds.py, ww_verify_hints.py); <args> are shell
# words (quote values with spaces, e.g. sword_mode="No Starting Sword"). Names already in <out.tsv>
# are skipped, so an interrupted run resumes where it stopped.
#
# One TSV row per batch: name  result  summary  wall_s  log
#   result is pass (exit 0), FAIL (exit 1) or ERROR (other exit); summary is the tool's last
#   summary line. Full output: build/rando-batches/logs/<name>.log.
#
# Environment: BATCH_JOBS (wwrando worker processes, default 3), RANDO_CLI (default
# build/native-mac/cos_rando_cli), PY (default build/rando-venv/bin/python).
# Exit 0 when every row of <out.tsv> is pass, 1 otherwise.
set -uo pipefail
[[ $# -eq 2 ]] || { sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"
TOOLS="$ROOT/native/tools/rando"
list="$1"; out="$2"
PY="${PY:-$ROOT/build/rando-venv/bin/python}"
CLI="${RANDO_CLI:-$ROOT/build/native-mac/cos_rando_cli}"
JOBS="${BATCH_JOBS:-3}"
LOGS="$ROOT/build/rando-batches/logs"
mkdir -p "$LOGS" "$(dirname "$out")"
touch "$out"

while IFS=$'\t' read -r name cmd || [[ -n "$name" ]]; do
  [[ -z "$name" || "$name" == \#* ]] && continue
  if cut -f1 "$out" | grep -qxF "$name"; then
    echo "skip $name (already in $out)"; continue
  fi
  eval "set -- $cmd"
  tool="$1"; shift
  log="$LOGS/$name.log"
  echo "run  $name: $tool $*"
  t0=$(date +%s)
  "$PY" -I "$TOOLS/$tool" --cli "$CLI" --jobs "$JOBS" "$@" > "$log" 2>&1 < /dev/null
  rc=$?
  wall=$(( $(date +%s) - t0 ))
  case $rc in 0) res=pass ;; 1) res=FAIL ;; *) res=ERROR ;; esac
  summary=$(grep -E '^(check_seeds|ww_verify_hints): ' "$log" | tail -1 | tr '\t' ' ')
  [[ -n "$summary" ]] || summary=$(tail -1 "$log" | tr '\t' ' ')
  printf '%s\t%s\t%s\t%s\t%s\n' "$name" "$res" "$summary" "$wall" "${log#$ROOT/}" >> "$out"
  echo "     $res in ${wall}s: $summary"
done < "$list"

! cut -f2 "$out" | grep -qv '^pass$'
