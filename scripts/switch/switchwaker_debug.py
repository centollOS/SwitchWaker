#!/usr/bin/env python3
"""Client of the SwitchWaker debug server (switch/native/source/debug_server.h, docs/DEBUG_SERVER.md).

The console runs it when the options menu turns it on (Depuración > Servidor de depuración, at the next start); its
log says '[switch] debug server listening on <ip>:6543'. The console's address: --host, else SWITCHWAKER_HOST, else the first line of build/switch_host.txt.

  switchwaker_debug.py info                      build, frame, stage, memory, address
  switchwaker_debug.py log [--all] [--save F] [--grep RE] [--seconds N]
                                                 the log as it is written (Ctrl-C ends it)
  switchwaker_debug.py deploy [NRO] [--no-reload] [--nro-only]
                                                 upload the NRO (default build/switch-native/switchwaker.nro)
                                                 and the caches built next to it,
                                                 check it, restart
  switchwaker_debug.py shot [OUT.png] [--game]   screenshot of the next frame
  switchwaker_debug.py press A [B ...] [ms]      also hold, release, stick L|R x y [ms]
  switchwaker_debug.py warps | warp N | warp STAGE [ROOM] [POINT]
  switchwaker_debug.py get REMOTE [LOCAL] | put LOCAL REMOTE | ls [REMOTE] | rm REMOTE | mkdir REMOTE
  switchwaker_debug.py logs | lastlog [LOCAL]    session logs (native/logs/); the newest one (also while it runs)
  switchwaker_debug.py crashes [--fetch DIR]     Atmosphere's crash reports
  switchwaker_debug.py wait [--seconds N]        until the server answers (after a reload)
  switchwaker_debug.py quit | reload | crash | ping | help
  switchwaker_debug.py raw COMMAND ARGS...       any command, its reply printed

Remote paths are relative to sdmc:/switch/switchwaker unless they start with /.
"""
import argparse
import os
import re
import socket
import sys
import time
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEFAULT_PORT = 6543
# What differs between the two ports' copies of this client (SwitchWakerHD: tools/switch/wwhd_debug.py)
PORT_NAME = "SwitchWaker"
MENU_SWITCH = "Depuración > Servidor de depuración"  # the menu setting that starts the server
ENV_HOST = "SWITCHWAKER_HOST"           # the console's address on this computer
ENV_PORT = "SWITCHWAKER_DEBUG_PORT"
DEFAULT_NRO = REPO / "build" / "switch-native" / "switchwaker.nro"
BUILD_HINT = "scripts/switch/build_native.sh"
REMOTE_NRO = "switchwaker.nro"          # relative to the server's root, sdmc:/switch/switchwaker
REMOTE_LOGS = "native/logs"
SESSION_LOG = r"switchwaker(?:_gl|_dk)?_\d{4}-\d\d-\d\d_\d\d-\d\d-\d\d\.log"


class ServerError(Exception):
    pass


def default_host():
    if os.environ.get(ENV_HOST):
        return os.environ[ENV_HOST]
    f = REPO / "build" / "switch_host.txt"
    if f.exists():
        line = f.read_text().strip().splitlines()
        if line:
            return line[0].strip()
    return None


def quote(arg):
    return f'"{arg}"' if (" " in arg or not arg) else arg


class Conn:
    def __init__(self, host, port, timeout=10.0):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.buf = b""

    def close(self):
        self.sock.close()

    def _fill(self):
        data = self.sock.recv(1 << 16)
        if not data:
            raise ConnectionError("the console closed the connection")
        self.buf += data

    def _line(self):
        while b"\n" not in self.buf:
            self._fill()
        line, self.buf = self.buf.split(b"\n", 1)
        return line.decode()

    def _exact(self, n, out=None):
        """n bytes: returned, or written to the file object out"""
        chunks = []
        while n:
            if not self.buf:
                self._fill()
            take = self.buf[:n]
            self.buf = self.buf[len(take):]
            n -= len(take)
            if out:
                out.write(take)
            else:
                chunks.append(take)
        return b"".join(chunks)

    def header(self):
        status, size = self._line().split(" ", 1)
        return status == "ok", int(size)

    def command(self, *args, payload=None, out=None, timeout=None):
        """sends a command; its reply (bytes, or written to out). Raises ServerError on 'err'."""
        if timeout is not None:
            self.sock.settimeout(timeout)
        self.sock.sendall((" ".join(quote(str(a)) for a in args) + "\n").encode())
        if payload is not None:
            self.sock.sendall(payload)
        ok, size = self.header()
        if not ok:
            raise ServerError(self._exact(size).decode(errors="replace"))
        return self._exact(size, out)


