// cos_rando_test_R7_2d (docs/RANDOMIZER_PLAN.md step R7.2d): material and animation colours
// (pc_rando_j3d.h: MAT3 tev / konst colours, MDL3 TEV colour registers as gclib's save regenerates
// them, TRK1 colour animations in gclib's layout) on the synthetic files of
// fixtures/R7_2d_j3d.txt (native/tools/rando/make_j3d_color_fixture.py: wwrando's own
// shift_all_colors_in_mat3 / _mdl3 / _trk1 and gclib's J3D.save). Each file shifted by the port must be
// byte-identical to gclib's. The disc's files are checked by the palettes smoke and palette_check.py.
// Prints "ok" and exits 0 when every check holds.
#include "rando_test.h"

#include "pc_rando_j3d.h"

#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<uint8_t>;

Bytes unhex(const std::string& s) {
    Bytes b;
    b.reserve(s.size() / 2);
    for (size_t i = 0; i + 1 < s.size(); i += 2) {
        b.push_back((uint8_t)strtoul(s.substr(i, 2).c_str(), nullptr, 16));
    }
    return b;
}

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < line.size()) {
        size_t j = line.find(' ', i);
        if (j == std::string::npos) {
            j = line.size();
        }
        if (j > i) {
            out.push_back(line.substr(i, j - i));
        }
        i = j + 1;
    }
    return out;
}

struct Case {
    std::string file;
    Bytes input;
};

void collect(const char* line, void* ctx) {
    ((std::vector<std::string>*)ctx)->push_back(line);
}

// First differing byte, for the failure message.
void reportDiff(const std::string& what, const Bytes& got, const Bytes& want) {
    size_t i = 0;
    while (i < got.size() && i < want.size() && got[i] == want[i]) {
        i++;
    }
    fprintf(stderr, "  %s: first difference at 0x%zx (got %zu bytes, want %zu)\n", what.c_str(), i, got.size(),
            want.size());
}

uint32_t rd32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

} // namespace

int main() {
    std::string text;
    RT_CHECK(rt::readFixture("R7_2d_j3d.txt", text));
    std::map<std::string, Case> cases;
    int shifts = 0, brkShifts = 0;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) {
            end = text.size();
        }
        const std::string line = text.substr(pos, end - pos);
        pos = end + 1;
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const std::vector<std::string> f = split(line);
        if (f[0] == "F" && f.size() == 4) {
            cases[f[1]] = {f[2], unhex(f[3])};
            const Case& c = cases[f[1]];
            // Unshifted, the MDL3 sync leaves gclib's own file alone (its registers already hold the
            // materials' colours), and the colour lines read.
            Bytes same = c.input;
            PcRandoRecolorStats st{};
            RT_CHECK_EQ(pc_rando_j3d_sync_mdl3(same.data(), same.size(), &st) <= 0 ||
                            st.mismatch + st.errors + st.conflicts == 0,
                        true);
            RT_CHECK(same == c.input);
            std::vector<std::string> lines;
            RT_CHECK(pc_rando_j3d_color_lines(c.input.data(), c.input.size(), collect, &lines) > 0);
            continue;
        }
        if (f[0] != "S" || f.size() != 5) {
            RT_CHECK(!"bad fixture line");
            continue;
        }
        auto it = cases.find(f[1]);
        if (!RT_CHECK(it != cases.end())) {
            continue;
        }
        const Case& c = it->second;
        const int h = atoi(f[2].c_str());
        const int v = atoi(f[3].c_str());
        const Bytes want = unhex(f[4]);
        Bytes got = c.input;
        PcRandoRecolorStats st{};
        RT_CHECK_EQ(pc_rando_j3d_shift_file(got.data(), got.size(), c.file.c_str(), h, v, &st), 1);
        RT_CHECK_EQ(st.errors, 0);
        RT_CHECK_EQ(st.mismatch, 0);
        RT_CHECK_EQ(st.conflicts, 0);
        RT_CHECK_EQ(st.trkInPlace, 0);
        // gclib's file, and zeros after it where its TRK1 came out shorter.
        const size_t n = want.size() <= got.size() ? want.size() : got.size();
        Bytes head(got.begin(), got.begin() + (ptrdiff_t)n);
        bool ok = head == want && want.size() <= got.size();
        for (size_t i = n; ok && i < got.size(); i++) {
            ok = got[i] == 0;
        }
        if (!RT_CHECK(ok)) {
            reportDiff(f[1] + " " + f[2] + "," + f[3], head, want);
        }
        shifts++;
        if (f[1] == "bdl") {
            RT_CHECK(st.matColors > 0 && st.mdlEntries > 0);
        }
        if (f[1] == "brk" || f[1] == "ccbrk") {
            RT_CHECK_EQ(st.trkRebuilt, 1);
            RT_CHECK_EQ(st.trkAnims, f[1] == "ccbrk" ? 5 : 6);
            brkShifts++;

            // TRK1 not the last chunk: gclib's chunk in its place, the chunk size and the next chunk
            // kept (only the chunk differs from gclib's file then).
            Bytes two = c.input;
            const size_t chunkSize = rd32(two.data() + 0x24);
            const uint8_t tail[0x20] = {'T', 'E', 'S', 'T', 0, 0, 0, 0x20, 1, 2, 3, 4};
            two.insert(two.end(), tail, tail + sizeof tail);
            two[0x0F] = 2;
            PcRandoRecolorStats st2{};
            pc_rando_j3d_shift_trk1(two.data(), two.size(), c.file.c_str(), h, v, &st2);
            RT_CHECK_EQ(st2.trkRebuilt, 1);
            RT_CHECK_EQ(st2.conflicts + st2.errors, 0);
            RT_CHECK_EQ((size_t)rd32(two.data() + 0x24), chunkSize);
            const size_t wantChunk = rd32(want.data() + 0x24);
            RT_CHECK(memcmp(two.data() + 0x28, want.data() + 0x28, wantChunk - 8) == 0);
            RT_CHECK(memcmp(two.data() + 0x20 + chunkSize, tail, sizeof tail) == 0);
        }
    }
    RT_CHECK_EQ(cases.size(), (size_t)4);
    RT_CHECK_EQ(shifts, 20);
    RT_CHECK_EQ(brkShifts, 10);

    // A material with unset colours (index 0xFFFF) and no list: skipped, nothing read out of bounds.
    {
        Bytes f = cases["bmd"].input;
        // Every colour index of material record 0 unset.
        PcRandoJ3dChunk mat3;
        RT_CHECK(pc_rando_j3d_find(f.data(), f.size(), ('M' << 24) | ('A' << 16) | ('T' << 8) | '3', &mat3));
        const size_t rec = mat3.offset + rd32(f.data() + mat3.offset + 0x0C);
        memset(f.data() + rec + 0x94, 0xFF, 8);
        memset(f.data() + rec + 0xDC, 0xFF, 8);
        PcRandoRecolorStats st{};
        RT_CHECK(pc_rando_j3d_shift_mat3(f.data(), f.size(), 120, -20, &st) > 0);
        RT_CHECK_EQ(st.errors, 0);
    }
    return rt::finish();
}
