#!/usr/bin/env python3
"""Fixture for the message encoder (docs/RANDOMIZER_PLAN.md step R0.12, test_R0_12.cpp).

    python3 -I native/tools/rando/make_msg_fixture.py [out]   (default native/check/rando/fixtures/R0_12_bmg_strings.txt)

Encodes message strings with gclib's own BMG code (bmg.py Message.write_string, then read_string on
the result) and writes one line per string:

    <utf-8 input, hex> <tab> <write_string bytes incl. the NUL, hex> <tab> <read_string result, utf-8 hex>

The strings are every string constant in wwrando's Python sources (ref/wwrando, MIT) that holds a
control-tag escape ("\\{") and is not a regex, plus a few hand-written cases for write_string's quirks. No disc data.
pc_rando_msg_encode must give the same bytes and pc_rando_msg_decode the same read_string text.
"""
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

oc.bootstrap()

import ast  # noqa: E402
import io  # noqa: E402
import types  # noqa: E402

from gclib.bmg import Message  # noqa: E402

EXTRA = [
  "",
  "Plain ASCII text.",
  "Two\nlines\n\n\nand a box break",
  "Café ¡Hola! ¿Qué? ÿ",
  "Back\\slash then {41 42} is a tag group",  # the escape flag is never reset
  "\\{1A 05 00 00 01}You got \\{1A 06 FF 00 00 01}magic power\\{1A 06 FF 00 00 00}!",
  "\\{1a 06 ff 00 00 05}lower-case hex",
  "\\{1A,07;FF 00é01 00 96}separators",
  "\\{001A 05 00 00 08}leading zeros",
  "Wait\\{1A 07 00 00 07 00 1E}here",
]


def wwrando_strings():
  root = oc.WWRANDO_DIR
  found = []
  for dirpath, dirnames, filenames in os.walk(root):
    dirnames[:] = sorted(d for d in dirnames if not d.startswith(".") and d not in ("gclib", "build"))
    for fn in sorted(filenames):
      if not fn.endswith(".py"):
        continue
      path = os.path.join(dirpath, fn)
      with open(path, encoding="utf-8") as f:
        try:
          tree = ast.parse(f.read(), path)
        except SyntaxError:
          continue
      for node in ast.walk(tree):
        if isinstance(node, ast.Constant) and isinstance(node.value, str) and "\\{" in node.value:
          if "[0-9a-f]" in node.value:  # a regex that matches tags, not a message
            continue
          found.append(node.value)
  return found


def encode(string):
  data = io.BytesIO(bytes(8 + 4 * len(string.encode("utf-8")) + 64))
  msg = Message.__new__(Message)
  msg.bmg = types.SimpleNamespace(dat1=types.SimpleNamespace(data=data))
  msg.string_offset = 0
  msg.string = string
  msg.write_string()
  raw = data.getvalue()[8:8 + msg.encoded_string_length]
  msg.read_string()
  return raw, msg.string


def main():
  out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    oc.REPO_ROOT, "native", "check", "rando", "fixtures", "R0_12_bmg_strings.txt")
  seen, lines = set(), []
  for s in EXTRA + wwrando_strings():
    if s in seen:
      continue
    seen.add(s)
    try:
      raw, back = encode(s)
    except Exception:  # noqa: BLE001 - e.g. a format string with a code point write_string refuses
      continue
    lines.append("%s\t%s\t%s" % (s.encode("utf-8").hex(), raw.hex(), back.encode("utf-8").hex()))
  with open(out, "w", encoding="ascii") as f:
    f.write("# R0.12: gclib bmg.py Message.write_string/read_string on wwrando's strings "
            "(make_msg_fixture.py)\n")
    f.write("\n".join(lines) + "\n")
  print("%s: %d strings" % (out, len(lines)), file=sys.stderr)


if __name__ == "__main__":
  main()
