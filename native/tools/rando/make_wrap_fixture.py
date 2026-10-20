#!/usr/bin/env python3
"""Fixtures for the randomizer's text helpers (docs/RANDOMIZER_PLAN.md step R3.20).

    python3 -I native/tools/rando/make_wrap_fixture.py [--out-dir DIR] [--disc GZLE01.iso]

Writes, with wwrando's and gclib's own code (ref/wwrando, MIT; gclib bfn.py and bmg.py):

  R3_20_wrap.txt (test_R3_20.cpp; no disc data): gclib's BFN.word_wrap_string run on a fixed,
  made-up width table (the "font" below, which stands in for the BFN's WID1/MAP1 lookups), over
  every wwrando string constant with a control tag plus hand-written cases, at several line lengths;
  Message.pad_string_to_next_4_lines and construct_string_from_parts; and tweaks.py's
  add_article_before_item_name / upper_first_letter / get_indefinite_article for every name in
  data/item_names.txt. Lines (all strings as UTF-8 hex; "-" for an empty one):

      F <256 widths, code 0..255> <replacement width>     the width table (once, first)
      W <max_line_length> <preserve 0|1> <in> <out>       BFN.word_wrap_string
      P <in> <out>                                        Message.pad_string_to_next_4_lines
      C <text_box_type> <extra_line_length> <parts joined by ","> <out>   construct_string_from_parts
      A <item name> <add_article_before_item_name> <upper_first_letter of that> <get_indefinite_article>

  R3_20_wrap_font.txt (smoke `wrap`; needs --disc): the three sample strings of the smoke, wrapped
  with the disc's real message font (files/res/Msg/fontres.arc rock_24_20_4i_usa.bfn, the font
  wwrando loads as self.bfn) at their text box's width. The fixture holds only the wrapped text
  (no font data):

      M <text_box_type> <in> <out>
"""
import os
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oracle_common as oc  # noqa: E402

oc.bootstrap()

import ast  # noqa: E402
import re  # noqa: E402
import types  # noqa: E402

from gclib.bfn import BFN  # noqa: E402
from gclib.bmg import Message, TextBoxType  # noqa: E402

# The made-up font: width of character code c (0..255), and the replacement code's width.
SYN_WIDTHS = [(c * 7 + 3) % 23 + 4 for c in range(256)]
SYN_WIDTHS[ord(" ")] = 9
SYN_REPLACEMENT = 17


def syn_width(char):
  """The BFN lookup with the made-up table: cp1252 byte -> width, else the replacement's."""
  try:
    b = char.encode("cp1252")
  except UnicodeEncodeError:
    return SYN_REPLACEMENT
  return SYN_WIDTHS[b[0]] if len(b) == 1 else SYN_REPLACEMENT


def syn_font():
  font = BFN.__new__(BFN)
  font.get_char_width = syn_width
  return font


EXTRA = [
  "",
  " ",
  "a",
  "Hello world",
  "  leading spaces and trailing spaces   ",
  "Several   spaces    between words, and a very long line that has to be wrapped more than once.",
  "Supercalifragilisticexpialidocious-Pneumonoultramicroscopicsilicovolcanoconiosis word",
  "Short\nlines\n\nwith blank ones   \n and spaces before a newline  \nend",
  "Trailing newline\n",
  "\n\nleading newlines",
  "\\{1A 05 00 00 01}You got \\{1A 06 FF 00 00 01}the Wind Waker\\{1A 06 FF 00 00 00}! Its melody lets "
  "you control the wind.",
  "Tags\\{1A 07 00 00 07 00 1E}inside\\{1A 06 FF 00 00 01}words\\{1A 06 FF 00 00 00} and a long "
  "\\{1A 06 FF 00 00 01}Tower of the Gods Small Key\\{1A 06 FF 00 00 00} name.",
  "Café ¡Hola! ¿Qué tal? Señor Ñandú, garçon. €uro — dash … ÿ",
  "Control chars \u0081 and ☃ use the replacement width.",
  "x" * 120,
  "word " * 40,
  "They say that \\{1A 06 FF 00 00 01}Dragon Roost Cavern\\{1A 06 FF 00 00 00} is "
  "\\{1A 06 FF 00 00 02}on the path\\{1A 06 FF 00 00 00} to "
  "\\{1A 06 FF 00 00 05}Gohma\\{1A 06 FF 00 00 00}.",
]

LINE_LENGTHS = [503, 419, 200, 64, 20]

# The smoke's three sample strings and their text box types (pc_rando_smoke_wrap.cpp kSamples).
SAMPLES = [
  (TextBoxType.ITEM_GET,
   "\\{1A 05 00 00 01}You got the \\{1A 06 FF 00 00 01}Tower of the Gods Small Key"
   "\\{1A 06 FF 00 00 00}! It opens one locked door in the Tower of the Gods."),
  (TextBoxType.DIALOG,
   "They say that \\{1A 06 FF 00 00 01}the Wind Waker\\{1A 06 FF 00 00 00} can be found "
   "on \\{1A 06 FF 00 00 01}Dragon Roost Island\\{1A 06 FF 00 00 00}, but whoever told you "
   "that was probably lying about the whole thing anyway."),
  (TextBoxType.DIALOG,
   "Hey, \\{1A 05 00 00 00}! A Piece of Heart, a Heart Container and 200 Rupees for you. "
   "Counterproductiveness-is-a-really-long-word-without-spaces-inside.\nNew line here."),
]


