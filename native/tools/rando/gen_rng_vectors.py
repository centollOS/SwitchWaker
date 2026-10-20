#!/usr/bin/env python3
"""Write the RNG/MD5 test vectors of docs/RANDOMIZER_PLAN.md step R0.2 (cos_rando_test_R0_2).

    python3 -I native/tools/rando/gen_rng_vectors.py [--out native/check/rando/fixtures/R0_2_rng_vectors.txt]
    python3 -I native/tools/rando/gen_rng_vectors.py --check     # exit 1 when the checked-in file differs

The values come from CPython's random.Random and hashlib.md5, through wwrando's own code
(WWRandomizer.sanitize_seed, convert_string_to_integer_md5, get_new_rng and RNG_CHANGING_OPTIONS), so
the C++ port (native/src/pc/features/rando/pc_rando_rng.cpp) is checked against what wwrando
1.10.0_9775811 really does. Runs in the oracle venv (setup_oracle.sh; oracle_common re-executes there).

Format, one record per line, tokens separated by single spaces:

    # comment
    md5 <hex of the input bytes, or - for empty> <md5 hexdigest>
    case <name>                       starts a case
    seed <sanitized seed> <key or -> <md5 hexdigest of integer_seed> <11 option values>
                                      the case's RNG is WWRandomizer.get_new_rng() for this seed
    int <decimal n>                   the case's RNG is Random(n) (Python seeds with abs(n))
    state <first 8 words of the MT state after seeding+warm-up> <index>
    <op> <args> : <results>           ops, each block of one op starting from a fresh copy of the case's RNG

Ops (each block draws 1000 outputs for the plan's five seeds, 100 for the other cases):
    u32 : v*1000                      getrandbits(32)
    bits <k*1000> : <hex*1000>        getrandbits(k), k in 0..96
    below <n*1000> : r*1000           _randbelow(n)
    random : int(x * 2**53)*1000      random()
    randint <a b>*1000 : r*1000
    randrange <start stop>*1000 : r*1000
    choice <len*1000> : idx*1000      choice(range(len))
    shuffle <n> : <perm>              one call (lines until 1000 outputs)
    sample <n> <k> : <selected>       sample(range(n), k), one call
    weighted <c1..cm> : <idx>         sample(range(m), 1, counts=c)[0] (WWRandomizer.weighted_choice)
    choices <w1..wm> : <idx>          choices(range(m), weights=w)[0] (hints.py)
"""
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

import argparse  # noqa: E402
import contextlib  # noqa: E402
import hashlib  # noqa: E402
import platform  # noqa: E402
import random  # noqa: E402

DEFAULT_OUT = os.path.join(oc.REPO_ROOT, "native", "check", "rando", "fixtures", "R0_2_rng_vectors.txt")
N = 1000      # outputs per op for the plan's seeds (TestSeed1, pytestseed, abc, 3304751, short key)
N_SMALL = 100  # for the other cases, which mostly check seeding
FULL_CASES = {"TestSeed1", "pytestseed", "abc", "3304751", "int96"}
TEST_SEED_KEY = "SwitchWakerR02TestKey"

# (name, seed string, option overrides, seed key). The default preset uses wwrando's defaults.
SEED_CASES = [
  ("TestSeed1", "TestSeed1", {}, None),
  ("pytestseed", "pytestseed", {}, None),
  ("abc", "abc", {}, None),
  ("3304751", "3304751", {}, None),
  # Sanitizing (spaces, a '/', a non-ASCII letter dropped; truncated to 42) and other hint options.
  ("sanitize", "  Wind Waker/Seed é-'%._x" + "0123456789" * 5, {
    "fishmen_hints": False, "num_path_hints": 5, "num_barren_hints": 0, "num_item_hints": 7,
    "cryptic_hints": False, "hint_importance": True}, None),
  # do_not_generate_spoiler_log: the key joins the md5 input and the last option is 1.
  ("nospoiler", "TestSeed1", {"do_not_generate_spoiler_log": True}, TEST_SEED_KEY),
]
# Short keys: integers whose bit_length gives fewer than 4 words (an md5 with zero top words seeds
# the same way), plus 0, a negative and a 2-word/3-word value.
INT_CASES = [
  ("int0", 0),
  ("int1", 1),
  ("intneg", -3304751),
  ("int32", 0xFFFFFFFF),
  ("int33", 1 << 32),
  ("int64", (1 << 64) - 1),
  ("int96", 0x00000000_DEADBEEF_00000001_12345678),
]


