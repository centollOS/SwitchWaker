// The randomizer's in-game smokes (COS_SMOKE=rando, COS_RANDO_SMOKE=<name>; docs/RANDOMIZER_PLAN.md
// step R0.1). pc_rando_smokes.cpp picks the function listed for <name> in pc_rando_smokes.def and
// calls it from the frame loop every game frame, from the first frame on, until it ends the run with
// randoSmokeEnd (or the run's timeout does). A step's smoke, in smokes/pc_rando_smoke_<name>.cpp:
//
//   #include "pc_rando_smokes.h"
//   namespace pc {
//   void randoSmokeFoo(unsigned int frames) {
//       if (!randoSmokeInPlay(10)) return;           // the play scene runs, the player is spawned
//       ...checks, each randoSmokeCheck("foo", what, ok)...
//       randoSmokeEnd("foo");                        // PASS (exit 0) if every check held, else FAIL (1)
//   }
//   }
//
// then appends PC_RANDO_SMOKE(foo, randoSmokeFoo) to pc_rando_smokes.def.
#pragma once

#include "pc_internal.h"

namespace pc {

#define PC_RANDO_SMOKE(name, fn) void fn(unsigned int frames);
#include "pc_rando_smokes.def"
#undef PC_RANDO_SMOKE

// True once the play scene runs with the player spawned for at least `settle` frames (counted
// from the first frame this was true).
bool randoSmokeInPlay(unsigned int settle = 0);
// Logs "[cos] rando-smoke <smoke>: <what>: ok|FAIL" and records a failure. Returns ok.
bool randoSmokeCheck(const char* smoke, const char* what, bool ok);
// Ends the run: "[cos] rando-smoke <smoke>: PASS" (exit 0) when no check failed, else FAIL (exit 1).
[[noreturn]] void randoSmokeEnd(const char* smoke);

} // namespace pc
