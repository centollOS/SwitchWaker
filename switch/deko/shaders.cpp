// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// The shader machinery follows SwitchWakerHD (https://github.com/centollOS/SwitchWakerHD) at df8fbde:
// runtime/src/gfx/deko/shaders_dk.cpp (one uam worker with a large stack at priority 0x3B, the queue
// order, the per-frame load budget, skipped draws until a shader is ready, the local cache appended
// as shaders compile, the failure dumps); the registry and the keys are this project's.
// The deko3d renderer's shaders at run time (dk.h, docs/DEKO3D_MIGRATION_PLAN.md section 6):
//   - the registry: every DKSH by (stage, GLSL hash), filled from initial_dksh_cache.bin and
//     dksh_local.bin at start (shader_cache.cpp, shaders_init) and by the uam worker;
//   - the uam worker: one thread (uam is not thread-safe), 8 MiB of stack, priority 0x3B, off the
//     render worker's core (as Aurora's compile thread: COS_SWITCH_COMPILE_CORE); jobs come from
//     Aurora's compile thread (switch/deko/aurora: WGSL -> Tint -> post-pass), foreground (a draw
//     waits for it) before background (the warm-up), the stage that skipped most draws first;
//   - dksh_local.bin (native/user/cache/): every DKSH compiled here, appended as a record with its CRC
//     (dksh_file.h), read back at the next start, ignored as a whole after a compiler change;
//   - the load budget: the render worker copies at most COS_DK_SHADER_BUDGET compiled shaders a frame
//     into code memory (default 64; 0 = no limit); a draw whose shader is not loaded yet is skipped
//     (a brief pop-in) and counted;
//   - the "[cos] shaders:" line of the perf window (shaders_report).
#include "dk.h"

#include <pthread.h>
#include <switch.h>

#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <unordered_map>

#include "dksh_file.h"
#include "uam_api.h"

extern "C" void cos_switch_thread_role(int role);  // switch/native/source/thread_wrap.c
constexpr int kThreadRoleCompile = 5;              // COS_SWITCH_THREAD_COMPILE (cos_switch.h)

#ifndef COS_SWITCH_DATA_DIR
#error "COS_SWITCH_DATA_DIR: the native port's directory on the SD card (switch/native/CMakeLists.txt)"
#endif

