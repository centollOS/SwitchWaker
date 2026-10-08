// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// Adapted from SwitchWakerHD (https://github.com/centollOS/SwitchWakerHD) at df8fbde:
// tools/switch/dksh_cache/dksh_cache.cpp.
// dksh_cache: the offline DKSH cache of the deko3d renderer (docs/DEKO3D_MIGRATION_PLAN.md, phase 1
// and section 6). From SwitchWakerHD's tool: the subcommands, the failure grouping by first error,
// the timing report and the read-back check; new here: Aurora's pipeline database as the input, Tint,
// the sharding over processes, the warnings, the dual-source probe and the fixed shaders.
// Build and run with build.sh (Debian container).
//
//   dksh_cache build <pipeline_cache.db> <out.bin> [--work DIR] [--jobs N] [--report FILE]
//                    [--dual-source-probe N] [--fixed DIR]
//       every GX config of the database -> the shader modules create_pipeline builds (main, alpha
//       prepass, dual source) -> WGSL (Aurora's generator) -> GLSL per stage (Tint + post-pass,
//       switch/deko/shader_translate) -> deduplicated by GLSL hash -> uam in N processes (uam is
//       not reentrant) -> out.bin (switch/deko/dksh_file.h) + a report. DIR (default
//       <out>.work) keeps the GLSL (glsl/<hash>_vs.glsl, _fs.glsl), the modules list and uam's
//       log per shader. Exit status 0 only if every stage of every module compiled.
//       --dual-source-probe N: also builds the first N configs' modules with dual-source blending
//       (DstAlphaMode::DualSource, which no config of the database needs today) and reports whether
//       uam takes them; they are not written to out.bin.
//       --fixed DIR: also every entry point of DIR/*.wgsl (fixed_wgsl.py writes them: clear, copy and
//       palette conversions, present, ImGui) with sequential bindings, as named records
//       <file>.<entry point>, and <out>.fixed.inc, their DKSH as a C table to embed in the NRO.
//   dksh_cache wgsl <out.bin> <file.wgsl>...
//       the same for some WGSL files alone, compiled in this process with uam's log per shader;
//       writes <out>.inc as well
//   dksh_cache dump <file.bin> [dir]
//       the file's records with each DKSH's program header (GPRs, code, constants, scratch); with
//       dir, writes <hash>_vs.dksh / _fs.dksh and modules.tsv there
//   dksh_cache check <file.bin> [name...]
//       reads the file as the deko3d NRO's loader does (switch/deko/shader_cache.cpp: read_dksh_file,
//       the compiler id, dksh_file_stats) and prints the same summary as its "[dk] shader cache:"
//       line; exit status 0 only if the id matches, nothing is bad and every module and named record
//       resolves, and each name given (default: the test pattern's two) is a named record
//   dksh_cache compile-shard <work dir> <index> <count>   (internal: one uam process of build)
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "aurora_host.h"
#include "dksh.h"  // switch/uam/source: the DKSH header layout
#include "dksh_file.h"
#include "shader_translate.h"
#include "uam_api.h"

extern char** environ;

namespace fs = std::filesystem;
using namespace swdk;

namespace {

using Clock = std::chrono::steady_clock;
double ms_since(Clock::time_point t) { return std::chrono::duration<double, std::milli>(Clock::now() - t).count(); }

std::vector<uint8_t> read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), {});
}
std::string read_text(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), {});
}
bool write_file(const fs::path& p, const void* data, size_t size) {
    std::ofstream f(p, std::ios::binary);
    f.write(static_cast<const char*>(data), std::streamsize(size));
    return bool(f);
}
bool write_file(const fs::path& p, const std::string& s) { return write_file(p, s.data(), s.size()); }
bool write_file(const fs::path& p, const std::vector<uint8_t>& v) { return write_file(p, v.data(), v.size()); }

std::string hex(uint64_t v) {
    char b[17];
    snprintf(b, sizeof b, "%016" PRIx64, v);
    return b;
}
const char* stage_suffix(ShaderStage s) { return s == ShaderStage::Vertex ? "vs" : "fs"; }

