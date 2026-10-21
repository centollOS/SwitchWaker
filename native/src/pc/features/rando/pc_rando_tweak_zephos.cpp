// Zephos and the auction intro (pc_rando_tweak_zephos.h, docs/RANDOMIZER_PLAN.md step R3.19):
// tweaks.py shorten_zephos_event and shorten_auction_intro_event (wwrando 1.10.0) on the raw
// event_list.dat bytes, through the event library of step R0.11 (pc_rando_event.h). Pure: no game
// headers, no game calls.
#include "pc_rando_tweak_zephos.h"

#include "pc_rando_event.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace {

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

bool sInit = false;
PcRandoZephosStatus sStatus[PCR_ZEPHOS_EDIT_COUNT] = {{0, 0, -1}, {0, 0, -1}};

// One actor of an edited event: its name, its vanilla action count and the vanilla name of one
// action the edit depends on (the one that ends the kept chain, or the first removed one).
struct ActorShape {
    const char* actor;
    int count;
    int ordinal;
    const char* action;
};

// shorten_zephos_event: zephos.actions[0:7], link.actions[0:7], camera.actions[0:5]; the kept chains
// end on Hr WAIT_1 (flag 7838), Link 001n_wait (7922) and CAMERA GETITEM (7965) on the USA disc.
constexpr int kTactKeep[3] = {7, 7, 5};
constexpr ActorShape kTactShape[3] = {
    {"Hr", 28, 6, "WAIT_1"},
    {"Link", 17, 6, "001n_wait"},
    {"CAMERA", 13, 4, "GETITEM"},
};
// shorten_auction_intro_event: camera.actions.remove(actions[3]) then remove(actions[4]).
constexpr ActorShape kAuctionCamera = {"CAMERA", 13, 3, "UNITRANS"};
constexpr const char* kAuctionPostPan = "PAUSE"; // actions[4]

// The actor slot of `shape` in `event`, with its global actor index, after checking the action count
// and the action name. A PcRandoEvlResult.
int checkActor(const PcRandoEvl* evl, int event, const ActorShape& shape, int* outActor) {
    const int slot = pc_rando_evl_find_actor(evl, event, shape.actor);
    if (slot < 0) {
        return PCR_EVL_NO_ACTOR;
    }
    const int actor = pc_rando_evl_event_actor(evl, event, slot);
    if (actor < 0) {
        return PCR_EVL_NO_ACTOR;
    }
    const int count = pc_rando_evl_action_count(evl, actor);
    if (count < 0) {
        return PCR_EVL_LOOP;
    }
    if (count != shape.count) {
        logf("[cos] rando zephos: actor %s has %d actions, expected %d\n", shape.actor, count, shape.count);
        return PCR_EVL_MISMATCH;
    }
    const int action = pc_rando_evl_actor_action(evl, actor, shape.ordinal);
    char name[PCR_EVL_NAME_MAX + 1];
    pc_rando_evl_name(pc_rando_evl_action_p(evl, action), name);
    if (strcmp(name, shape.action) != 0) {
        logf("[cos] rando zephos: actor %s action %d is '%s', expected '%s'\n", shape.actor, shape.ordinal, name,
             shape.action);
        return PCR_EVL_MISMATCH;
    }
    *outActor = actor;
    return PCR_EVL_OK;
}

void note(int edit, int result) {
    sStatus[edit].seen++;
    sStatus[edit].last = result;
    if (result == PCR_EVL_OK) {
        sStatus[edit].applied++;
    }
}

} // namespace