def connect(args, timeout=10.0):
    if not args.host:
        sys.exit(f"the console's address: --host IP, {ENV_HOST}=IP, or build/switch_host.txt")
    try:
        return Conn(args.host, args.port, timeout)
    except OSError as e:
        sys.exit(f"cannot reach {args.host}:{args.port}: {e} (is {PORT_NAME} running with its debug server on? {MENU_SWITCH}, then restart)")


# Commands that change the console's files or the running game.
CHANGING_COMMANDS = ("deploy", "put", "rm", "mkdir", "reload", "quit", "crash", "warp", "press", "hold",
                     "release", "stick", "raw")


def check_game(args):
    """SwitchWakerHD's debug server listens on the same port and answers the same commands: refuse to
    change anything when the game that answers is not SwitchWaker (a deploy once wrote this NRO and
    its caches into sdmc:/switch/wwhd/ and restarted WWHD). SwitchWaker's info gives its version as
    git describe ("0.2.0-25"); WWHD's as "v0.4.1+3 (43d74c8)"."""
    c = connect(args)
    try:
        text = c.command("info", timeout=30.0).decode(errors="replace")
    finally:
        c.close()
    first = text.splitlines()[0] if text else ""
    version = first.split(" ", 1)[1].strip() if first.startswith("version ") else ""
    if version.startswith("v") or "(" in version:
        sys.exit(f"the console runs another game ({first.strip()}), not {PORT_NAME}: nothing sent. "
                 f"Start {PORT_NAME} and try again.")


def simple(args, *cmd, timeout=30.0):
    c = connect(args)
    try:
        text = c.command(*cmd, timeout=timeout).decode(errors="replace")
    finally:
        c.close()
    if text:
        print(text, end="" if text.endswith("\n") else "\n")


def cmd_log(args):
    c = connect(args)
    c.sock.sendall(b"log all\n" if args.all else b"log\n")
    ok, _ = c.header()
    if not ok:
        sys.exit("the server refused the log stream")
    c.sock.settimeout(1.0)
    pattern = re.compile(args.grep) if args.grep else None
    save = open(args.save, "a", encoding="utf-8") if args.save else None
    end = time.time() + args.seconds if args.seconds else None
    pending = c.buf.decode(errors="replace")
    c.buf = b""
    try:
        while end is None or time.time() < end:
            lines = pending.split("\n")
            pending = lines.pop()
            for line in lines:
                if pattern and not pattern.search(line):
                    continue
                print(line, flush=True)
                if save:
                    save.write(line + "\n")
                    save.flush()
            try:
                data = c.sock.recv(1 << 16)
            except socket.timeout:
                continue
            if not data:
                print("[debug client] the console closed the stream", file=sys.stderr)
                break
            pending += data.decode(errors="replace")
    except KeyboardInterrupt:
        pass
    finally:
        c.close()
        if save:
            save.close()


def put_file(c, local, remote):
    data = Path(local).read_bytes()
    t0 = time.time()
    reply = c.command("put", remote, len(data), payload=data, timeout=max(60.0, len(data) / 1e5)).decode()
    want = f"crc32 {zlib.crc32(data) & 0xFFFFFFFF:08x} size {len(data)}"
    if reply != want:
        raise ServerError(f"upload check failed: console says '{reply}', expected '{want}'")
    dt = time.time() - t0
    print(f"{local} -> {remote}: {len(data) / 1048576:.1f} MiB in {dt:.1f} s ({len(data) / 1048576 / max(dt, 1e-3):.1f} MiB/s), crc32 ok")


