// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Adapted from SwitchWakerHD (https://github.com/centollOS/SwitchWakerHD) at df8fbde:
// runtime/src/gfx/deko/shader_files.cpp.
#include "dksh_file.h"

#include <zlib.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <set>
#include <utility>

#define XXH_STATIC_LINKING_ONLY
#include <xxhash.h>

namespace swdk {
namespace {

constexpr uint8_t kKindShader = 1, kKindModule = 2, kKindNamed = 3;
constexpr size_t kRecordHeaderSize = 9;  // kind, payload size, crc

template <class T> void put(std::vector<uint8_t>* out, const T& v) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
    out->insert(out->end(), p, p + sizeof v);
}
template <class T> T get(const uint8_t* p) {
    T v;
    memcpy(&v, p, sizeof v);
    return v;
}
uint64_t fnv(const char* s, uint64_t h = 0xCBF29CE484222325ull) {
    for (; *s; ++s) h = (h ^ uint8_t(*s)) * 0x100000001B3ull;
    return h;
}

void finish(std::vector<uint8_t>* out, uint8_t kind, const std::vector<uint8_t>& payload) {
    uLong crc = crc32(0, &kind, 1);
    crc = crc32(crc, payload.data(), uInt(payload.size()));
    put(out, kind);
    put(out, uint32_t(payload.size()));
    put(out, uint32_t(crc));
    out->insert(out->end(), payload.begin(), payload.end());
}

}  // namespace

uint64_t dksh_compiler_id(uint32_t gxConfigVersion) {
    const std::string tail = "|translate " + std::to_string(kTranslateRevision) + "|gx config " +
                             std::to_string(gxConfigVersion) + "|SWDK " + std::to_string(kDkshFileVersion);
    return fnv(tail.c_str(), fnv(kDkshCompilerName));
}

std::vector<uint8_t> dksh_file_header(uint64_t compilerId) {
    std::vector<uint8_t> h;
    h.insert(h.end(), {'S', 'W', 'D', 'K'});
    put(&h, kDkshFileVersion);
    put(&h, compilerId);
    return h;
}

void append_record(std::vector<uint8_t>* out, const DkshShaderRecord& r) {
    std::vector<uint8_t> p;
    put(&p, uint8_t(r.stage));
    put(&p, r.glslHash);
    p.insert(p.end(), r.dksh.begin(), r.dksh.end());
    finish(out, kKindShader, p);
}

void append_record(std::vector<uint8_t>* out, const DkshModuleRecord& r) {
    std::vector<uint8_t> p;
    put(&p, r.moduleHash);
    put(&p, r.vertexGlslHash);
    put(&p, r.fragmentGlslHash);
    finish(out, kKindModule, p);
}

void append_record(std::vector<uint8_t>* out, const DkshNamedRecord& r) {
    std::vector<uint8_t> p;
    put(&p, uint8_t(r.stage));
    put(&p, uint8_t(r.name.size()));
    p.insert(p.end(), r.name.begin(), r.name.begin() + std::min<size_t>(r.name.size(), 255));
    put(&p, r.glslHash);
    put(&p, uint8_t(r.bindings.size()));
    for (const SlotBinding& b : r.bindings) {
        put(&p, uint8_t(b.kind));
        put(&p, b.slot);
        put(&p, uint8_t(b.group));
        put(&p, uint8_t(b.binding));
    }
    finish(out, kKindNamed, p);
}