// "0:123(4): error: ..." -> "error: ...": equal messages on different lines group together
std::string normalize(std::string line) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
    const size_t colon = line.find("): ");
    if (colon != std::string::npos && colon < 16 && isdigit(uint8_t(line[0]))) line = line.substr(colon + 3);
    return line;
}
std::vector<std::string> lines_of(const std::string& log) {
    std::vector<std::string> out;
    std::istringstream in(log);
    std::string line;
    while (std::getline(in, line)) {
        line = normalize(line);
        if (!line.empty()) out.push_back(line);
    }
    return out;
}
std::string first_error(const std::string& log) {
    const auto lines = lines_of(log);
    for (const auto& l : lines)
        if (l.find("error") != std::string::npos) return l;
    return lines.empty() ? "(no output)" : lines.front();
}
std::string first_line(const std::string& s) {
    const size_t nl = s.find('\n');
    return nl == std::string::npos ? s : s.substr(0, nl);
}
// one line, tabs and newlines escaped (the shard tsv files)
std::string escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '\n') o += "\\n";
        else if (c == '\t') o += "\\t";
        else if (c == '\\') o += "\\\\";
        else o += c;
    }
    return o;
}
std::string unescape(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            const char n = s[++i];
            o += n == 'n' ? '\n' : n == 't' ? '\t' : n;
        } else {
            o += s[i];
        }
    }
    return o;
}
double pct(const std::vector<double>& sorted, double p) {
    return sorted.empty() ? 0 : sorted[std::min(sorted.size() - 1, size_t(double(sorted.size()) * p))];
}
std::string timing(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    double sum = 0;
    for (double x : v) sum += x;
    char b[200];
    snprintf(b, sizeof b, "n %zu, mean %.2f ms, p50 %.2f, p90 %.2f, p99 %.2f, max %.2f; sum %.1f s", v.size(),
             v.empty() ? 0 : sum / double(v.size()), pct(v, .5), pct(v, .9), pct(v, .99), v.empty() ? 0 : v.back(),
             sum / 1000);
    return b;
}

// uam's warnings: every line of a successful compile's log
std::vector<std::string> warnings_of(const std::string& log) {
    std::vector<std::string> out;
    for (auto& l : lines_of(log))
        if (l.find("warning") != std::string::npos) out.push_back(l);
    return out;
}

struct Report {
    std::string text;
    void line(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
        char b[2048];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(b, sizeof b, fmt, ap);
        va_end(ap);
        text += b;
        text += '\n';
        fputs(b, stdout);
        fputc('\n', stdout);
    }
};

// ---- compile-shard ---------------------------------------------------------------------------------
// glsl/<hash>_<vs|fs>.glsl with index % count == shard (sorted by name) -> shard<k>.bin (shader
// records) and shard<k>.tsv (name, ok, ms, bytes, log)
int compile_shard(const fs::path& work, int shard, int count) {
    std::vector<fs::path> files;
    for (const auto& e : fs::directory_iterator(work / "glsl"))
        if (e.path().extension() == ".glsl") files.push_back(e.path());
    std::sort(files.begin(), files.end());
    std::vector<uint8_t> bin;
    std::string tsv;
    uam::init(true);
    for (size_t i = 0; i < files.size(); i++) {
        if (int(i % size_t(count)) != shard) continue;
        const std::string name = files[i].stem().string();  // <hash>_<vs|fs>
        const bool vertex = name.size() > 3 && name.compare(name.size() - 3, 3, "_vs") == 0;
        const std::string glsl = read_text(files[i]);
        const auto t0 = Clock::now();
        uam::Result res = uam::compile(vertex ? uam::Stage::Vertex : uam::Stage::Fragment, glsl.c_str());
        const double ms = ms_since(t0);
        const bool ok = res.ok && !res.dksh.empty();
        DkshShaderRecord r;
        r.stage = vertex ? ShaderStage::Vertex : ShaderStage::Fragment;
        r.glslHash = strtoull(name.substr(0, 16).c_str(), nullptr, 16);
        if (ok) r.dksh = std::move(res.dksh);
        append_record(&bin, r);
        char head[96];
        snprintf(head, sizeof head, "%s\t%d\t%.3f\t%zu\t", name.c_str(), ok ? 1 : 0, ms, r.dksh.size());
        tsv += head + escape(res.log) + "\n";
    }
    uam::shutdown();
    const std::string k = std::to_string(shard);
    if (!write_file(work / ("shard" + k + ".bin"), bin) || !write_file(work / ("shard" + k + ".tsv"), tsv)) {
        fprintf(stderr, "shard %d: cannot write in %s\n", shard, work.c_str());
        return 1;
    }
    return 0;
}

std::string self_exe(const char* argv0) {
    std::error_code ec;
    auto p = fs::read_symlink("/proc/self/exe", ec);
    return ec ? std::string(argv0) : p.string();
}

// ---- fixed shaders -----------------------------------------------------------------------------------
// "@vertex fn vs_main(" / "@fragment\nfn fs_opaque(": the module's entry points in source order
std::vector<std::pair<ShaderStage, std::string>> entry_points(const std::string& wgsl) {
    std::vector<std::pair<ShaderStage, std::string>> out;
    for (const char* attr : {"@vertex", "@fragment"}) {
        size_t at = 0;
        while ((at = wgsl.find(attr, at)) != std::string::npos) {
            at += strlen(attr);
            size_t fn = wgsl.find("fn ", at);
            if (fn == std::string::npos) break;
            fn += 3;
            const size_t end = wgsl.find('(', fn);
            if (end == std::string::npos) break;
            out.emplace_back(attr[1] == 'v' ? ShaderStage::Vertex : ShaderStage::Fragment, wgsl.substr(fn, end - fn));
        }
    }
    return out;
}