def wwrando_strings():
  found = []
  for dirpath, dirnames, filenames in os.walk(oc.WWRANDO_DIR):
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
          if "[0-9a-f]" in node.value:  # a regex, not a message
            continue
          found.append(node.value)
  return found


def h(s):
  return s.encode("utf-8").hex() if s else "-"


def message(box_type):
  msg = Message.__new__(Message)
  msg.text_box_type = box_type
  return msg


def item_names():
  with open(os.path.join(oc.WWRANDO_DIR, "data", "item_names.txt"), "r") as f:
    matches = re.findall(r"^([0-9a-f]{2}) - (.+)$", f.read(), re.IGNORECASE | re.MULTILINE)
  return [name for _, name in matches if name]


def write_synthetic(path):
  font = syn_font()
  lines = ["# R3.20: gclib word wrap / padding / parts and wwrando's articles on a made-up width "
           "table (make_wrap_fixture.py)",
           "F " + " ".join(str(w) for w in SYN_WIDTHS) + " %d" % SYN_REPLACEMENT]
  seen = set()
  strings = []
  for s in EXTRA + wwrando_strings():
    if s not in seen:
      seen.add(s)
      strings.append(s)
  nwrap = 0
  for s in strings:
    for length in LINE_LENGTHS:
      for preserve in (1, 0):
        try:
          out = font.word_wrap_string(s, length, preserve_control_codes=bool(preserve))
        except (AssertionError, ValueError, IndexError):
          continue  # gclib raises on a malformed tag; the C++ port documents its own choice
        lines.append("W %d %d %s %s" % (length, preserve, h(s), h(out)))
        nwrap += 1
  pad_msg = message(TextBoxType.DIALOG)
  for s in ["", "one", "one\ntwo", "a\nb\nc", "a\nb\nc\nd", "a\nb\nc\nd\ne", "x\n", "\n\n\n\n"]:
    lines.append("P %s %s" % (h(s), h(pad_msg.pad_string_to_next_4_lines(s))))
  part_sets = [
    (TextBoxType.DIALOG, 0, ["Hello world"]),
    (TextBoxType.DIALOG, 0, strings[5:9]),
    (TextBoxType.ITEM_GET, 0, strings[10:13]),
    (TextBoxType.HINT, 25, [strings[16], strings[15], "short"]),
    (TextBoxType.WIND_WAKER_SONG, -40, [strings[4], strings[7]]),
  ]
  for box_type, extra, parts in part_sets:
    msg = message(box_type)
    msg.construct_string_from_parts(font, parts, extra)
    lines.append("C %d %d %s %s" % (box_type.value, extra, ",".join(h(p) for p in parts),
                                    h(msg.string)))
  with oc_quiet():
    import tweaks
  names = item_names()
  for name in names:
    art = tweaks.add_article_before_item_name(name)
    lines.append("A %s %s %s %s" % (h(name), h(art), h(tweaks.upper_first_letter(art)),
                                    tweaks.get_indefinite_article(name)))
  with open(path, "w", encoding="ascii") as f:
    f.write("\n".join(lines) + "\n")
  print("%s: %d wraps, %d item names" % (path, nwrap, len(names)), file=sys.stderr)


def oc_quiet():
  import contextlib
  return contextlib.redirect_stdout(sys.stderr)


def write_font(path, disc):
  from gclib.gcm import GCM
  from gclib.rarc import RARC
  gcm = GCM(disc)
  gcm.read_entire_disc()
  arc = RARC(gcm.read_file_data("files/res/Msg/fontres.arc"))
  font = arc.get_file("rock_24_20_4i_usa.bfn", BFN)
  print("font encoding %s, replacement code 0x%X" % (font.inf1s[0].encoding_type,
                                                     font.inf1s[0].replacement_code), file=sys.stderr)
  lines = ["# R3.20: the smoke's samples wrapped with the disc's rock_24_20_4i_usa.bfn "
           "(make_wrap_fixture.py --disc; text only, no font data)"]
  for box_type, s in SAMPLES:
    msg = message(box_type)
    lines.append("M %d %s %s" % (box_type.value, h(s), h(msg.word_wrap_string_part(font, s))))
  with open(path, "w", encoding="ascii") as f:
    f.write("\n".join(lines) + "\n")
  print("%s: %d samples" % (path, len(SAMPLES)), file=sys.stderr)


def main():
  args = sys.argv[1:]
  out_dir = os.path.join(oc.REPO_ROOT, "native", "check", "rando", "fixtures")
  disc = None
  while args:
    a = args.pop(0)
    if a == "--out-dir" and args:
      out_dir = args.pop(0)
    elif a == "--disc" and args:
      disc = args.pop(0)
    else:
      oc.die("usage: make_wrap_fixture.py [--out-dir DIR] [--disc GZLE01.iso]")
  write_synthetic(os.path.join(out_dir, "R3_20_wrap.txt"))
  if disc:
    write_font(os.path.join(out_dir, "R3_20_wrap_font.txt"), disc)


if __name__ == "__main__":
  main()
