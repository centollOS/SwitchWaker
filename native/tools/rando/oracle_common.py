"""Shared helpers of the wwrando oracle tools (ww_export.py, ww_verify.py, ww_trace.py,
make_test_placement.py). See README.md in this folder.

Each tool starts with:

    import os, sys
    sys.dont_write_bytecode = True
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import oracle_common as oc
    oc.bootstrap()

bootstrap() puts ref/wwrando and build/rando-deps/gclib on sys.path (python -I ignores PYTHONPATH)
and, when the running interpreter lacks wwrando's dependencies, re-executes the tool with the venv
made by setup_oracle.sh (build/rando-venv), so `python3 -I native/tools/rando/<tool>.py ...` works.
"""
import contextlib
import dataclasses
import json
import os
import sys
import tempfile

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(TOOLS_DIR, "..", "..", ".."))
WWRANDO_DIR = os.environ.get("COS_WWRANDO_DIR") or os.path.join(REPO_ROOT, "ref", "wwrando")
GCLIB_DIR = os.environ.get("COS_GCLIB_DIR") or os.path.join(REPO_ROOT, "build", "rando-deps", "gclib")
VENV_DIR = os.environ.get("COS_RANDO_VENV") or os.path.join(REPO_ROOT, "build", "rando-venv")

# wwrando's VERSION at the reference commit; permalinks embed it (decision D1).
EXPECTED_VERSION = "1.10.0_9775811"
PLACEMENT_FORMAT = 1

DEPENDENCIES = ("ruamel.yaml", "numpy", "PIL", "appdirs", "tqdm")


def die(msg, code=2):
  print(f"{os.path.basename(sys.argv[0])}: error: {msg}", file=sys.stderr)
  sys.exit(code)


def _have_dependencies():
  try:
    for mod in DEPENDENCIES:
      __import__(mod)
  except ImportError:
    return False
  return True


def bootstrap():
  """Make wwrando importable, re-executing in the oracle venv if needed. Exits 2 when not set up."""
  if not _have_dependencies():
    venv_python = os.path.join(VENV_DIR, "bin", "python")
    in_venv = os.path.realpath(sys.prefix) == os.path.realpath(VENV_DIR)
    if os.path.exists(venv_python) and not in_venv and not os.environ.get("COS_ORACLE_REEXEC"):
      os.environ["COS_ORACLE_REEXEC"] = "1"
      script = os.path.abspath(sys.argv[0])
      os.execv(venv_python, [venv_python, "-I", script] + sys.argv[1:])
    die("wwrando's Python dependencies are missing; run native/tools/rando/setup_oracle.sh")
  if not os.path.isfile(os.path.join(WWRANDO_DIR, "randomizer.py")):
    die(f"wwrando not found at {WWRANDO_DIR} (set COS_WWRANDO_DIR)")
  if not os.path.isfile(os.path.join(GCLIB_DIR, "gclib", "gcm.py")):
    die(f"gclib not found at {GCLIB_DIR}; run native/tools/rando/setup_oracle.sh")
  for path in (GCLIB_DIR, WWRANDO_DIR):
    if path not in sys.path:
      sys.path.insert(0, path)
  with contextlib.redirect_stdout(sys.stderr):
    import version  # wwrando's version.py
  if version.VERSION != EXPECTED_VERSION and not os.environ.get("COS_WWRANDO_ANY_VERSION"):
    die(f"wwrando VERSION is {version.VERSION}, expected {EXPECTED_VERSION} "
        "(set COS_WWRANDO_ANY_VERSION=1 to use it anyway)")


# ---------------------------------------------------------------------------------------------
# Options

def parse_opt_pairs(pairs):
  """['name=<json>', ...] -> dict. A value that is not valid JSON is taken as a string, so
  sword_mode=Swordless works as well as sword_mode='"Swordless"'."""
  out = {}
  for kv in pairs:
    if "=" not in kv:
      die(f"option '{kv}' is not name=value")
    k, v = kv.split("=", 1)
    try:
      out[k] = json.loads(v)
    except json.JSONDecodeError:
      out[k] = v
  return out


