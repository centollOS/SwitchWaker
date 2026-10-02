/* Scaffold check (C11): the decomp's base headers compile as C under TARGET_PC. */
#include "global.h"

_Static_assert(sizeof(s32) == 4 && sizeof(u32) == 4, "32-bit types");
_Static_assert(sizeof(BOOL) == 4, "BOOL is int");

int cos_pc_check_c(u32 value) {
    return __cntlzw(value) + (TRUE ? 1 : 0);
}
