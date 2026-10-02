// Forced into every C source of the Switch host build (-include). newlib on
// Horizon lacks a per-thread CPU clock; the host only uses it for diagnostic
// timing, so wall time stands in.
#pragma once

#include <time.h>

#ifndef CLOCK_THREAD_CPUTIME_ID
#define CLOCK_THREAD_CPUTIME_ID CLOCK_MONOTONIC
#endif