std::string bindings_text(const std::vector<SlotBinding>& bindings) {
    static const char* kinds[] = {"ubo", "ssbo", "sampler", "immediates"};
    std::string out;
    for (auto& b : bindings)
        out += std::string(out.empty() ? "" : ", ") + kinds[int(b.kind)] + " " + std::to_string(b.slot) +
               " <- @group(" + std::to_string(b.group) + ") @binding(" + std::to_string(b.binding) + ")";
    return out;
}

// The fixed shaders' DKSH as a C table to embed in the NRO: one array per distinct DKSH and
// {name, stage, dksh, size} rows (the bindings are in the cache file's named records and the report).
std::string fixed_table(const std::vector<DkshNamedRecord>& named,
                        const std::map<std::pair<uint8_t, uint64_t>, DkshShaderRecord>& shaders) {
    std::string inc =
        "// Generated by native/tools/dksh_cache: the fixed shaders' DKSH (uam, compiler id " +
        hex(dksh_compiler_id(dksh::gx_config_version())) + "). Do not edit.\n";
    std::set<uint64_t> done;
    for (const auto& n : named) {
        auto it = shaders.find({uint8_t(n.stage), n.glslHash});
        if (it == shaders.end() || it->second.dksh.empty() || !done.insert(n.glslHash).second) continue;
        const auto& d = it->second.dksh;
        inc += "static const unsigned char kDksh_" + hex(n.glslHash) + "[" + std::to_string(d.size()) + "] = {";
        for (size_t i = 0; i < d.size(); i++) inc += (i % 24 ? "" : "\n    ") + std::to_string(d[i]) + ",";
        inc += "\n};\n";
    }
    inc += "struct FixedDksh { const char* name; unsigned char stage; const unsigned char* dksh; unsigned size; };\n";
    inc += "static const FixedDksh kFixedDksh[] = {\n";
    for (const auto& n : named) {
        auto it = shaders.find({uint8_t(n.stage), n.glslHash});
        if (it == shaders.end() || it->second.dksh.empty()) continue;
        inc += "    {\"" + n.name + "\", " + std::to_string(int(n.stage)) + ", kDksh_" + hex(n.glslHash) + ", " +
               std::to_string(it->second.dksh.size()) + "},  // " + bindings_text(n.bindings) + "\n";
    }
    inc += "};\n";
    return inc;
}

// every .wgsl of dir (sorted): each entry point translated with sequential bindings
struct FixedStage {
    DkshNamedRecord named;
    std::string glsl, error;
};
std::vector<FixedStage> translate_fixed(const std::vector<fs::path>& files) {
    std::vector<FixedStage> out;
    for (const auto& in : files) {
        const std::string wgsl = read_text(in);
        for (const auto& [stage, ep] : entry_points(wgsl)) {
            FixedStage f;
            f.named.stage = stage;
            f.named.name = in.stem().string() + "." + ep;
            TranslateResult t = wgsl_to_glsl(wgsl, ep, stage, BindingScheme::Sequential);
            if (t.ok) {
                f.glsl = std::move(t.glsl);
                f.named.glslHash = glsl_hash(f.glsl);
                f.named.bindings = std::move(t.bindings);
            } else {
                f.error = t.error;
            }
            out.push_back(std::move(f));
        }
    }
    return out;
}
std::vector<fs::path> wgsl_files(const fs::path& dir) {
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dir, ec))
        if (e.path().extension() == ".wgsl") files.push_back(e.path());
    std::sort(files.begin(), files.end());
    return files;
}

// ---- build -----------------------------------------------------------------------------------------
struct ModuleEntry {
    uint64_t moduleHash = 0;
    uint8_t mode = 0;
    uint64_t rowHash = 0;  // the first config that needs it
    uint64_t vs = 0, fs = 0;
    std::string vsError, fsError;
};

// --dual-source-probe: the first n configs' modules with dual-source blending, compiled in this process
void dual_source_probe(const std::vector<dksh::GxConfigRow>& rows, size_t n, const fs::path& work, Report* rep) {
    if (n == 0) return;
    std::error_code ec;
    fs::create_directories(work / "dual_source", ec);
    std::set<uint64_t> seen;
    size_t modules = 0, ok = 0;
    std::map<std::string, int> failures;
    uam::init(true);
    for (size_t i = 0; i < rows.size() && modules < n; i++) {
        dksh::GxModule m = dksh::gx_module(rows[i], 2);
        if (!seen.insert(m.moduleHash).second) continue;
        modules++;
        TranslateResult t = wgsl_to_glsl(m.wgsl, "fs_main", ShaderStage::Fragment, BindingScheme::GX);
        if (!t.ok) {
            failures["translate: " + first_line(t.error)]++;
            continue;
        }
        write_file(work / "dual_source" / (hex(m.moduleHash) + "_fs.glsl"), t.glsl);
        uam::Result r = uam::compile(uam::Stage::Fragment, t.glsl.c_str());
        if (r.ok && !r.dksh.empty()) {
            ok++;
            for (auto& w : warnings_of(r.log)) failures["warning: " + w]++;
        } else {
            failures["uam: " + first_error(r.log)]++;
        }
    }
    uam::shutdown();
    rep->line("dual-source probe: %zu / %zu fragment shaders of DualSource modules compiled (GLSL in %s)", ok, modules,
              (work / "dual_source").c_str());
    for (auto& [why, count] : failures) rep->line("  %5d x %s", count, why.c_str());
}

