// Debug server: a TCP port on the console for the development machine: the log as it is written, files on
// the SD card (get, put, ls, rm), controller presses injected into the game's input, and the commands the
// game adds (info, warp, screenshot, reload, quit). Off unless the settings menu turns it on (Debug, at the
// next start); only for the local network: there is no password.
//
// This file and debug_server.cpp know nothing of the game and are the same in both ports (keep them so):
//   SwitchWakerHD: runtime/src/platform/, commands in debug_switch.cpp, setting switchDebugServer, client
//                  tools/switch/wwhd_debug.py, docs/debug-server.md
//   SwitchWaker:   switch/native/source/, commands in cos_debug.cpp, setting COS_DEBUG_SERVER, client
//                  scripts/switch/switchwaker_debug.py, docs/DEBUG_SERVER.md
// They build on the Switch (libnx sockets) and on POSIX hosts (debug_server_host_test.cpp next to the client).
//
// Protocol: one command per line ("name arg arg ...", "double quotes" around an argument with spaces).
// Every reply is "ok <n>\n" or "err <n>\n" followed by n bytes (text, or a file's contents). "put <path>
// <size>" is followed by the file's size bytes. "log" turns the connection into a stream of the log's text
// until the client closes it; "logtext" replies with the log text kept (the last 2 MiB: the running session's log
// file cannot be read while the game writes it).
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace debugsrv {

using Args = std::vector<std::string>;  // the command's arguments (its name not included)

struct Reply {
    bool ok = true;
    std::string text;                 // the reply's bytes, unless file is set
    std::string file;                 // send this file's contents instead
    bool removeFile = false;          // remove file once sent (a screenshot)
    std::function<void()> after;      // run once the reply is sent (quit, reload)
};
inline Reply ok(std::string text = {}) { return {true, std::move(text), {}, false, {}}; }
inline Reply err(std::string text) { return {false, std::move(text), {}, false, {}}; }

using Handler = std::function<Reply(const Args&)>;

struct Config {
    int port = 6543;
    std::string root;                              // relative paths of get / put / ls / rm / mkdir
    std::function<void(const char*)> log;          // a line for the game's log (no newline)
    std::function<void()> threadStart;             // run first on each of the server's threads (priority, core)
};

// starts the listening thread (sockets initialised on the Switch); false when the port cannot be opened
bool start(const Config& config);
bool running();
// commands the game adds; any thread, before or after start. Handlers run on the connection's thread.
void add_command(const char* name, const char* usage, Handler handler);
// the log's text as it is written (the game's log writer): kept for "log" streams; cheap while not running
// and not keeping
void log_tap(const char* data, size_t size);
// keep the log's text before start (the first lines, written before the run options are read); drop_log
// lets it go when the server will not start
void keep_log();
void drop_log();
// closes the sockets (the Switch: and the socket service) before the program ends or restarts
void stop();

// ---- controller presses from "press", "hold", "release", "stick"
// Buttons use libnx's HidNpadButton bits (A = bit 0 ... Down = bit 15) on every host.
struct Injected {
    uint64_t buttons = 0;  // held now
    bool stick[2] = {};    // left, right: overridden now
    float x[2] = {}, y[2] = {};  // -1..1, y up
};
Injected injected();  // the game's input poll, every time it reads the controller

}  // namespace debugsrv