extern "C" int pc_rando_tweak_zephos_shorten_tact_ht(void* evlp) {
    PcRandoEvl* evl = (PcRandoEvl*)evlp;
    const int event = pc_rando_evl_find_event(evl, "TACT_HT");
    if (event < 0) {
        return PCR_EVL_NO_EVENT;
    }
    int actors[3];
    int32_t flags[3];
    for (int i = 0; i < 3; i++) {
        const int r = checkActor(evl, event, kTactShape[i], &actors[i]);
        if (r != PCR_EVL_OK) {
            return r;
        }
        // wwrando reads actions[-1].flag_id_to_set after the slice: the flag of the last kept action.
        flags[i] = pc_rando_evl_action_flag(evl, pc_rando_evl_actor_action(evl, actors[i], kTactKeep[i] - 1));
    }
    // Everything is checked; the edits below cannot be refused any more (the chains were walked).
    for (int i = 0; i < 3; i++) {
        const int r = pc_rando_evl_truncate_actions(evl, actors[i], kTactKeep[i]);
        if (r != PCR_EVL_OK) {
            return r;
        }
    }
    for (int i = 0; i < 3; i++) {
        const int r = pc_rando_evl_set_ending_flag(evl, event, i, flags[i]);
        if (r != PCR_EVL_OK) {
            return r;
        }
    }
    return PCR_EVL_OK;
}

extern "C" int pc_rando_tweak_zephos_shorten_auction(void* evlp) {
    PcRandoEvl* evl = (PcRandoEvl*)evlp;
    const int event = pc_rando_evl_find_event(evl, "AUCTION_START");
    if (event < 0) {
        return PCR_EVL_NO_EVENT;
    }
    int camera = -1;
    int r = checkActor(evl, event, kAuctionCamera, &camera);
    if (r != PCR_EVL_OK) {
        return r;
    }
    char name[PCR_EVL_NAME_MAX + 1];
    pc_rando_evl_name(pc_rando_evl_action_p(evl, pc_rando_evl_actor_action(evl, camera, 4)), name);
    if (strcmp(name, kAuctionPostPan) != 0) {
        logf("[cos] rando zephos: AUCTION_START CAMERA action 4 is '%s', expected '%s'\n", name, kAuctionPostPan);
        return PCR_EVL_MISMATCH;
    }
    // remove(actions[3]) then remove(actions[4]) of the original list, in one relink.
    int order[13];
    int n = 0;
    for (int i = 0; i < kAuctionCamera.count; i++) {
        if (i != 3 && i != 4) {
            order[n++] = i;
        }
    }
    return pc_rando_evl_set_chain(evl, camera, order, n);
}

extern "C" void pc_rando_tweak_zephos_patch(PcRandoResFile* file) {
    if (file == nullptr || strcmp(file->arc, "Stage") != 0 || strcmp(file->name, "event_list.dat") != 0) {
        return;
    }
    int edit;
    if (strcmp(file->stage, "sea") == 0) {
        edit = PCR_ZEPHOS_TACT_HT;
    } else if (strcmp(file->stage, "Orichh") == 0) {
        edit = PCR_ZEPHOS_AUCTION;
    } else {
        return;
    }
    PcRandoEvl evl;
    int r = pc_rando_evl_open(&evl, file->data, file->size);
    if (r == PCR_EVL_OK) {
        r = edit == PCR_ZEPHOS_TACT_HT ? pc_rando_tweak_zephos_shorten_tact_ht(&evl)
                                       : pc_rando_tweak_zephos_shorten_auction(&evl);
    }
    note(edit, r);
    logf("[cos] rando zephos: %s/%s %s: %s\n", file->stage, file->name,
         edit == PCR_ZEPHOS_TACT_HT ? "shorten_zephos_event (TACT_HT)" : "shorten_auction_intro_event (AUCTION_START)",
         pc_rando_evl_result_name(r));
}

extern "C" void pc_rando_tweak_zephos_init(void) {
    if (sInit) {
        return;
    }
    sInit = true;
    pc_rando_register_res_patcher(pc_rando_tweak_zephos_patch, PCR_RES_DAT);
}

extern "C" void pc_rando_tweak_zephos_status(int edit, PcRandoZephosStatus* out) {
    if (out == nullptr) {
        return;
    }
    if (edit < 0 || edit >= PCR_ZEPHOS_EDIT_COUNT) {
        *out = PcRandoZephosStatus{0, 0, -1};
        return;
    }
    *out = sStatus[edit];
}

extern "C" void pc_rando_tweak_zephos_reset_status(void) {
    for (PcRandoZephosStatus& s : sStatus) {
        s = PcRandoZephosStatus{0, 0, -1};
    }
}