int build(const char* argv0, const std::string& dbPath, const fs::path& outPath, fs::path work, int jobs,
          fs::path reportPath, size_t dualSourceProbe, const fs::path& fixedDir) {
    const auto t0 = Clock::now();
    if (work.empty()) work = outPath.string() + ".work";
    if (reportPath.empty()) reportPath = outPath.string() + ".report.txt";
    std::error_code ec;
    fs::remove_all(work, ec);
    fs::create_directories(work / "glsl", ec);
    Report rep;

    // 1. configs -> modules -> GLSL (one thread: Tint is fast, ~1 ms per stage)
    std::vector<dksh::GxConfigRow> rows;
    std::string err;
    if (!dksh::read_gx_configs(dbPath, &rows, &err)) {
        fprintf(stderr, "%s\n", err.c_str());
        return 1;
    }
    rep.line("dksh_cache build: %s", dbPath.c_str());
    rep.line("compiler id %s (%s, translate rev %u, GX config v%u, SWDK v%u)",
             hex(dksh_compiler_id(dksh::gx_config_version())).c_str(), kDkshCompilerName, kTranslateRevision,
             dksh::gx_config_version(), kDkshFileVersion);
    rep.line("GX configs: %zu%s%s", rows.size(), err.empty() ? "" : "; skipped: ", err.c_str());

    translate_init();
    std::vector<ModuleEntry> modules;
    std::unordered_map<uint64_t, size_t> moduleIndex;
    std::map<uint64_t, std::pair<ShaderStage, std::string>> glsl;  // hash -> stage, text
    std::map<std::string, std::pair<int, std::string>> translateFailures;
    std::map<uint8_t, size_t> modesNeeded;
    std::vector<double> tintMs;
    size_t moduleRefs = 0;
    std::vector<std::vector<uint64_t>> configModules;  // per row: its module hashes
    for (const auto& row : rows) {
        std::vector<uint64_t> mine;
        for (auto& m : dksh::gx_modules(row)) {
            moduleRefs++;
            mine.push_back(m.moduleHash);
            modesNeeded[m.dstAlphaMode]++;
            if (moduleIndex.count(m.moduleHash)) continue;
            ModuleEntry e;
            e.moduleHash = m.moduleHash;
            e.mode = m.dstAlphaMode;
            e.rowHash = row.rowHash;
            write_file(work / "glsl" / (hex(m.moduleHash) + ".wgsl"), m.wgsl);
            for (const ShaderStage stage : {ShaderStage::Vertex, ShaderStage::Fragment}) {
                const auto ts = Clock::now();
                TranslateResult r = wgsl_to_glsl(m.wgsl, stage == ShaderStage::Vertex ? "vs_main" : "fs_main",
                                                 stage, BindingScheme::GX);
                tintMs.push_back(ms_since(ts));
                uint64_t& slot = stage == ShaderStage::Vertex ? e.vs : e.fs;
                if (!r.ok) {
                    (stage == ShaderStage::Vertex ? e.vsError : e.fsError) = r.error;
                    auto& f = translateFailures[std::string(stage_suffix(stage)) + ": " + first_line(r.error)];
                    if (!f.first++) f.second = hex(m.moduleHash);
                    continue;
                }
                slot = glsl_hash(r.glsl);
                if (!glsl.count(slot)) {
                    glsl[slot] = {stage, r.glsl};
                    write_file(work / "glsl" / (hex(slot) + "_" + stage_suffix(stage) + ".glsl"), r.glsl);
                }
            }
            moduleIndex[m.moduleHash] = modules.size();
            modules.push_back(std::move(e));
        }
        configModules.push_back(std::move(mine));
    }
    size_t vsCount = 0;
    for (auto& [h, g] : glsl) vsCount += g.first == ShaderStage::Vertex;
    rep.line("modules: %zu needed by the configs (%zu none, %zu replace, %zu dual-source), %zu distinct",
             moduleRefs, modesNeeded[0], modesNeeded[1], modesNeeded[2], modules.size());
    rep.line("GLSL after Tint + post-pass: %zu distinct (%zu vertex, %zu fragment) from %zu module stages",
             glsl.size(), vsCount, glsl.size() - vsCount, modules.size() * 2);
    rep.line("Tint + post-pass per stage: %s", timing(tintMs).c_str());
    if (translateFailures.empty()) {
        rep.line("translation failures: none");
    } else {
        rep.line("translation failures:");
        for (auto& [why, f] : translateFailures)
            rep.line("  %5d x %s (e.g. module %s)", f.first, why.c_str(), f.second.c_str());
    }
    {
        std::string tsv = "module\tmode\tfirst_config\tvs_glsl\tfs_glsl\n";
        for (auto& m : modules)
            tsv += hex(m.moduleHash) + "\t" + dksh::dst_alpha_mode_name(m.mode) + "\t" + hex(m.rowHash) + "\t" +
                   hex(m.vs) + "\t" + hex(m.fs) + "\n";
        write_file(work / "modules.tsv", tsv);
    }

    // fixed shaders: their GLSL goes to the same shards
    std::vector<FixedStage> fixedStages;
    size_t fixedTranslateFailures = 0;
    if (!fixedDir.empty()) {
        fixedStages = translate_fixed(wgsl_files(fixedDir));
        for (auto& f : fixedStages) {
            if (!f.error.empty()) {
                fixedTranslateFailures++;
                rep.line("  fixed %s: translation failed: %s", f.named.name.c_str(), first_line(f.error).c_str());
                continue;
            }
            const auto key = f.named.glslHash;
            if (!glsl.count(key)) {
                glsl[key] = {f.named.stage, f.glsl};
                write_file(work / "glsl" / (hex(key) + "_" + stage_suffix(f.named.stage) + ".glsl"), f.glsl);
            }
        }
    }

    // 2. uam in jobs processes
    const auto tc = Clock::now();
    const std::string exe = self_exe(argv0);
    std::vector<pid_t> pids;
    for (int k = 0; k < jobs; k++) {
        const std::string ks = std::to_string(k), ns = std::to_string(jobs), ws = work.string();
        const char* args[] = {exe.c_str(), "compile-shard", ws.c_str(), ks.c_str(), ns.c_str(), nullptr};
        pid_t pid;
        if (posix_spawn(&pid, exe.c_str(), nullptr, nullptr, const_cast<char**>(args), environ) != 0) {
            fprintf(stderr, "cannot start %s\n", exe.c_str());
            return 1;
        }
        pids.push_back(pid);
    }
    bool shardsOk = true;
    for (pid_t pid : pids) {
        int status = 0;
        waitpid(pid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            shardsOk = false;
            fprintf(stderr, "a uam shard failed (status %d)\n", status);
        }
    }
    if (!shardsOk) return 1;
    const double compileWall = ms_since(tc);

    // 3. merge: shader records sorted by (stage, hash), then the modules in config order
    std::map<std::pair<uint8_t, uint64_t>, DkshShaderRecord> shaders;
    std::map<std::string, std::pair<int, std::string>> uamFailures;
    std::map<std::string, std::pair<int, std::string>> uamWarnings;
    std::vector<double> uamMs[2];
    size_t withWarnings = 0;
    for (int k = 0; k < jobs; k++) {
        DkshFile part;
        std::vector<uint8_t> data = read_file(work / ("shard" + std::to_string(k) + ".bin"));
        data.insert(data.begin(), kDkshFileHeaderSize, 0);
        memcpy(data.data(), dksh_file_header(0).data(), kDkshFileHeaderSize);
        if (!read_dksh_file(data, &part, &err) || part.badCrc) {
            fprintf(stderr, "shard %d: unreadable\n", k);
            return 1;
        }
        for (auto& r : part.shaders) shaders[{uint8_t(r.stage), r.glslHash}] = std::move(r);
        std::istringstream tsv(read_text(work / ("shard" + std::to_string(k) + ".tsv")));
        std::string line;
        std::string logs;
        while (std::getline(tsv, line)) {
            std::vector<std::string> f;
            size_t a = 0, b;
            while ((b = line.find('\t', a)) != std::string::npos && f.size() < 4) {
                f.push_back(line.substr(a, b - a));
                a = b + 1;
            }
            f.push_back(line.substr(a));
            if (f.size() != 5) continue;
            const bool vertex = f[0].size() > 3 && f[0].compare(f[0].size() - 3, 3, "_vs") == 0;
            const std::string log = unescape(f[4]);
            uamMs[vertex ? 0 : 1].push_back(atof(f[2].c_str()));
            if (f[1] != "1") {
                auto& e = uamFailures[std::string(vertex ? "vs" : "fs") + ": " + first_error(log)];
                if (!e.first++) e.second = f[0];
            } else {
                const auto w = warnings_of(log);
                withWarnings += !w.empty();
                std::set<std::string> seen;
                for (auto& msg : w) {
                    if (!seen.insert(msg).second) continue;
                    auto& e = uamWarnings[std::string(vertex ? "vs" : "fs") + ": " + msg];
                    if (!e.first++) e.second = f[0];
                }
            }
            if (!log.empty()) logs += "== " + f[0] + (f[1] == "1" ? " ok\n" : " FAILED\n") + log + "\n";
        }
        write_file(work / ("uam_log" + std::to_string(k) + ".txt"), logs);
    }
    std::vector<uint8_t> file = dksh_file_header(dksh_compiler_id(dksh::gx_config_version()));
    size_t ok[2] = {}, total[2] = {}, dkshBytes = 0, maxBytes = 0;
    for (auto& [key, r] : shaders) {
        const int s = r.stage == ShaderStage::Vertex ? 0 : 1;
        total[s]++;
        if (!r.dksh.empty()) {
            ok[s]++;
            dkshBytes += r.dksh.size();
            maxBytes = std::max(maxBytes, r.dksh.size());
        }
        append_record(&file, r);
    }
    for (auto& m : modules) append_record(&file, DkshModuleRecord{m.moduleHash, m.vs, m.fs});
    std::vector<DkshNamedRecord> named;
    for (auto& f : fixedStages)
        if (f.error.empty()) named.push_back(f.named);
    for (auto& n : named) append_record(&file, n);
    if (!write_file(outPath, file)) {
        fprintf(stderr, "%s: cannot write\n", outPath.c_str());
        return 1;
    }

    auto compiled = [&](ShaderStage s, uint64_t h) {
        auto it = shaders.find({uint8_t(s), h});
        return h && it != shaders.end() && !it->second.dksh.empty();
    };
    size_t modulesOk = 0, configsOk = 0;
    std::unordered_map<uint64_t, bool> moduleOk;
    for (auto& m : modules) {
        const bool good = compiled(ShaderStage::Vertex, m.vs) && compiled(ShaderStage::Fragment, m.fs);
        moduleOk[m.moduleHash] = good;
        modulesOk += good;
    }
    for (auto& cm : configModules) {
        bool good = true;
        for (uint64_t h : cm) good = good && moduleOk[h];
        configsOk += good;
    }

    rep.line("uam: %zu / %zu vertex, %zu / %zu fragment compiled in %d processes, %.1f s wall", ok[0], total[0],
             ok[1], total[1], jobs, compileWall / 1000);
    rep.line("uam per vertex shader: %s", timing(uamMs[0]).c_str());
    rep.line("uam per fragment shader: %s", timing(uamMs[1]).c_str());
    if (uamFailures.empty()) {
        rep.line("uam failures: none");
    } else {
        rep.line("uam failures (by first error):");
        for (auto& [why, f] : uamFailures) rep.line("  %5d x %s (e.g. %s)", f.first, why.c_str(), f.second.c_str());
    }
    if (uamWarnings.empty()) {
        rep.line("uam warnings: none");
    } else {
        rep.line("uam warnings (%zu shaders with any; distinct messages, shaders with each):", withWarnings);
        for (auto& [why, f] : uamWarnings) rep.line("  %5d x %s (e.g. %s)", f.first, why.c_str(), f.second.c_str());
    }
    rep.line("modules with both stages compiled: %zu / %zu; configs with every module compiled: %zu / %zu", modulesOk,
             modules.size(), configsOk, rows.size());
    size_t fixedOk = 0;
    if (!fixedDir.empty()) {
        for (auto& n : named) fixedOk += compiled(n.stage, n.glslHash);
        fs::path incPath = outPath;
        incPath += ".fixed.inc";
        write_file(incPath, fixed_table(named, shaders));
        rep.line("fixed shaders (%s): %zu / %zu entry points compiled; C table %s", fixedDir.c_str(), fixedOk,
                 fixedStages.size(), incPath.c_str());
        for (auto& n : named)
            rep.line("  %-40s %s  [%s]", n.name.c_str(), compiled(n.stage, n.glslHash) ? "ok" : "FAILED",
                     bindings_text(n.bindings).c_str());
    }
    rep.line("DKSH: %zu bytes (%.2f MiB), mean %zu, max %zu per shader; %s: %zu bytes (%.2f MiB)", dkshBytes,
             double(dkshBytes) / 1048576.0, (ok[0] + ok[1]) ? dkshBytes / (ok[0] + ok[1]) : 0, maxBytes,
             outPath.filename().c_str(), file.size(), double(file.size()) / 1048576.0);

    // read back
    DkshFile back;
    const bool readOk = read_dksh_file(read_file(outPath), &back, &err);
    size_t backOk = 0;
    for (auto& r : back.shaders) backOk += !r.dksh.empty();
    const bool same = readOk && back.badCrc == 0 && back.shaders.size() == shaders.size() &&
                      back.modules.size() == modules.size() && back.named.size() == named.size() &&
                      backOk == ok[0] + ok[1] &&
                      back.compilerId == dksh_compiler_id(dksh::gx_config_version());
    rep.line("read back: %zu shader records (%zu with DKSH), %zu module records, %zu named, %zu bad CRC: %s",
             back.shaders.size(), backOk, back.modules.size(), back.named.size(), back.badCrc, same ? "OK" : "MISMATCH");
    dual_source_probe(rows, dualSourceProbe, work, &rep);
    rep.line("wall %.1f s; work files in %s", ms_since(t0) / 1000, work.c_str());
    write_file(reportPath, rep.text);
    const bool complete = translateFailures.empty() && uamFailures.empty() && configsOk == rows.size() &&
                          fixedTranslateFailures == 0 && fixedOk == named.size();
    return same && complete ? 0 : 2;
}