def cmd_put(args):
    c = connect(args)
    try:
        put_file(c, args.local, args.remote)
    finally:
        c.close()


def cmd_get(args):
    local = args.local or Path(args.remote).name
    c = connect(args)
    try:
        with open(local, "wb") as f:
            c.command("get", args.remote, out=f, timeout=120.0)
    except ServerError:
        os.remove(local)
        raise
    finally:
        c.close()
    print(f"{args.remote} -> {local} ({os.path.getsize(local)} bytes)")


def wait_server(args, seconds):
    end = time.time() + seconds
    while time.time() < end:
        try:
            c = Conn(args.host, args.port, timeout=2.0)
            c.command("ping", timeout=5.0)
            c.close()
            return True
        except (OSError, ServerError, ValueError):
            time.sleep(1.0)
    return False


def cmd_deploy(args):
    nro = Path(args.nro or DEFAULT_NRO)
    if not nro.exists():
        sys.exit(f"{nro} does not exist ({BUILD_HINT} builds it)")
    c = connect(args)
    try:
        put_file(c, nro, args.remote)
        # the bundled caches built next to the NRO (deko3d's DKSH file changes with every build)
        remote_dir = args.remote.rpartition("/")[0]
        for extra in ([] if args.nro_only else ["initial_pipeline_cache.db", "initial_dksh_cache.bin"]):
            if (nro.parent / extra).exists():
                put_file(c, nro.parent / extra, f"{remote_dir}/{extra}" if remote_dir else extra)
        if args.no_reload:
            return
        print(c.command("reload", timeout=10.0).decode())
    finally:
        c.close()
    time.sleep(3.0)  # the old process goes away first
    print("waiting for the game to come back...", flush=True)
    if not wait_server(args, 120):
        sys.exit("the server did not come back within 120 s (crashed at start? get the log with lastlog)")
    simple(args, "info")


def cmd_shot(args):
    out = args.out or time.strftime("shot_%Y%m%d_%H%M%S.png")
    c = connect(args)
    try:
        with open(out, "wb") as f:
            c.command("shot", *(["game"] if args.game else []), out=f, timeout=40.0)
    except ServerError:
        os.remove(out)
        raise
    finally:
        c.close()
    print(out)


def remote_ls(args, path):
    c = connect(args)
    try:
        text = c.command("ls", path).decode(errors="replace")
    finally:
        c.close()
    entries = []
    for line in text.splitlines():
        kind, size, name = line[0], line[2:14].strip(), line[15:]
        entries.append((kind, size, name))
    return entries


def cmd_lastlog(args):
    # session logs are named by their start time (<name>_<date>_<time>.log); other names are older builds'
    logs = sorted((name for kind, _, name in remote_ls(args, REMOTE_LOGS)
                   if kind == "f" and re.fullmatch(SESSION_LOG, name)),
                  key=lambda name: re.search(r"\d{4}-\d\d-\d\d_\d\d-\d\d-\d\d", name).group())
    if not logs:
        sys.exit(f"no session logs in {REMOTE_LOGS}/")
    args.remote = REMOTE_LOGS + "/" + logs[-1]
    args.local = args.local or logs[-1]
    try:
        cmd_get(args)
    except ServerError:
        # the running session: its file is open for writing, the console keeps its text in memory
        c = connect(args)
        try:
            Path(args.local).write_bytes(c.command("logtext", timeout=30.0))
        finally:
            c.close()
        print(f"{args.remote} is the running session's log: {args.local} has the text the console keeps (the last 2 MiB)")


