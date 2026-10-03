// The game-only CARD names Aurora's <dolphin/card.h> lacks.
//
// Phase 2, step 2.7 (m_Do; docs/NATIVE_PORT_PHASE2_3.md, decision D2). The name dolphin/card.h is
// both the game's and Aurora's, so Aurora's wins and no forwarder can sit in front of it. The game's card.h
// spells the CARD result codes CARD_ERROR_* (an anonymous enum); Aurora's spells them
// CARD_RESULT_* (macros) with the same values. A game unit that uses the CARD_ERROR_* names
// includes this header after dolphin/card.h under `TARGET_PC` (the GameCube build keeps the game's own
// card.h, which declares them).
#ifndef COS_CARD_EXTRAS_H
#define COS_CARD_EXTRAS_H

#include <dolphin/card.h>

// The game's dolphin/card.h, with each value taken from Aurora's matching CARD_RESULT_* macro.
enum {
    CARD_ERROR_UNLOCKED = CARD_RESULT_UNLOCKED,
    CARD_ERROR_READY = CARD_RESULT_READY,
    CARD_ERROR_BUSY = CARD_RESULT_BUSY,
    CARD_ERROR_WRONGDEVICE = CARD_RESULT_WRONGDEVICE,
    CARD_ERROR_NOCARD = CARD_RESULT_NOCARD,
    CARD_ERROR_NOFILE = CARD_RESULT_NOFILE,
    CARD_ERROR_IOERROR = CARD_RESULT_IOERROR,
    CARD_ERROR_BROKEN = CARD_RESULT_BROKEN,
    CARD_ERROR_EXIST = CARD_RESULT_EXIST,
    CARD_ERROR_NOENT = CARD_RESULT_NOENT,
    CARD_ERROR_INSSPACE = CARD_RESULT_INSSPACE,
    CARD_ERROR_NOPERM = CARD_RESULT_NOPERM,
    CARD_ERROR_LIMIT = CARD_RESULT_LIMIT,
    CARD_ERROR_NAMETOOLONG = CARD_RESULT_NAMETOOLONG,
    CARD_ERROR_ENCODING = CARD_RESULT_ENCODING,
    CARD_ERROR_CANCELED = CARD_RESULT_CANCELED,
    CARD_ERROR_FATAL_ERROR = CARD_RESULT_FATAL_ERROR,
};

#endif // COS_CARD_EXTRAS_H
