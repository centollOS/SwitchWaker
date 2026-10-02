#ifndef DOLZEL_H
#define DOLZEL_H

#if defined(__MWERKS__) && !defined(DECOMPCTX)
#include "d/dolzel.mch"
#else
#include "d/dolzel.pch"
#endif

// Parts of headers that were not in the original PCH.
#include "f_op/f_op_actor_mng.h" // IWYU pragma: export

#endif /* DOLZEL_H */
