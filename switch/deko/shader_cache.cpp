// The DKSH cache at start (dk.h; docs/DEKO3D_MIGRATION_PLAN.md section 6): initial_dksh_cache.bin,
// written at build time by native/tools/dksh_cache from native/data/initial_pipeline_cache.db and put
// next to the NRO (COS_SWITCH_NRO_DIR) by scripts/switch/build_native.sh, push.sh and make_sd.sh, is
// read whole, its compiler id checked against this build's (a stale file is ignored as a whole), and
// every compiled shader is copied into the code block and initialized. One "[dk] shader cache:" line
// says what came in and how long it took. Phase 2 draws only the test pattern's shaders from it;
// phase 3 looks Aurora's pipelines up by module hash (the module records, kept here already).
#include "dk.h"

#include "dksh_file.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

#ifndef COS_SWITCH_NRO_DIR
#error "COS_SWITCH_NRO_DIR: the NRO's directory on the SD card (switch/native/CMakeLists.txt)"
#endif

namespace swdk {
namespace {

constexpr const char* kCachePath = COS_SWITCH_NRO_DIR "/initial_dksh_cache.bin";

std::unordered_map<uint64_t, DkShader> g_shaders[2];  // [vertex, fragment] by GLSL hash
struct ModuleShaders {
    uint64_t vertex, fragment;
};
std::unordered_map<uint64_t, ModuleShaders> g_modules;  // Aurora's shader hash -> its GLSL hashes
std::unordered_map<std::string, NamedShader> g_named;
char g_status[160] = "not loaded";

bool read_whole(const char* path, std::vector<uint8_t>* out) {
    FILE* f = fopen(path, "rb");
    if (f == nullptr) return false;
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    bool ok = size > 0;
    if (ok) {
        out->resize(size_t(size));
        ok = fread(out->data(), 1, out->size(), f) == out->size();
    }
    fclose(f);
    return ok;
}

}  // namespace

void shader_cache_init(uint32_t gxConfigVersion) {
    const uint64_t t0 = now_ns();
    std::vector<uint8_t> data;
    if (!read_whole(kCachePath, &data)) {
        dklog("shader cache: no %s: no precompiled shaders (scripts/switch/build_native.sh --renderer deko3d writes it "
              "next to the NRO; push.sh and make_sd.sh copy it)",
              kCachePath);
        snprintf(g_status, sizeof g_status, "no initial_dksh_cache.bin");
        return;
    }
    const uint64_t t1 = now_ns();
    DkshFile file;
    std::string error;
    if (!read_dksh_file(data, &file, &error)) {
        dklog("shader cache: %s: %s; ignored", kCachePath, error.c_str());
        snprintf(g_status, sizeof g_status, "initial_dksh_cache.bin unreadable");
        return;
    }
    const uint64_t expected = dksh_compiler_id(gxConfigVersion);
    const DkshFileStats stats = dksh_file_stats(file, expected);
    if (!stats.idMatches) {
        dklog("shader cache: %s: compiler id 0x%016llx, this build's 0x%016llx (%s, GX config v%u): stale, ignored",
              kCachePath, (unsigned long long)file.compilerId, (unsigned long long)expected, kDkshCompilerName,
              gxConfigVersion);
        snprintf(g_status, sizeof g_status, "initial_dksh_cache.bin stale (compiler id)");
        return;
    }
    size_t loaded = 0, refused = 0;
    const uint32_t codeBefore = code_used();
    for (const DkshShaderRecord& r : file.shaders) {
        if (r.dksh.empty()) continue;
        auto& map = g_shaders[r.stage == ShaderStage::Vertex ? 0 : 1];
        DkShader shader;
        if (map.count(r.glslHash) == 0 && code_load(shader, r.dksh.data(), uint32_t(r.dksh.size()), "cached DKSH")) {
            map.emplace(r.glslHash, shader);
            loaded++;
        } else {
            refused++;
        }
    }
    for (const DkshModuleRecord& m : file.modules) g_modules[m.moduleHash] = {m.vertexGlslHash, m.fragmentGlslHash};
    for (const DkshNamedRecord& n : file.named) {
        auto& map = g_shaders[n.stage == ShaderStage::Vertex ? 0 : 1];
        auto it = map.find(n.glslHash);
        if (it != map.end()) g_named[n.name] = NamedShader{it->second, n.bindings};
    }
    const uint64_t t2 = now_ns();
    dklog("shader cache: %s: %.2f MiB read in %.0f ms; %s; %zu loaded into code memory in %.0f ms (%u KiB, %.0f%% of "
          "the %u MiB block)%s",
          kCachePath, double(data.size()) / 1048576.0, double(t1 - t0) / 1e6, dksh_file_summary(stats).c_str(), loaded,
          double(t2 - t1) / 1e6, (code_used() - codeBefore) >> 10, 100.0 * code_used() / kCodeSize, kCodeSize >> 20,
          refused ? " (some refused: see above)" : "");
    for (const char* name : {"dk_test_pattern.vs_main", "dk_test_pattern.fs_main"}) {
        auto it = g_named.find(name);
        if (it == g_named.end()) {
            dklog("shader cache: no %s (the test pattern is drawn without it)", name);
            continue;
        }
        std::string slots;
        static const char* kinds[] = {"ubo", "ssbo", "sampler", "immediates"};
        for (const SlotBinding& b : it->second.bindings) {
            slots += (slots.empty() ? "" : ", ") + std::string(kinds[int(b.kind) & 3]) + " " + std::to_string(b.slot) +
                     " <- @group(" + std::to_string(b.group) + ") @binding(" + std::to_string(b.binding) + ")";
        }
        dklog("shader cache: %s: %s", name, slots.empty() ? "no bindings" : slots.c_str());
    }
    snprintf(g_status, sizeof g_status, "%zu DKSH loaded in %.0f ms from initial_dksh_cache.bin", loaded,
             double(t2 - t0) / 1e6);
}

const NamedShader* shader_cache_named(const char* name) {
    auto it = g_named.find(name);
    return it == g_named.end() ? nullptr : &it->second;
}

const char* shader_cache_status() { return g_status; }

}  // namespace swdk