namespace swdk {
namespace {

constexpr const char* kLocalPath = COS_SWITCH_DATA_DIR "/user/cache/dksh_local.bin";
constexpr const char* kFailDir = COS_SWITCH_DATA_DIR "/user/cache";
constexpr size_t kWorkerStack = 8u << 20;  // Mesa's GLSL parser and nv50_ir recurse deeply (uam_api.h)
constexpr int kFailDumps = 16;

int stage_index(ShaderStage s) { return s == ShaderStage::Vertex ? 0 : 1; }

// ---- the registry
std::mutex g_mutex;
std::unordered_map<uint64_t, std::unique_ptr<ShaderEntry>> g_entries[2];
std::unordered_map<uint64_t, ModuleHashes> g_modules;

// ---- the worker
struct Job {
    ShaderEntry* entry;
    std::string glsl;
};
std::mutex g_jobMutex;
std::condition_variable g_jobCv;   // a job queued
std::condition_variable g_doneCv;  // an entry left Queued
std::deque<Job> g_foreground, g_background;
bool g_workerUp = false;
uint64_t g_compilerId = 0;

// the session's figures (g_jobMutex)
uint64_t g_compiled = 0, g_failed = 0, g_compileNs = 0, g_compileMaxNs = 0, g_pending = 0;
std::atomic<uint64_t> g_loaded{0}, g_loadsSession{0}, g_skipped{0};
int g_failDumps = 0;

// ---- the load budget (render worker)
uint32_t g_budget = 64;
uint32_t g_loadsThisFrame = 0;

const char* stage_name(ShaderStage s) { return s == ShaderStage::Vertex ? "vertex" : "fragment"; }

void append_local(const ShaderEntry& e, const std::vector<uint8_t>& dksh) {
    // the header once (a file of another compiler is replaced at start), then the record
    std::vector<uint8_t> bytes;
    DkshShaderRecord r;
    r.stage = e.stage;
    r.glslHash = e.glslHash;
    r.dksh = dksh;
    append_record(&bytes, r);
    FILE* f = fopen(kLocalPath, "ab");
    if (!f) {
        static int logged = 0;
        if (logged++ < 3) dklog("shaders: cannot append to %s", kLocalPath);
        return;
    }
    fwrite(bytes.data(), 1, bytes.size(), f);
    fclose(f);
}

void dump_failure(const ShaderEntry& e, const std::string& glsl, const std::string& log) {
    if (g_failDumps >= kFailDumps) return;
    g_failDumps++;
    char path[256];
    snprintf(path, sizeof path, "%s/shaderfail_%016llx_%s.glsl", kFailDir, (unsigned long long)e.glslHash,
             e.stage == ShaderStage::Vertex ? "vs" : "fs");
    if (FILE* f = fopen(path, "wb")) {
        fwrite(glsl.data(), 1, glsl.size(), f);
        fprintf(f, "\n/* uam:\n%s\n*/\n", log.c_str());
        fclose(f);
    }
}

// the next job: foreground first, then the one whose draws were skipped most, then background
bool take_job(Job* out) {
    for (std::deque<Job>* q : {&g_foreground, &g_background}) {
        if (q->empty()) continue;
        auto best = q->begin();
        for (auto it = q->begin(); it != q->end(); ++it)
            if (it->entry->skippedDraws.load(std::memory_order_relaxed) >
                best->entry->skippedDraws.load(std::memory_order_relaxed))
                best = it;
        *out = std::move(*best);
        q->erase(best);
        return true;
    }
    return false;
}

void* worker_main(void*) {
    cos_switch_thread_role(kThreadRoleCompile);
    svcSetThreadPriority(CUR_THREAD_HANDLE, kMaxThreadPriority);
    const uint64_t t0 = now_ns();
    uam::init(true);
    static const char kTest[] = "#version 460\nvoid main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";
    const uam::Result test = uam::compile(uam::Stage::Vertex, kTest);
    dklog("shaders: uam worker started (priority 0x%X, %zu MiB stack); self-test %s (%zu bytes) in %.1f ms",
          kMaxThreadPriority, kWorkerStack >> 20, test.ok ? "compiled" : "FAILED", test.dksh.size(),
          double(now_ns() - t0) / 1e6);
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(g_jobMutex);
            g_jobCv.wait(lock, [] { return !g_foreground.empty() || !g_background.empty(); });
            take_job(&job);
        }
        ShaderEntry* e = job.entry;
        const uint64_t c0 = now_ns();
        uam::Result res = uam::compile(e->stage == ShaderStage::Vertex ? uam::Stage::Vertex : uam::Stage::Fragment,
                                       job.glsl.c_str());
        const uint64_t ns = now_ns() - c0;
        const bool ok = res.ok && !res.dksh.empty();
        if (ok) {
            append_local(*e, res.dksh);
        } else {
            dklog("shaders: uam refused %s shader %016llx (%.0f ms): %s", stage_name(e->stage),
                  (unsigned long long)e->glslHash, double(ns) / 1e6, res.log.substr(0, 400).c_str());
            dump_failure(*e, job.glsl, res.log);
        }
        {
            std::lock_guard<std::mutex> lock(g_jobMutex);
            if (ok) {
                e->dksh = std::move(res.dksh);
                g_compiled++;
            } else {
                g_failed++;
            }
            g_compileNs += ns;
            g_compileMaxNs = std::max(g_compileMaxNs, ns);
            g_pending--;
            e->state.store(ok ? ShaderState::Compiled : ShaderState::Failed, std::memory_order_release);
        }
        g_doneCv.notify_all();
    }
    return nullptr;
}

