// The randomizer on the file select and the "Clean unused seed files" row (docs/RANDOMIZER_PLAN.md
// step R4.4, decisions D3 and D15; API in features/rando/pc_rando_fileselect.h, the hook declared in
// native/include/pc/pc_rando_hooks.h). Reading only: a vanilla slot (no rando record) and a blank slot
// with no pending seed show nothing, and the game's data is never written. Everything here allocates
// host memory (JKRPcHostAllocScope): the hook runs in the file select's draw with the scene's heap
// current, and that heap is gone when the cache is next freed.
#include "d/dolzel.h" // IWYU pragma: keep
#include "features/rando/pc_rando_fileselect.h"
#include "features/rando/pc_rando_menu.h"
#include "features/rando/pc_rando_settings.h"
#include "features/rando/pc_rando_slot.h"
#include "pc/pc_harness.h"
#include "pc/pc_rando_hooks.h"
#include "pc_internal.h"

#include "d/d_save.h"
#include "m_Do/m_Do_MemCardRWmng.h"

#include <imgui.h>

#include <climits>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <unistd.h>

namespace pc {

namespace {

// A gap of more frames than this between two draws is a new visit of the file select: the sidecar
// cache is dropped (a sidecar may have been written or removed meanwhile).
constexpr unsigned int kNewVisitFrames = 30;
// DataSelProc indices (d_file_select.cpp) where the selected slot is on screen: dataSelect,
// SelectTitAnime, dataSelectPaneMove, menuSelect, ToCopyPaneMove, ToErasePaneMove, ToBackPaneMove.
constexpr int kLastShownProc = 6;

struct CacheEntry {
    bool ok = false;
    std::string seed, words, sidecar;
};

RandoFileSelectInfo sInfo;
std::map<pcr::SeedHash, CacheEntry> sCache;
pcr::SeedHash sPendingHash{};
CacheEntry sPendingEntry;
bool sPendingCached = false;
bool sMenuInit = false;

std::string cardDir() {
    char buf[PATH_MAX];
    return pc_rando_slot_card_dir(buf, sizeof buf) != 0 ? std::string(buf) : std::string();
}

const CacheEntry& lookup(const pcr::SeedHash& h) {
    auto it = sCache.find(h);
    if (it != sCache.end()) {
        return it->second;
    }
    CacheEntry e;
    e.sidecar = pc_rando_slot_sidecar_name(h.data());
    const std::string dir = cardDir();
    PcRandoSeed seed;
    std::string err;
    if (!dir.empty() && pc_rando_seed_load_file((dir + "/" + e.sidecar).c_str(), &seed, &err) &&
        memcmp(seed.hash, h.data(), 16) == 0) {
        e.ok = true;
        e.seed = seed.seed;
        e.words = pcr::seedHashWords(seed);
    } else {
        writef(STDERR_FILENO, "[cos] rando file select: %s: %s\n", e.sidecar.c_str(),
               err.empty() ? "the file's hash differs from the slot's record" : err.c_str());
    }
    return sCache.emplace(h, e).first->second;
}

const CacheEntry& pendingEntry(const PcRandoSeed& p) {
    pcr::SeedHash h;
    memcpy(h.data(), p.hash, 16);
    if (!sPendingCached || h != sPendingHash) {
        sPendingCached = true;
        sPendingHash = h;
        sPendingEntry = CacheEntry();
        sPendingEntry.ok = true;
        sPendingEntry.seed = p.seed;
        sPendingEntry.words = pcr::seedHashWords(p);
    }
    return sPendingEntry;
}

std::string cleanValue(bool es) {
    (void)es;
    return std::string();
}

std::string cleanActivate(bool es) {
    return randoCleanSidecars(es, nullptr);
}

} // namespace

const RandoFileSelectInfo& randoFileSelectInfo() {
    return sInfo;
}

void randoFileSelectOverlay() {
    const unsigned int now = pc_frame_count();
    if (sInfo.frame == 0 || sInfo.text.empty() || sInfo.frame + 1 < now) {
        return;
    }
    if (ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 pos(io.DisplaySize.x * 0.5f, io.DisplaySize.y - io.DisplaySize.y * 0.04f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.65f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                   ImGuiWindowFlags_NoMove;
    if (ImGui::Begin("##rando_file_select", nullptr, flags)) {
        const bool warn = sInfo.kind == pcr::FileInfoKind::Missing;
        if (warn) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.35f, 1.0f));
        }
        ImGui::TextUnformatted(sInfo.text.c_str());
        if (warn) {
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();
    sInfo.shownFrames++;
}

void randoFileSelectMenuInit() {
    if (sMenuInit) {
        return;
    }
    sMenuInit = true;
    randoMenuAddAction({"Borrar semillas sin usar", "Clean unused seed files",
                        "A: borra los archivos switchwaker-rando-*.json de la tarjeta que no usa ninguna partida "
                        "ni la semilla pendiente",
                        "A: removes the card's switchwaker-rando-*.json files that no save slot and no pending seed "
                        "uses",
                        cleanValue, cleanActivate, 150}); // after R4.5's "Spoiler log..." (140)
}

std::string randoCleanSidecars(bool es, pcr::SidecarCleanResult* out) {
    JKRPcHostAllocScope hostAlloc;
    pcr::SidecarCleanResult r;
    std::string toast;
    if (randoGenBusy()) {
        r.error = "a seed is being generated";
        toast = es ? "Espera a que termine la generación" : "Wait until the seed is generated";
    } else {
        std::vector<pcr::SeedHash> keep;
        if (const PcRandoSeed* p = pc_rando_slot_pending(); p != nullptr) {
            pcr::SeedHash h;
            memcpy(h.data(), p->hash, 16);
            keep.push_back(h);
        }
        if (const PcRandoSeed* a = pc_rando_slot_active_seed(); a != nullptr && pc_rando()) {
            pcr::SeedHash h;
            memcpy(h.data(), a->hash, 16);
            keep.push_back(h);
        }
        const std::string dir = cardDir();
        r = pcr::sidecarClean(dir, keep, false);
        sCache.clear();
        writef(STDERR_FILENO,
               "[cos] rando clean: %s: %d .gci file(s), %d record(s), %zu kept, %zu removed, %zu failed%s%s\n",
               dir.empty() ? "(no card folder)" : dir.c_str(), r.gciFiles, r.records, r.kept.size(),
               r.removed.size(), r.failed.size(), r.error.empty() ? "" : ": ", r.error.c_str());
        for (const std::string& f : r.removed) {
            writef(STDERR_FILENO, "[cos] rando clean: removed %s\n", f.c_str());
        }
        char b[160];
        if (!r.ok && r.removed.empty()) {
            toast = std::string(es ? "No se borró nada: " : "Nothing removed: ") + r.error;
        } else {
            snprintf(b, sizeof b, es ? "%zu archivo(s) de semilla borrado(s), %zu en uso" : "%zu seed file(s) removed, %zu in use",
                     r.removed.size(), r.kept.size());
            toast = b;
        }
    }
    if (out != nullptr) {
        *out = r;
    }
    return toast;
}

} // namespace pc

extern "C" void pc_rando_file_select_draw(const uint8_t* saveData, uint8_t selectNum, uint8_t proc) {
    using namespace pc;
    using K = pcr::FileInfoKind;
    JKRPcHostAllocScope hostAlloc;
    const unsigned int now = pc_frame_count();
    if (sInfo.frame == 0 || now > sInfo.frame + kNewVisitFrames) {
        sCache.clear();
    }
    sInfo.frame = now;
    sInfo.slot = selectNum;
    sInfo.proc = proc;
    sInfo.kind = K::None;
    sInfo.text.clear();
    sInfo.seed.clear();
    sInfo.words.clear();
    if (saveData == nullptr || selectNum > 2 || proc > kLastShownProc) {
        return;
    }
    const bool es = pc_ui_spanish();
    uint8_t* slot = const_cast<uint8_t*>(saveData) + selectNum * sizeof(card_gamedata);
    const dSv_save_c_PACKED* save = (const dSv_save_c_PACKED*)slot;
    const bool used = mDoMemCdRWm_TestCheckSumGameData(slot) && save->mPlayer.mInfo.mPlayerName[0] != 0;
    const CacheEntry* e = nullptr;
    if (used) {
        uint8_t hash[16];
        if (!pc_rando_slot_record_read(save->mReserve.mReserve, hash, nullptr)) {
            return; // vanilla (or a record of another version, which plays vanilla too)
        }
        pcr::SeedHash h;
        memcpy(h.data(), hash, 16);
        e = &lookup(h);
        sInfo.kind = e->ok ? K::Rando : K::Missing;
    } else {
        const PcRandoSeed* p = pc_rando_slot_pending();
        if (p == nullptr) {
            return;
        }
        e = &pendingEntry(*p);
        sInfo.kind = K::Pending;
    }
    sInfo.seed = e->seed;
    sInfo.words = e->words;
    sInfo.text = pcr::fileInfoText(sInfo.kind, e->seed, e->words, e->sidecar, es);
}
