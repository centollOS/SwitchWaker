#!/usr/bin/env bash
# Set up the wwrando oracle (Mac only, dev time): a Python venv with wwrando's dependencies and a
# gclib checkout, both under build/ of the checkout this script lives in (build/ is ignored by git).
#
#   native/tools/rando/setup_oracle.sh [--force] [--check]
#
#   --force   delete build/rando-venv and build/rando-deps first and set them up again
#   --check   only report whether the oracle is ready (exit 0 ready, 1 not ready); changes nothing
#
# Creates:
#   build/rando-venv          Python >= 3.11 venv with the pinned packages below
#   build/rando-deps/gclib    LagoLunatic/gclib at GCLIB_COMMIT (ref/wwrando/gclib is an empty
#                             submodule in our read-only checkout)
#
# Needs ref/wwrando (wwrando 1.10.0 at 9775811, read-only, MIT). The oracle tools in this folder
# (ww_export.py, ww_verify.py, ww_trace.py, make_test_placement.py) find the venv by themselves; see
# native/tools/rando/README.md.
#
# Environment:
#   PYTHON           interpreter used to create the venv (default: python3.12, then python3)
#   GCLIB_SRC        where to clone gclib from (default: the GitHub URL; a local clone works offline,
#                    e.g. build/rando-proto/deps/gclib)
#   PIP_OPTS         extra pip install options (e.g. --no-index --find-links <dir> for offline use)
set -euo pipefail

GCLIB_URL="https://github.com/LagoLunatic/gclib.git"
GCLIB_COMMIT="d9322523a7e2175233def3af0a8c7e0a2bf55f42"   # wwrando 9775811's gclib submodule
WWRANDO_COMMIT="9775811"
# The set that ran the oracle prototypes (CPython 3.12.8). wwrando's own requirements.txt also
# pins Qt (only needed by its UI) and certifi (only for its update check); neither is installed.
PACKAGES=(
  "ruamel.yaml==0.19.1"
  "numpy==2.5.4"
  "pillow==13.0.0"
  "imagequant==1.1.5"
  "cffi==2.1.1"
  "pycparser==3.11"
  "appdirs==1.4.4"
  "tqdm==4.70.1"
)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
VENV="$ROOT/build/rando-venv"
DEPS="$ROOT/build/rando-deps"
WWRANDO="$ROOT/ref/wwrando"
STAMP="$VENV/.oracle-stamp"
STAMP_TEXT="$(printf '%s\n' "${PACKAGES[@]}")"

FORCE=0
CHECK=0
for arg in "$@"; do
  case "$arg" in
    --force) FORCE=1 ;;
    --check) CHECK=1 ;;
    -h|--help) sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "setup_oracle: unknown argument '$arg' (see --help)" >&2; exit 2 ;;
  esac
done

say() { echo "setup_oracle: $*"; }
die() { echo "setup_oracle: error: $*" >&2; exit 1; }

venv_ok() {
  [[ -x "$VENV/bin/python" && -f "$STAMP" && "$(cat "$STAMP")" == "$STAMP_TEXT" ]] || return 1
  "$VENV/bin/python" -I -c "import ruamel.yaml, numpy, PIL, imagequant, appdirs, tqdm" 2>/dev/null
}

gclib_ok() {
  [[ -f "$DEPS/gclib/gclib/gcm.py" ]] || return 1
  [[ "$(git -C "$DEPS/gclib" rev-parse HEAD 2>/dev/null)" == "$GCLIB_COMMIT" ]]
}

wwrando_ok() {
  [[ -f "$WWRANDO/randomizer.py" && -f "$WWRANDO/logic/item_locations.txt" ]]
}

smoke() {
  # Import wwrando the way the tools do (python -I ignores PYTHONPATH, so set sys.path directly).
  "$VENV/bin/python" -I - "$WWRANDO" "$DEPS/gclib" <<'EOF'
import sys
sys.dont_write_bytecode = True
sys.path[:0] = [sys.argv[1], sys.argv[2]]
import contextlib
with contextlib.redirect_stdout(sys.stderr):
  import randomizer, version
print(version.VERSION)
EOF
}

if [[ $CHECK -eq 1 ]]; then
  ok=1
  wwrando_ok && say "ref/wwrando: ok" || { say "ref/wwrando: missing ($WWRANDO)"; ok=0; }
  venv_ok && say "venv: ok ($VENV)" || { say "venv: missing or outdated ($VENV)"; ok=0; }
  gclib_ok && say "gclib: ok ($DEPS/gclib)" || { say "gclib: missing or wrong commit ($DEPS/gclib)"; ok=0; }
  if [[ $ok -eq 1 ]]; then
    v="$(smoke 2>/dev/null)" && say "wwrando imports: ok (VERSION $v)" || { say "wwrando import failed"; ok=0; }
  fi
  [[ $ok -eq 1 ]] && exit 0 || exit 1
fi

wwrando_ok || die "ref/wwrando not found at $WWRANDO (clone https://github.com/LagoLunatic/wwrando at $WWRANDO_COMMIT there)"
head="$(git -C "$WWRANDO" rev-parse --short=7 HEAD 2>/dev/null || echo unknown)"
[[ "$head" == "$WWRANDO_COMMIT" ]] || say "warning: ref/wwrando is at $head, the plan targets $WWRANDO_COMMIT"

if [[ $FORCE -eq 1 ]]; then
  say "removing $VENV and $DEPS"
  rm -rf "$VENV" "$DEPS"
fi

# 1. The venv.
if venv_ok; then
  say "venv up to date: $VENV"
else
  py="${PYTHON:-}"
  if [[ -z "$py" ]]; then
    for cand in python3.12 python3.13 python3; do
      if command -v "$cand" >/dev/null 2>&1; then py="$cand"; break; fi
    done
  fi
  [[ -n "$py" ]] || die "no python3 found (set PYTHON)"
  "$py" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 11) else 1)' \
    || die "$py is $("$py" -V 2>&1); wwrando needs Python >= 3.11 (StrEnum); set PYTHON"
  say "creating $VENV with $("$py" -V 2>&1)"
  rm -rf "$VENV"
  mkdir -p "$ROOT/build"
  "$py" -m venv "$VENV"
  # shellcheck disable=SC2086
  "$VENV/bin/python" -m pip install --disable-pip-version-check -q ${PIP_OPTS:-} "${PACKAGES[@]}" \
    || die "pip install failed (offline? use PIP_OPTS='--no-index --find-links <wheel dir>')"
  printf '%s' "$STAMP_TEXT" > "$STAMP"
  venv_ok || die "venv created but the packages do not import"
fi

# 2. gclib.
if gclib_ok; then
  say "gclib up to date: $DEPS/gclib"
else
  src="${GCLIB_SRC:-$GCLIB_URL}"
  say "cloning gclib from $src"
  rm -rf "$DEPS/gclib"
  mkdir -p "$DEPS"
  git clone -q "$src" "$DEPS/gclib" || die "git clone $src failed (offline? set GCLIB_SRC to a local clone)"
  git -C "$DEPS/gclib" -c advice.detachedHead=false checkout -q "$GCLIB_COMMIT" \
    || die "gclib commit $GCLIB_COMMIT not found in $src"
  gclib_ok || die "gclib checkout incomplete"
fi

# 3. Smoke: wwrando imports with these paths.
v="$(smoke)" || die "wwrando does not import with the venv"
say "ready: wwrando VERSION $v"
[[ "$v" == "1.10.0_$WWRANDO_COMMIT" ]] || say "warning: expected VERSION 1.10.0_$WWRANDO_COMMIT (the permalink embeds it)"