void start_worker() {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, kWorkerStack);
    pthread_t thread;
    const int rc = pthread_create(&thread, &attr, worker_main, nullptr);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        dklog("shaders: cannot start the uam worker (%d): pipelines the caches lack will never draw", rc);
        return;
    }
    pthread_detach(thread);
    g_workerUp = true;
}

// dksh_local.bin: its records into the registry (loaded into code memory as the initial cache's)
void read_local(uint32_t gxConfigVersion) {
    FILE* f = fopen(kLocalPath, "rb");
    const uint64_t expected = dksh_compiler_id(gxConfigVersion);
    if (f) {
        fseek(f, 0, SEEK_END);
        const long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::vector<uint8_t> data(size > 0 ? size_t(size) : 0);
        const bool read = size > 0 && fread(data.data(), 1, data.size(), f) == data.size();
        fclose(f);
        DkshFile file;
        std::string error;
        if (read && read_dksh_file(data, &file, &error) && file.compilerId == expected) {
            size_t loaded = 0, known = 0;
            for (const DkshShaderRecord& r : file.shaders) {
                if (r.dksh.empty()) continue;
                ShaderEntry* e = shader_entry(r.stage, r.glslHash);
                if (e->state.load() == ShaderState::Ready) {
                    known++;
                    continue;
                }
                if (code_load(e->shader, r.dksh.data(), uint32_t(r.dksh.size()), "local DKSH")) {
                    e->state.store(ShaderState::Ready);
                    loaded++;
                }
            }
            dklog("shaders: %s: %zu shaders compiled on this console loaded (%zu already in the initial cache, %zu "
                  "bad records)",
                  kLocalPath, loaded, known, file.badCrc);
            g_loaded += loaded;
            return;
        }
        dklog("shaders: %s %s: started again", kLocalPath,
              read && error.empty() ? "is from another compiler (uam, Tint options or GX config version)"
                                    : "is unreadable");
    }
    // a new file: the header alone
    if (FILE* w = fopen(kLocalPath, "wb")) {
        const std::vector<uint8_t> header = dksh_file_header(expected);
        fwrite(header.data(), 1, header.size(), w);
        fclose(w);
    }
}

}  // namespace

ShaderEntry* shader_entry(ShaderStage stage, uint64_t glslHash) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto& slot = g_entries[stage_index(stage)][glslHash];
    if (!slot) {
        slot = std::make_unique<ShaderEntry>();
        slot->stage = stage;
        slot->glslHash = glslHash;
    }
    return slot.get();
}

ModuleHashes shader_module(uint64_t moduleHash) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_modules.find(moduleHash);
    return it == g_modules.end() ? ModuleHashes{} : it->second;
}

void shader_module_add(uint64_t moduleHash, uint64_t vertexGlslHash, uint64_t fragmentGlslHash) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_modules[moduleHash] = {vertexGlslHash, fragmentGlslHash};
}

bool shader_compile(ShaderEntry* e, const std::string& glsl, bool foreground) {
    if (!g_workerUp) return false;
    std::lock_guard<std::mutex> lock(g_jobMutex);
    ShaderState expected = ShaderState::Missing;
    if (!e->state.compare_exchange_strong(expected, ShaderState::Queued)) {
        // already queued as background: a foreground request moves it up
        if (foreground && expected == ShaderState::Queued) {
            for (auto it = g_background.begin(); it != g_background.end(); ++it) {
                if (it->entry == e) {
                    g_foreground.push_back(std::move(*it));
                    g_background.erase(it);
                    break;
                }
            }
        }
        return false;
    }
    (foreground ? g_foreground : g_background).push_back({e, glsl});
    g_pending++;
    g_jobCv.notify_one();
    return true;
}