// ---- wgsl: fixed shaders alone, compiled in this process ------------------------------------------------
int fixed(const fs::path& outPath, const std::vector<fs::path>& inputs) {
    translate_init();
    uam::init(true);
    std::vector<uint8_t> file = dksh_file_header(dksh_compiler_id(dksh::gx_config_version()));
    std::map<std::pair<uint8_t, uint64_t>, DkshShaderRecord> shaders;
    std::vector<DkshNamedRecord> named;
    int failures = 0;
    for (auto& f : translate_fixed(inputs)) {
        if (!f.error.empty()) {
            printf("%-40s translate FAILED: %s\n", f.named.name.c_str(), first_line(f.error).c_str());
            failures++;
            continue;
        }
        write_file(outPath.parent_path() / (f.named.name + ".glsl"), f.glsl);
        const auto t0 = Clock::now();
        uam::Result r = uam::compile(f.named.stage == ShaderStage::Vertex ? uam::Stage::Vertex : uam::Stage::Fragment,
                                     f.glsl.c_str());
        const double ms = ms_since(t0);
        const bool ok = r.ok && !r.dksh.empty();
        printf("%-40s %s %6.1f ms %6zu bytes  [%s]\n", f.named.name.c_str(), ok ? "ok    " : "FAILED", ms,
               r.dksh.size(), bindings_text(f.named.bindings).c_str());
        for (auto& l : lines_of(r.log)) printf("    %s\n", l.c_str());
        if (!ok) {
            failures++;
            continue;
        }
        shaders[{uint8_t(f.named.stage), f.named.glslHash}] = DkshShaderRecord{f.named.stage, f.named.glslHash, r.dksh};
        named.push_back(f.named);
    }
    uam::shutdown();
    for (auto& [k, r] : shaders) append_record(&file, r);
    for (auto& n : named) append_record(&file, n);
    write_file(outPath, file);
    fs::path incPath = outPath;
    incPath += ".inc";
    write_file(incPath, fixed_table(named, shaders));
    printf("%zu named shaders, %zu distinct DKSH, %d failures -> %s, %s\n", named.size(), shaders.size(), failures,
           outPath.c_str(), incPath.c_str());
    return failures ? 2 : 0;
}

