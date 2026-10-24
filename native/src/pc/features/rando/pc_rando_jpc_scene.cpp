// pc_rando_jpc_scene.cpp - the particle import and editor registry (docs/RANDOMIZER_PLAN.md step R7.1b;
// see pc_rando_jpc_scene.h).
#include "pc_rando_jpc_scene.h"

namespace {

struct Editor {
    int target;
    PcRandoJpcEditor fn;
    void* user;
};

PcRandoJpcImport sImports[PCR_JPC_SCENE_MAX_IMPORTS];
size_t sImportCount = 0;
Editor sEditors[PCR_JPC_SCENE_MAX_EDITORS];
size_t sEditorCount = 0;
PcRandoJpcCommonNotifier sCommonNotifier = nullptr;

bool editorApplies(const Editor& e, int scene) {
    if (e.target == scene) {
        return true;
    }
    return e.target == PCR_JPC_SCENE_ALL && scene >= 0 && scene <= 254;
}

} // namespace

int pc_rando_jpc_scene_add_import(int dest, uint16_t id, int source) {
    if (dest < 0 || dest > 254 || (source != PCR_JPC_SCENE_ANY && (source < 0 || source > 254))) {
        return 0;
    }
    for (size_t i = 0; i < sImportCount; i++) {
        if (sImports[i].dest == dest && sImports[i].id == id) {
            // One copy per (destination, id): wwrando skips an id the destination already holds,
            // so a second import of it would be skipped too.
            return 1;
        }
    }
    if (sImportCount >= PCR_JPC_SCENE_MAX_IMPORTS) {
        return 0;
    }
    sImports[sImportCount].id = id;
    sImports[sImportCount].dest = (int16_t)dest;
    sImports[sImportCount].source = (int16_t)source;
    sImportCount++;
    return 1;
}

int pc_rando_jpc_scene_add_editor(int target, PcRandoJpcEditor fn, void* user) {
    if (fn == nullptr ||
        !((target >= 0 && target <= 254) || target == PCR_JPC_SCENE_COMMON || target == PCR_JPC_SCENE_ALL)) {
        return 0;
    }
    for (size_t i = 0; i < sEditorCount; i++) {
        if (sEditors[i].target == target && sEditors[i].fn == fn && sEditors[i].user == user) {
            return 1;
        }
    }
    if (sEditorCount >= PCR_JPC_SCENE_MAX_EDITORS) {
        return 0;
    }
    sEditors[sEditorCount++] = Editor{target, fn, user};
    if (target == PCR_JPC_SCENE_COMMON && sCommonNotifier != nullptr) {
        sCommonNotifier();
    }
    return 1;
}

size_t pc_rando_jpc_scene_imports(int dest, PcRandoJpcImport* out, size_t cap) {
    size_t n = 0;
    for (size_t i = 0; i < sImportCount; i++) {
        if (sImports[i].dest != dest) {
            continue;
        }
        if (out != nullptr && n < cap) {
            out[n] = sImports[i];
        }
        n++;
    }
    return n;
}

bool pc_rando_jpc_scene_has_editors(int scene) {
    for (size_t i = 0; i < sEditorCount; i++) {
        if (editorApplies(sEditors[i], scene)) {
            return true;
        }
    }
    return false;
}

size_t pc_rando_jpc_scene_run_editors(int scene, uint8_t* data, size_t size, size_t from) {
    // An editor may register another one; the count is read again on each step.
    for (size_t i = from; i < sEditorCount; i++) {
        const Editor e = sEditors[i];
        if (editorApplies(e, scene)) {
            e.fn(scene, data, size, e.user);
        }
    }
    return sEditorCount;
}

size_t pc_rando_jpc_scene_import_count(void) {
    return sImportCount;
}

size_t pc_rando_jpc_scene_editor_count(void) {
    return sEditorCount;
}

void pc_rando_jpc_scene_clear(void) {
    sImportCount = 0;
    sEditorCount = 0;
}

void pc_rando_jpc_scene_set_common_notifier(PcRandoJpcCommonNotifier fn) {
    sCommonNotifier = fn;
}