def cmd_crashes(args):
    entries = [e for e in remote_ls(args, "/atmosphere/crash_reports") if e[0] == "f"]
    for _, size, name in entries:
        print(f"{size:>10}  {name}")
    if args.fetch:
        Path(args.fetch).mkdir(parents=True, exist_ok=True)
        for _, _, name in entries:
            local = Path(args.fetch) / name
            if local.exists():
                continue
            args.remote, args.local = "/atmosphere/crash_reports/" + name, str(local)
            cmd_get(args)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", default=default_host())
    p.add_argument("--port", type=int, default=int(os.environ.get(ENV_PORT, DEFAULT_PORT)))
    sub = p.add_subparsers(dest="cmd", required=True)

    for name in ("info", "ping", "help", "warps", "quit", "reload", "crash"):
        sub.add_parser(name)
    for name in ("press", "hold", "release", "warp", "stick"):
        sp = sub.add_parser(name)
        sp.add_argument("rest", nargs="*")
    sp = sub.add_parser("raw")
    sp.add_argument("rest", nargs="+")
    sp = sub.add_parser("log")
    sp.add_argument("--all", action="store_true", help="start with the last 2 MiB the console kept")
    sp.add_argument("--save", help="also append the lines to this file")
    sp.add_argument("--grep", help="only lines matching this regular expression")
    sp.add_argument("--seconds", type=float, help="stop after this long")
    sp = sub.add_parser("get")
    sp.add_argument("remote")
    sp.add_argument("local", nargs="?")
    sp = sub.add_parser("put")
    sp.add_argument("local")
    sp.add_argument("remote")
    for name in ("ls", "rm", "mkdir"):
        sp = sub.add_parser(name)
        sp.add_argument("remote", nargs="?" if name == "ls" else None, default="")
    sp = sub.add_parser("deploy")
    sp.add_argument("nro", nargs="?")
    sp.add_argument("--remote", default=REMOTE_NRO, help=f"where on the SD card (default {REMOTE_NRO})")
    sp.add_argument("--no-reload", action="store_true")
    sp.add_argument("--nro-only", action="store_true",
                    help="only the NRO, not the initial_pipeline_cache.db / initial_dksh_cache.bin next to it")
    sp = sub.add_parser("shot")
    sp.add_argument("out", nargs="?")
    sp.add_argument("--game", action="store_true", help="the game's picture alone (no bars, counter or menu)")
    sub.add_parser("logs")
    sp = sub.add_parser("lastlog")
    sp.add_argument("local", nargs="?")
    sp = sub.add_parser("crashes")
    sp.add_argument("--fetch", metavar="DIR", help="download the reports not yet in DIR")
    sp = sub.add_parser("wait")
    sp.add_argument("--seconds", type=float, default=120)

    args = p.parse_args()
    try:
        if args.cmd in CHANGING_COMMANDS:
            check_game(args)
        if args.cmd in ("info", "ping", "help", "warps", "quit", "reload", "crash"):
            simple(args, args.cmd)
        elif args.cmd in ("press", "hold", "release", "warp", "stick"):
            simple(args, args.cmd, *args.rest, timeout=120.0)
        elif args.cmd == "raw":
            simple(args, *args.rest, timeout=120.0)
        elif args.cmd in ("ls", "rm", "mkdir"):
            simple(args, args.cmd, *([args.remote] if args.remote else []))
        elif args.cmd == "logs":
            for kind, size, name in remote_ls(args, REMOTE_LOGS):
                print(f"{size:>10}  {name}")
        elif args.cmd == "wait":
            if not wait_server(args, args.seconds):
                sys.exit("no answer")
            print("ready")
        else:
            {"log": cmd_log, "get": cmd_get, "put": cmd_put, "deploy": cmd_deploy, "shot": cmd_shot,
             "lastlog": cmd_lastlog, "crashes": cmd_crashes}[args.cmd](args)
    except ServerError as e:
        sys.exit(f"console: {e}")
    except BrokenPipeError:  # (the output went to a pipe that closed: head, grep -m)
        os._exit(0)
    except (ConnectionError, socket.timeout) as e:
        sys.exit(f"connection: {e}")


if __name__ == "__main__":
    main()