// ---- dump --------------------------------------------------------------------------------------------
int dump(const fs::path& in, const fs::path& dir) {
    DkshFile f;
    std::string err;
    if (!read_dksh_file(read_file(in), &f, &err)) {
        fprintf(stderr, "%s: %s\n", in.c_str(), err.c_str());
        return 1;
    }
    printf("%s: compiler id %s (this build %s), %zu shaders, %zu modules, %zu named, %zu bad CRC\n", in.c_str(),
           hex(f.compilerId).c_str(), hex(dksh_compiler_id(dksh::gx_config_version())).c_str(), f.shaders.size(),
           f.modules.size(), f.named.size(), f.badCrc);
    std::error_code ec;
    if (!dir.empty()) fs::create_directories(dir, ec);
    std::map<uint32_t, size_t> gprs[2];
    size_t bad = 0, scratch = 0, failed = 0;
    uint64_t code = 0, cbuf = 0;
    for (const auto& r : f.shaders) {
        if (r.dksh.empty()) {
            failed++;
            continue;
        }
        const int s = r.stage == ShaderStage::Vertex ? 0 : 1;
        DkshHeader h{};
        DkshProgramHeader p{};
        bool sane = r.dksh.size() >= sizeof h;
        if (sane) memcpy(&h, r.dksh.data(), sizeof h);
        sane = sane && h.magic == DKSH_MAGIC && h.num_programs == 1 && h.control_sz % 256 == 0 &&
               h.code_sz % 256 == 0 && h.control_sz + h.code_sz == r.dksh.size() &&
               h.programs_off + sizeof p <= h.control_sz;
        if (sane) {
            memcpy(&p, r.dksh.data() + h.programs_off, sizeof p);
            sane = p.type == (s == 0 ? DkshProgramType_Vertex : DkshProgramType_Fragment) && p.entrypoint < h.code_sz &&
                   p.num_gprs > 0 && p.num_gprs <= 255 && p.constbuf1_off + p.constbuf1_sz <= h.code_sz;
        }
        if (!sane) {
            bad++;
            printf("  BAD header: %s_%s\n", hex(r.glslHash).c_str(), stage_suffix(r.stage));
            continue;
        }
        gprs[s][p.num_gprs]++;
        scratch += p.per_warp_scratch_sz != 0;
        code += h.code_sz;
        cbuf += p.constbuf1_sz;
        if (!dir.empty())
            write_file(dir / (hex(r.glslHash) + "_" + stage_suffix(r.stage) + ".dksh"), r.dksh);
    }
    for (int s = 0; s < 2; s++) {
        std::string hist;
        for (auto& [g, n] : gprs[s]) hist += " " + std::to_string(g) + ":" + std::to_string(n);
        printf("%s GPRs (count:shaders):%s\n", s ? "fragment" : "vertex", hist.c_str());
    }
    printf("headers: %zu sane, %zu bad, %zu failed records; code %" PRIu64 " bytes, constbuf1 %" PRIu64
           " bytes, %zu with scratch memory\n",
           f.shaders.size() - bad - failed, bad, failed, code, cbuf, scratch);
    for (const auto& n : f.named) {
        printf("  named %s glsl %s, %zu bindings\n", n.name.c_str(), hex(n.glslHash).c_str(), n.bindings.size());
    }
    if (!dir.empty()) {
        std::string tsv = "module\tvs_glsl\tfs_glsl\n";
        for (auto& m : f.modules)
            tsv += hex(m.moduleHash) + "\t" + hex(m.vertexGlslHash) + "\t" + hex(m.fragmentGlslHash) + "\n";
        write_file(dir / "modules.tsv", tsv);
    }
    return bad ? 2 : 0;
}

