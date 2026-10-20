// The randomizer's archive-load dispatcher (pc_rando_resources.h, docs/RANDOMIZER_PLAN.md step R0.10).
// Pure: no game headers, no game calls (cos_rando_core links it into the unit tests). The game side,
// which allocates on the archive heap and calls pc_rando_res_dispatch from dRes_info_c::loadResource,
// is game_hooks/pc_rando_hooks_resources.cpp.
#include "pc_rando_resources.h"

#include "pc/pc_rando_hooks.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

constexpr int kMaxPatchers = 64;
constexpr int kMaxDirs = 256; // dRes_control_c has 64 object + 64 stage slots

struct Patcher {
    PcRandoResPatcher fn;
    uint32_t type;
};

Patcher sPatchers[kMaxPatchers];
int sPatcherCount = 0;

struct DirEntry {
    const void* info;
    char dir[PCR_RES_DIR_MAX];
};

DirEntry sDirs[kMaxDirs];
int sDirNext = 0; // next slot to overwrite once the table is full

PcRandoResStats sStats;

void logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logf(const char* fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        (void)!write(STDERR_FILENO, buf, (size_t)n < sizeof buf ? (size_t)n : sizeof buf - 1);
    }
}

void copyStr(char* dst, size_t cap, const char* src) {
    if (src == nullptr) {
        src = "";
    }
    const size_t n = strnlen(src, cap - 1); // mArchiveName may fill its 14 bytes without a NUL
    memcpy(dst, src, n);
    dst[n] = '\0';
}

bool isDzx(uint32_t type) {
    return type == PCR_RES_DZS || type == PCR_RES_DZR;
}

constexpr uint32_t kDzxFilter = 0xFFFFFFFFu; // internal: DZS or DZR

bool takes(const Patcher& p, uint32_t type) {
    if (p.type == PCR_RES_ANY) {
        return true;
    }
    if (p.type == kDzxFilter) {
        return isDzx(type);
    }
    return p.type == type;
}

} // namespace