def md5_lines():
  out = []
  pattern = bytes(range(256))
  lengths = list(range(0, 130)) + [255, 256, 1000]
  for n in lengths:
    data = (pattern * 4)[:n]
    out.append(f"md5 {data.hex() or '-'} {hashlib.md5(data).hexdigest()}")
  return out


def state_line(rng):
  st = rng.getstate()[1]
  return "state " + " ".join(str(x) for x in st[:8]) + f" {st[624]}"


def op_blocks(base, N):
  """The op lines for one case. `base` is the case's RNG after seeding and warm-up; N outputs per op."""
  lines = []
  helper = random.Random(12345)  # picks arguments only; never part of what is checked

  def fresh():
    r = random.Random()
    r.setstate(base.getstate())
    return r

  r = fresh()
  lines.append("u32 : " + " ".join(str(r.getrandbits(32)) for _ in range(N)))

  r = fresh()
  ks = [(i * 7) % 97 for i in range(N)]
  lines.append("bits " + " ".join(map(str, ks)) + " : " + " ".join(format(r.getrandbits(k), "x") for k in ks))

  r = fresh()
  ns = [helper.randrange(1, 1 << helper.randrange(1, 64)) for _ in range(N)]
  lines.append("below " + " ".join(map(str, ns)) + " : " + " ".join(str(r._randbelow(n)) for n in ns))

  r = fresh()
  lines.append("random : " + " ".join(str(int(r.random() * (1 << 53))) for _ in range(N)))

  r = fresh()
  pairs = []
  for _ in range(N):
    a = helper.randint(-100, 100)
    pairs.append((a, a + helper.randint(0, 300)))
  lines.append("randint " + " ".join(f"{a} {b}" for a, b in pairs) + " : " +
               " ".join(str(r.randint(a, b)) for a, b in pairs))

  r = fresh()
  pairs = []
  for _ in range(N):
    a = helper.randint(0, 20)
    pairs.append((a, a + helper.randint(1, 1000)))
  lines.append("randrange " + " ".join(f"{a} {b}" for a, b in pairs) + " : " +
               " ".join(str(r.randrange(a, b)) for a, b in pairs))

  r = fresh()
  lens = [1 + (i % 9) if i % 3 else 1 + helper.randrange(400) for i in range(N)]
  lines.append("choice " + " ".join(map(str, lens)) + " : " +
               " ".join(str(r.choice(range(m))) for m in lens))

  r = fresh()
  produced = 0
  sizes = [0, 1, 2, 3, 50, 7, 100, 320]
  i = 0
  while produced < N:
    n = sizes[i] if i < len(sizes) else helper.randrange(1, 120)
    i += 1
    x = list(range(n))
    r.shuffle(x)
    lines.append(f"shuffle {n} : " + " ".join(map(str, x)))
    produced += max(n, 1)

  r = fresh()
  produced = 0
  # Both of sample's methods: pool (n <= setsize) and set (n > setsize), around the thresholds
  # (k <= 5: 21; k = 6: 21 + 64 = 85; k = 22: 21 + 256 = 277).
  fixed = [(6, 4), (21, 1), (22, 1), (21, 5), (22, 5), (85, 6), (86, 6), (277, 22), (278, 22), (320, 320),
           (1, 1), (5, 0), (36, 2), (500, 7)]
  i = 0
  while produced < N:
    if i < len(fixed):
      n, k = fixed[i]
    else:
      n = helper.randrange(1, 400)
      k = helper.randrange(0, min(n, 40) + 1)
    i += 1
    s = r.sample(range(n), k)
    lines.append(f"sample {n} {k} : " + " ".join(map(str, s)))
    produced += max(k, 1)

  r = fresh()
  for i in range(N):
    m = 1 + helper.randrange(30)
    counts = [helper.choice([1, 1, 1, 2, 4, 8, 16, 0]) for _ in range(m)]
    if sum(counts) == 0:
      counts[-1] = 1
    idx = r.sample(range(m), 1, counts=counts)[0]
    lines.append("weighted " + " ".join(map(str, counts)) + f" : {idx}")

  r = fresh()
  for i in range(N // 5):
    m = 1 + helper.randrange(20)
    w = [helper.choice([1, 2, 3, 5, 10]) for _ in range(m)]
    idx = r.choices(range(m), weights=w)[0]
    lines.append("choices " + " ".join(map(str, w)) + f" : {idx}")
  return lines


def build():
  oc.bootstrap()
  with contextlib.redirect_stdout(sys.stderr):
    import randomizer
    from randomizer import WWRandomizer, RNG_CHANGING_OPTIONS
  if len(RNG_CHANGING_OPTIONS) != 11:
    oc.die(f"RNG_CHANGING_OPTIONS has {len(RNG_CHANGING_OPTIONS)} entries, the C++ port expects 11")

  lines = [
    "# R0.2 RNG/MD5 vectors (docs/RANDOMIZER_PLAN.md). Generated by native/tools/rando/gen_rng_vectors.py",
    f"# from wwrando {oc.EXPECTED_VERSION} on CPython {platform.python_version()}; do not edit by hand.",
    "# RNG_CHANGING_OPTIONS: " + " ".join(RNG_CHANGING_OPTIONS),
  ]
  lines += md5_lines()

  class Stub:  # just enough of a WWRandomizer for its unbound seed methods
    pass

  for name, seed, overrides, key in SEED_CASES:
    opts = oc.make_options(overrides)
    randomizer.SEED_KEY = key or ""
    stub = Stub()
    stub.options = opts
    sanitized = WWRandomizer.sanitize_seed(seed)
    seed_string = sanitized + (randomizer.SEED_KEY if opts.do_not_generate_spoiler_log else "")
    stub.integer_seed = WWRandomizer.convert_string_to_integer_md5(stub, seed_string)
    rng = WWRandomizer.get_new_rng(stub)
    values = [int(opts[o]) for o in RNG_CHANGING_OPTIONS]
    lines.append(f"case {name}")
    lines.append(f"seed {sanitized} {key or '-'} {stub.integer_seed:032x} " + " ".join(map(str, values)))
    lines.append(state_line(rng))
    lines += op_blocks(rng, N if name in FULL_CASES else N_SMALL)
  randomizer.SEED_KEY = ""

  for name, n in INT_CASES:
    rng = random.Random(n)
    lines.append(f"case {name}")
    lines.append(f"int {n}")
    lines.append(state_line(rng))
    lines += op_blocks(rng, N if name in FULL_CASES else N_SMALL)
  return "\n".join(lines) + "\n"


def main():
  ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("--out", default=DEFAULT_OUT)
  ap.add_argument("--check", action="store_true", help="compare with --out instead of writing it")
  a = ap.parse_args()
  text = build()
  if a.check:
    try:
      with open(a.out) as f:
        old = f.read()
    except OSError:
      old = None
    if old != text:
      print(f"{a.out} differs from a fresh run", file=sys.stderr)
      return 1
    print("same")
    return 0
  with open(a.out, "w") as f:
    f.write(text)
  print(f"wrote {a.out} ({len(text)} bytes)", file=sys.stderr)
  return 0


if __name__ == "__main__":
  sys.exit(main())