def make_options(values=None, strict=True):
  """A wwrando Options with `values` applied and converted to the declared types (StrEnum options
  given as strings become enums). Unknown names are an error when strict, else ignored."""
  from options.wwrando_options import Options
  opts = Options()
  by_name = Options.by_name()
  for k, v in (values or {}).items():
    if k not in by_name:
      if strict:
        die(f"unknown wwrando option '{k}'")
      continue
    opts[k] = v
  try:
    opts.validate()
  except (ValueError, TypeError) as e:
    die(f"invalid option value: {e}")
  return opts


def options_to_json(opts):
  from options.wwrando_options import Options
  return {f.name: _plain(getattr(opts, f.name)) for f in dataclasses.fields(Options)}


def _plain(v):
  # StrEnum -> its str value; lists/dicts recursively.
  if isinstance(v, str):
    return str(v)
  if isinstance(v, (list, tuple)):
    return [_plain(x) for x in v]
  if isinstance(v, dict):
    return {str(k): _plain(x) for k, x in v.items()}
  return v


def load_options_file(path):
  with open(path) as f:
    d = json.load(f)
  # Accept a whole placement JSON as well as a bare options dict.
  return d.get("options", d) if isinstance(d, dict) and "format" in d else d


# ---------------------------------------------------------------------------------------------
# wwrando instances

def set_seed_key(key):
  """Override wwrando's SEED_KEY (empty in a source checkout) for do_not_generate_spoiler_log seeds."""
  import randomizer
  randomizer.SEED_KEY = key


def make_randomizer(seed, opts, logs_dir=None):
  """A dry-run WWRandomizer (no ISO). wwrando's prints go to stderr so stdout stays clean."""
  from wwrando import make_argparser
  from randomizer import WWRandomizer
  argv = ["--dry"] + ([] if logs_dir else ["--nologs"])
  args = make_argparser().parse_args(args=argv)
  out_dir = logs_dir or tempfile.mkdtemp(prefix="ww_oracle_")
  if logs_dir:
    os.makedirs(logs_dir, exist_ok=True)
  with contextlib.redirect_stdout(sys.stderr):
    return WWRandomizer(seed=seed, clean_iso_path=None, randomized_output_folder=out_dir,
                        options=opts, cmd_line_args=args)


def randomize(r):
  with contextlib.redirect_stdout(sys.stderr):
    r.randomize_all()


def decode_permalink(permalink):
  from randomizer import WWRandomizer
  with contextlib.redirect_stdout(sys.stderr):
    seed, opts = WWRandomizer.decode_permalink(permalink)
  return seed, opts


def placement_from_randomizer(r, opts):
  """The golden placement JSON, schema format 1 (docs/RANDOMIZER_PLAN.md "The oracle")."""
  return {
    "format": PLACEMENT_FORMAT,
    "seed": r.seed,
    "permalink": r.permalink,
    "options": options_to_json(opts),
    "extra_starting_items": list(r.extra_start_items.random_starting_items),
    "charts": {str(k): v for k, v in r.charts.island_number_to_chart_name.items()},
    "required_boss_locations": list(r.boss_reqs.required_boss_item_locations),
    "entrances": dict(r.entrances.entrance_connections),
    "starting_island_room": r.starting_island.room_number,
    "locations": dict(r.logic.done_item_locations),
  }


def write_json(d, path):
  text = json.dumps(d, indent=1, default=str) + "\n"
  if path == "-":
    sys.stdout.write(text)
  else:
    with open(path, "w", newline="\n") as f:
      f.write(text)


def read_json(path):
  try:
    with open(path) as f:
      return json.load(f)
  except (OSError, json.JSONDecodeError) as e:
    die(f"cannot read {path}: {e}")
