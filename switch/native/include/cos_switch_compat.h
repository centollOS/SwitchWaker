// Force-included in every game unit of the Switch build (switch/native/CMakeLists.txt), after
// native/'s own force-included headers.
//
// newlib's <ctype.h> defines its character class masks as the macros _U, _L, _N, _S, _P, _C, _X
// and _B, and the decomp names struct members after their offsets (f_op_msg_mng.h has a member
// _C). In C++ the game never needs the macros: libstdc++'s <cctype> replaces the classification
// macros with functions, and its newlib ctype_base (bits/ctype_base.h, without an include guard of
// its own, read through <locale>), the one libstdc++ header that names the masks, is read here
// first. The macros are then removed.
#pragma once

#ifdef __cplusplus
#include <cctype>
#include <locale>
#undef _U
#undef _L
#undef _N
#undef _S
#undef _P
#undef _C
#undef _X
#undef _B
#endif
