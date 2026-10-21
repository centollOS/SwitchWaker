// The randomizer's seed generation in the background (docs/RANDOMIZER_PLAN.md step R4.3; API and
// behaviour in pc_rando_genjob.h). Pure C++: cos_rando_core links it (unit test test_R4_3.cpp).
#include "pc_rando_genjob.h"

#include "pc_rando_slot.h"
#include "pc_rando_spoiler.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <pthread.h>
#include <sys/stat.h>
#include <utility>

namespace pcr {

namespace {

// Shared by the game thread and the worker. Allocated once and never freed, so a worker still
// running when the program's static destructors run never touches a destroyed object.
struct Shared {
    std::mutex mu;
    GenJobState state = GenJobState::Idle;
    std::string text;
    int done = 0;
    int total = 0;
    GenRequest req;
    GenJobPaths paths;
    GenJobResult result;
    GenJobThreadHooks hooks;
};

Shared& shared() {
    static Shared* s = new Shared();
    return *s;
}

void onProgress(void* user, const char* text, int done, int total) {
    Shared* s = static_cast<Shared*>(user);
    std::lock_guard<std::mutex> lock(s->mu);
    s->text = text != nullptr ? text : "";
    s->done = done;
    s->total = total;
}

bool writeText(const std::string& path, const std::string& text) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) {
        return false;
    }
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    return std::fclose(f) == 0 && ok;
}

void* workerMain(void* arg) {
    Shared* s = static_cast<Shared*>(arg);
    GenJobThreadHooks hooks;
    GenRequest req;
    GenJobPaths paths;
    {
        std::lock_guard<std::mutex> lock(s->mu);
        hooks = s->hooks;
    }
    if (hooks.enter != nullptr) {
        hooks.enter();
    }
    {
        std::lock_guard<std::mutex> lock(s->mu);
        req = s->req;
        paths = s->paths;
    }
    req.progress = onProgress;
    req.progressUser = s;
    GenJobResult res;
    res.version = req.version;
    const auto t0 = std::chrono::steady_clock::now();
    generate(req, res.gen);
    res.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    genJobWriteFiles(res.gen, req.version, paths, &res);
    {
        std::lock_guard<std::mutex> lock(s->mu);
        s->result = std::move(res);
        s->req = GenRequest();
        s->state = GenJobState::Done;
    }
    // The locals' strings are freed inside the scope too.
    req = GenRequest();
    paths = GenJobPaths();
    if (hooks.exit != nullptr) {
        hooks.exit();
    }
    return nullptr;
}

} // namespace

void genJobSetThreadHooks(const GenJobThreadHooks& hooks) {
    Shared& s = shared();
    std::lock_guard<std::mutex> lock(s.mu);
    s.hooks = hooks;
}

bool genJobStart(const GenRequest& req, const GenJobPaths& paths, std::string* err) {
    Shared& s = shared();
    {
        std::lock_guard<std::mutex> lock(s.mu);
        if (s.state != GenJobState::Idle) {
            if (err != nullptr) {
                *err = s.state == GenJobState::Running ? "a seed is being generated" : "the last result was not taken";
            }
            return false;
        }
        s.state = GenJobState::Running;
        s.text.clear();
        s.done = 0;
        s.total = 0;
        s.req = req;
        s.req.progress = nullptr;
        s.req.progressUser = nullptr;
        s.paths = paths;
        s.result = GenJobResult();
    }
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8u << 20);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t thread;
    const int rc = pthread_create(&thread, &attr, workerMain, &s);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        std::lock_guard<std::mutex> lock(s.mu);
        s.state = GenJobState::Idle;
        s.req = GenRequest();
        if (err != nullptr) {
            char b[64];
            std::snprintf(b, sizeof b, "pthread_create failed (%d)", rc);
            *err = b;
        }
        return false;
    }
    return true;
}

GenJobProgress genJobProgress() {
    Shared& s = shared();
    std::lock_guard<std::mutex> lock(s.mu);
    GenJobProgress p;
    p.state = s.state;
    p.text = s.text;
    p.done = s.done;
    p.total = s.total;
    return p;
}

bool genJobTake(GenJobResult* out) {
    Shared& s = shared();
    std::lock_guard<std::mutex> lock(s.mu);
    if (s.state != GenJobState::Done) {
        return false;
    }
    *out = std::move(s.result);
    s.result = GenJobResult();
    s.state = GenJobState::Idle;
    return true;
}

void genJobWait() {
    for (;;) {
        if (genJobProgress().state != GenJobState::Running) {
            return;
        }
        struct timespec ts = {0, 5 * 1000 * 1000};
        nanosleep(&ts, nullptr);
    }
}

bool genJobMakeDirs(const std::string& dir) {
    if (dir.empty()) {
        return false;
    }
    struct stat st;
    if (stat(dir.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    // Every parent first ("sdmc:/..." keeps its device prefix: a prefix ending in ':' is skipped).
    for (size_t i = 1; i < dir.size(); i++) {
        if (dir[i] != '/' || dir[i - 1] == ':' || dir[i - 1] == '/') {
            continue;
        }
        const std::string part = dir.substr(0, i);
        if (stat(part.c_str(), &st) != 0 && mkdir(part.c_str(), 0755) != 0 && errno != EEXIST) {
            return false;
        }
    }
    if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return stat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void genJobWriteFiles(const GenResult& res, const std::string& version, const GenJobPaths& paths,
                      GenJobResult* out) {
    auto fail = [&](const std::string& what) {
        if (out->fileError.empty()) {
            out->fileError = what;
        }
    };
    const std::string name = res.sanitizedSeed;
    if (!paths.logDir.empty() && !name.empty()) {
        if (!genJobMakeDirs(paths.logDir)) {
            fail(paths.logDir);
        } else {
            const std::string trailer = logTrailer(version.c_str());
            const std::string spoiler = paths.logDir + "/" + logFileName(name, "Spoiler Log");
            const std::string nonSpoiler = paths.logDir + "/" + logFileName(name, "Non-Spoiler Log");
            const std::string error = paths.logDir + "/" + logFileName(name, "Error Log");
            if (res.ok) {
                // A stale error log of an earlier failed run of this seed would look current.
                std::remove(error.c_str());
                if (res.spoilerLog.empty()) {
                    std::remove(spoiler.c_str()); // do_not_generate_spoiler_log
                } else if (writeText(spoiler, res.spoilerLog + trailer)) {
                    out->spoilerPath = spoiler;
                } else {
                    fail(spoiler);
                }
                if (writeText(nonSpoiler, res.nonSpoilerLog + trailer)) {
                    out->nonSpoilerPath = nonSpoiler;
                } else {
                    fail(nonSpoiler);
                }
            } else {
                std::remove(spoiler.c_str());
                std::remove(nonSpoiler.c_str());
                if (!res.errorLog.empty()) {
                    if (writeText(error, res.errorLog + trailer)) {
                        out->errorPath = error;
                    } else {
                        fail(error);
                    }
                }
            }
        }
    }
    if (res.ok && !paths.cardDir.empty()) {
        std::string err;
        if (!genJobMakeDirs(paths.cardDir)) {
            fail(paths.cardDir);
        } else if (pc_rando_slot_write_sidecar(paths.cardDir, res.seed, &err)) {
            out->sidecarPath = paths.cardDir + "/" + pc_rando_seed_sidecar_name(res.seed);
        } else {
            fail(paths.cardDir + "/" + pc_rando_seed_sidecar_name(res.seed) + (err.empty() ? "" : ": " + err));
        }
    }
}

} // namespace pcr