bool shader_wait(ShaderEntry* e) {
    std::unique_lock<std::mutex> lock(g_jobMutex);
    g_doneCv.wait(lock, [e] { return e->state.load(std::memory_order_acquire) != ShaderState::Queued; });
    return e->state.load(std::memory_order_acquire) != ShaderState::Failed;
}

bool shader_usable(ShaderEntry* e) {
    const ShaderState s = e->state.load(std::memory_order_acquire);
    if (s == ShaderState::Ready) return true;
    if (s == ShaderState::Compiled && (g_budget == 0 || g_loadsThisFrame < g_budget)) {
        std::vector<uint8_t> dksh;
        {
            std::lock_guard<std::mutex> lock(g_jobMutex);
            dksh.swap(e->dksh);
        }
        g_loadsThisFrame++;
        char name[48];
        snprintf(name, sizeof name, "%s %016llx", stage_name(e->stage), (unsigned long long)e->glslHash);
        if (!dksh.empty() && code_load(e->shader, dksh.data(), uint32_t(dksh.size()), name)) {
            e->state.store(ShaderState::Ready, std::memory_order_release);
            g_loaded++;
            g_loadsSession++;
            return true;
        }
        e->state.store(ShaderState::Failed, std::memory_order_release);
        return false;
    }
    e->skippedDraws.fetch_add(1, std::memory_order_relaxed);
    g_skipped++;
    return false;
}

void shaders_frame_start() { g_loadsThisFrame = 0; }

void shaders_init(uint32_t gxConfigVersion) {
    g_compilerId = dksh_compiler_id(gxConfigVersion);
    g_budget = uint32_t(std::max(0L, env_long("COS_DK_SHADER_BUDGET", 64)));
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        uint64_t ready = 0;
        for (auto& map : g_entries)
            for (auto& [hash, e] : map) ready += e->state.load() == ShaderState::Ready;
        g_loaded = ready;
    }
    read_local(gxConfigVersion);
    translate_init();
    start_worker();
    dklog("shaders: %llu loaded; misses compiled by the uam worker and appended to %s; load budget %u per frame "
          "(COS_DK_SHADER_BUDGET, 0 = no limit); a draw whose shader is not ready is skipped",
          (unsigned long long)g_loaded.load(), kLocalPath, g_budget);
}

ShaderTotals shader_totals() {
    ShaderTotals t;
    std::lock_guard<std::mutex> lock(g_jobMutex);
    t.loaded = g_loaded.load();
    t.pending = g_pending;
    t.failed = g_failed;
    t.compiled = g_compiled;
    t.compileNs = g_compileNs;
    t.compileMaxNs = g_compileMaxNs;
    t.skippedDraws = g_skipped.load();
    t.loadsThisSession = g_loadsSession.load();
    return t;
}

int shaders_report(char* out, size_t size) {
    static ShaderTotals last;
    const ShaderTotals t = shader_totals();
    const uint64_t compiled = t.compiled - last.compiled;
    const uint64_t ns = t.compileNs - last.compileNs;
    const int n = snprintf(out, size,
                           "[cos] shaders: %llu loaded (%llu this session), %llu pending, %llu failed; window: %llu "
                           "compiled (mean %.0f ms, session max %.0f ms), %llu draws skipped; code %u KiB of %u\n",
                           (unsigned long long)t.loaded, (unsigned long long)t.loadsThisSession,
                           (unsigned long long)t.pending, (unsigned long long)t.failed, (unsigned long long)compiled,
                           compiled ? double(ns) / double(compiled) / 1e6 : 0.0, double(t.compileMaxNs) / 1e6,
                           (unsigned long long)(t.skippedDraws - last.skippedDraws), code_used() >> 10, kCodeSize >> 10);
    last = t;
    return n > 0 ? n : 0;
}

}  // namespace swdk

// the harness's perf window (native/src/pc/runtime/pc_frame.cpp via cos_switch_shader_cache_report)
extern "C" int aurora_switch_dk_shaders_report(char* out, size_t size) { return swdk::shaders_report(out, size); }