// ---- check: the NRO's view of a file ---------------------------------------------------------------------
int check(const fs::path& in, std::vector<std::string> names) {
    if (names.empty()) names = {"dk_test_pattern.vs_main", "dk_test_pattern.fs_main"};
    DkshFile f;
    std::string err;
    if (!read_dksh_file(read_file(in), &f, &err)) {
        printf("%s: %s\n", in.c_str(), err.c_str());
        return 2;
    }
    const DkshFileStats st = dksh_file_stats(f, dksh_compiler_id(dksh::gx_config_version()));
    printf("%s: %s\n", in.c_str(), dksh_file_summary(st).c_str());
    bool ok = st.idMatches && st.failed == 0 && st.badCrc == 0 && st.modulesComplete == st.modules &&
              st.namedComplete == st.named;
    for (const std::string& name : names) {
        const DkshNamedRecord* found = nullptr;
        for (const auto& n : f.named)
            if (n.name == name) found = &n;
        printf("  %-28s %s%s%s\n", name.c_str(), found ? "present" : "MISSING", found ? ": " : "",
               found ? bindings_text(found->bindings).c_str() : "");
        ok = ok && found;
    }
    printf("check: %s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 2;
}

int usage() {
    fprintf(stderr,
            "usage: dksh_cache build <pipeline_cache.db> <out.bin> [--work DIR] [--jobs N] [--report FILE]\n"
            "                        [--dual-source-probe N] [--fixed DIR]\n"
            "       dksh_cache wgsl <out.bin> <file.wgsl>...\n"
            "       dksh_cache dump <file.bin> [dir]\n"
            "       dksh_cache check <file.bin> [name...]\n");
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) return usage();
    const std::string cmd = argv[1];
    if (cmd == "compile-shard" && argc == 5) return compile_shard(argv[2], atoi(argv[3]), atoi(argv[4]));
    if (cmd == "build" && argc >= 4) {
        fs::path work, report;
        size_t probe = 0;
        fs::path fixedDir;
        int jobs = int(std::max(1L, sysconf(_SC_NPROCESSORS_ONLN)));
        for (int i = 4; i + 1 < argc; i += 2) {
            if (!strcmp(argv[i], "--work")) work = argv[i + 1];
            else if (!strcmp(argv[i], "--jobs")) jobs = std::max(1, atoi(argv[i + 1]));
            else if (!strcmp(argv[i], "--report")) report = argv[i + 1];
            else if (!strcmp(argv[i], "--dual-source-probe")) probe = size_t(atol(argv[i + 1]));
            else if (!strcmp(argv[i], "--fixed")) fixedDir = argv[i + 1];
            else return usage();
        }
        return build(argv[0], argv[2], argv[3], work, jobs, report, probe, fixedDir);
    }
    if (cmd == "wgsl" && argc >= 4) {
        std::vector<fs::path> in(argv + 3, argv + argc);
        return fixed(argv[2], in);
    }
    if (cmd == "dump" && (argc == 3 || argc == 4)) return dump(argv[2], argc == 4 ? argv[3] : "");
    if (cmd == "check" && argc >= 3) return check(argv[2], std::vector<std::string>(argv + 3, argv + argc));
    return usage();
}