bool read_dksh_file(const std::vector<uint8_t>& data, DkshFile* out, std::string* error) {
    if (data.size() < kDkshFileHeaderSize || memcmp(data.data(), "SWDK", 4) != 0) {
        *error = "not a DKSH cache file (no SWDK magic)";
        return false;
    }
    const uint8_t* d = data.data();
    if (get<uint32_t>(d + 4) != kDkshFileVersion) {
        *error = "SWDK version " + std::to_string(get<uint32_t>(d + 4)) + ", this build reads " +
                 std::to_string(kDkshFileVersion);
        return false;
    }
    out->compilerId = get<uint64_t>(d + 8);
    size_t i = kDkshFileHeaderSize;
    while (i + kRecordHeaderSize <= data.size()) {
        const uint8_t kind = d[i];
        const uint32_t size = get<uint32_t>(d + i + 1), crc = get<uint32_t>(d + i + 5);
        if (i + kRecordHeaderSize + size > data.size()) break;  // cut short
        const uint8_t* p = d + i + kRecordHeaderSize;
        i += kRecordHeaderSize + size;
        uLong c = crc32(0, &kind, 1);
        c = crc32(c, p, size);
        if (uint32_t(c) != crc) {
            out->badCrc++;
            continue;
        }
        if (kind == kKindShader && size >= 9) {
            DkshShaderRecord r;
            r.stage = ShaderStage(p[0]);
            r.glslHash = get<uint64_t>(p + 1);
            r.dksh.assign(p + 9, p + size);
            out->shaders.push_back(std::move(r));
        } else if (kind == kKindModule && size == 24) {
            out->modules.push_back({get<uint64_t>(p), get<uint64_t>(p + 8), get<uint64_t>(p + 16)});
        } else if (kind == kKindNamed && size >= 2) {
            DkshNamedRecord r;
            r.stage = ShaderStage(p[0]);
            const size_t n = p[1];
            if (size < 2 + n + 9) continue;
            r.name.assign(reinterpret_cast<const char*>(p + 2), n);
            r.glslHash = get<uint64_t>(p + 2 + n);
            const size_t count = p[2 + n + 8];
            if (size != 2 + n + 9 + 4 * count) continue;
            const uint8_t* b = p + 2 + n + 9;
            for (size_t k = 0; k < count; k++, b += 4)
                r.bindings.push_back({SlotBinding::Kind(b[0]), b[1], b[2], b[3]});
            out->named.push_back(std::move(r));
        }
        // unknown kinds: skipped (a later version's additions)
    }
    return true;
}

DkshFileStats dksh_file_stats(const DkshFile& file, uint64_t expectedCompilerId) {
    DkshFileStats s;
    s.idMatches = file.compilerId == expectedCompilerId;
    s.badCrc = file.badCrc;
    std::set<std::pair<uint8_t, uint64_t>> compiled;
    for (const DkshShaderRecord& r : file.shaders) {
        if (r.dksh.empty()) {
            s.failed++;
            continue;
        }
        (r.stage == ShaderStage::Vertex ? s.vertex : s.fragment)++;
        s.dkshBytes += r.dksh.size();
        compiled.insert({uint8_t(r.stage), r.glslHash});
    }
    s.modules = file.modules.size();
    for (const DkshModuleRecord& m : file.modules) {
        s.modulesComplete += compiled.count({uint8_t(ShaderStage::Vertex), m.vertexGlslHash}) &&
                             compiled.count({uint8_t(ShaderStage::Fragment), m.fragmentGlslHash});
    }
    s.named = file.named.size();
    for (const DkshNamedRecord& n : file.named) s.namedComplete += compiled.count({uint8_t(n.stage), n.glslHash});
    return s;
}

std::string dksh_file_summary(const DkshFileStats& s) {
    char line[320];
    snprintf(line, sizeof line,
             "%zu shaders (%zu vertex, %zu fragment, %zu failed), %.2f MiB of DKSH; %zu / %zu modules and %zu / %zu "
             "named shaders complete; compiler id %s; %zu bad CRC",
             s.vertex + s.fragment + s.failed, s.vertex, s.fragment, s.failed, double(s.dkshBytes) / 1048576.0,
             s.modulesComplete, s.modules, s.namedComplete, s.named, s.idMatches ? "matches" : "DIFFERS (stale file)",
             s.badCrc);
    return line;
}

uint64_t glsl_hash(const std::string& glsl) { return XXH3_64bits(glsl.data(), glsl.size()); }

}  // namespace swdk