extern "C" {

int pc_rando_register_res_patcher(PcRandoResPatcher fn, uint32_t type) {
    if (fn == nullptr) {
        return 0;
    }
    for (int i = 0; i < sPatcherCount; i++) {
        if (sPatchers[i].fn == fn) {
            return 1;
        }
    }
    if (sPatcherCount >= kMaxPatchers) {
        logf("[cos] rando: res patcher list full (%d)\n", kMaxPatchers);
        return 0;
    }
    sPatchers[sPatcherCount++] = Patcher{fn, type};
    return 1;
}

int pc_rando_register_dzx_patcher(PcRandoResPatcher fn) {
    return pc_rando_register_res_patcher(fn, kDzxFilter);
}

int pc_rando_unregister_res_patcher(PcRandoResPatcher fn) {
    for (int i = 0; i < sPatcherCount; i++) {
        if (sPatchers[i].fn == fn) {
            memmove(&sPatchers[i], &sPatchers[i + 1], (size_t)(sPatcherCount - i - 1) * sizeof sPatchers[0]);
            sPatcherCount--;
            return 1;
        }
    }
    return 0;
}

int pc_rando_res_patcher_count(void) {
    return sPatcherCount;
}

void pc_rando_res_describe(PcRandoResFile* file, PcRandoResNames* store, const char* arcName,
                           const char* dir) {
    copyStr(store->dir, sizeof store->dir, dir);
    copyStr(store->arc, sizeof store->arc, arcName);
    store->stage[0] = '\0';
    static const char kStagePrefix[] = "/res/Stage/";
    const size_t pre = sizeof kStagePrefix - 1;
    if (strncmp(store->dir, kStagePrefix, pre) == 0) {
        const char* s = store->dir + pre;
        const char* end = strchr(s, '/');
        size_t n = end != nullptr ? (size_t)(end - s) : strlen(s);
        if (n >= sizeof store->stage) {
            n = sizeof store->stage - 1;
        }
        memcpy(store->stage, s, n);
        store->stage[n] = '\0';
    }
    int room = -1;
    if (strncmp(store->arc, "Room", 4) == 0 && store->arc[4] != '\0') {
        int v = 0;
        const char* p = store->arc + 4;
        for (; *p >= '0' && *p <= '9'; p++) {
            v = v * 10 + (*p - '0');
        }
        if (*p == '\0') {
            room = v;
        }
    }
    file->dir = store->dir;
    file->stage = store->stage;
    file->arc = store->arc;
    file->room = room;
}

void* pc_rando_res_dispatch(PcRandoResFile* file) {
    sStats.dispatched++;
    file->replaced = 0;
    bool called = false;
    for (int i = 0; i < sPatcherCount; i++) {
        if (!takes(sPatchers[i], file->type)) {
            continue;
        }
        called = true;
        sPatchers[i].fn(file);
    }
    if (called) {
        sStats.patched++;
    }
    if (file->replaced) {
        sStats.replaced++;
    }
    return file->data;
}

uint8_t* pc_rando_res_replace(PcRandoResFile* file, size_t newSize) {
    if (file->alloc == nullptr || newSize == 0) {
        sStats.failed++;
        logf("[cos] rando: res %s/%s/%s: cannot replace (no allocator)\n", file->stage, file->arc,
             file->name);
        return nullptr;
    }
    uint8_t* p = (uint8_t*)file->alloc(newSize, file->allocCtx);
    if (p == nullptr) {
        sStats.failed++;
        logf("[cos] rando: res %s/%s/%s: out of memory for a %zu-byte copy; left unchanged\n",
             file->stage, file->arc, file->name, newSize);
        return nullptr;
    }
    const size_t keep = file->size < newSize ? file->size : newSize;
    memcpy(p, file->data, keep);
    if (newSize > keep) {
        memset(p + keep, 0, newSize - keep);
    }
    file->data = p;
    file->size = newSize;
    file->replaced = 1;
    return p;
}

int pc_rando_res_grow_dzx(PcRandoResFile* file, const PcRandoDzxAdd* adds, int nAdds,
                          uint32_t* firstIndex) {
    int result = PCR_DZX_OK;
    const size_t size = pc_rando_dzx_grown_size(file->data, file->size, adds, nAdds, &result);
    if (size == 0 || result != PCR_DZX_OK) {
        logf("[cos] rando: res %s/%s/%s: grow refused (%s)\n", file->stage, file->arc, file->name,
             pc_rando_dzx_result_name(result));
        return result != PCR_DZX_OK ? result : PCR_DZX_BAD_FILE;
    }
    if (file->alloc == nullptr) {
        sStats.failed++;
        logf("[cos] rando: res %s/%s/%s: cannot grow (no allocator)\n", file->stage, file->arc, file->name);
        return PCR_DZX_NO_SPACE;
    }
    uint8_t* dst = (uint8_t*)file->alloc(size, file->allocCtx);
    if (dst == nullptr) {
        sStats.failed++;
        logf("[cos] rando: res %s/%s/%s: out of memory for a %zu-byte grown copy; left unchanged\n",
             file->stage, file->arc, file->name, size);
        return PCR_DZX_NO_SPACE;
    }
    size_t outSize = 0;
    result = pc_rando_dzx_grow(file->data, file->size, adds, nAdds, dst, size, &outSize, firstIndex);
    if (result != PCR_DZX_OK) {
        // The copy stays allocated on the archive heap (a solid heap cannot free) but is unused.
        logf("[cos] rando: res %s/%s/%s: grow failed (%s)\n", file->stage, file->arc, file->name,
             pc_rando_dzx_result_name(result));
        return result;
    }
    logf("[cos] rando: res %s/%s/%s: grown 0x%zx -> 0x%zx\n", file->stage, file->arc, file->name,
         file->size, outSize);
    file->data = dst;
    file->size = outSize;
    file->replaced = 1;
    return PCR_DZX_OK;
}

void pc_rando_res_note_dir(const void* info, const char* dir) {
    if (info == nullptr) {
        return;
    }
    int slot = -1;
    for (int i = 0; i < kMaxDirs; i++) {
        if (sDirs[i].info == info) {
            slot = i;
            break;
        }
        if (slot < 0 && sDirs[i].info == nullptr) {
            slot = i;
        }
    }
    if (slot < 0) {
        slot = sDirNext;
        sDirNext = (sDirNext + 1) % kMaxDirs;
    }
    sDirs[slot].info = info;
    copyStr(sDirs[slot].dir, sizeof sDirs[slot].dir, dir);
}

const char* pc_rando_res_dir_of(const void* info) {
    if (info == nullptr) {
        return nullptr;
    }
    for (int i = 0; i < kMaxDirs; i++) {
        if (sDirs[i].info == info) {
            return sDirs[i].dir;
        }
    }
    return nullptr;
}

void pc_rando_res_clear_dirs(void) {
    memset(sDirs, 0, sizeof sDirs);
    sDirNext = 0;
}

void pc_rando_res_stats(PcRandoResStats* out) {
    *out = sStats;
}

void pc_rando_res_hook_set(const void* info, const char* arcPath) {
    pc_rando_res_note_dir(info, arcPath);
}

} // extern "C"
