/* MSL header name shim (TARGET_PC): the game includes Metrowerks' "msl_memory.h" (MSL's
 * <memory>: std::uninitialized_copy, std::uninitialized_fill_n, ...); the host C++ library's
 * <memory> provides the same declarations. */
#ifndef COS_PC_MSL_MEMORY_H
#define COS_PC_MSL_MEMORY_H
#include <memory>
#endif
