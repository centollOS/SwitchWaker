// The shader lines of the Switch log (docs/SWITCH_BUILD.md, "Shader cache").
//
// The deko3d NRO's shaders are DKSH: the bundled initial_dksh_cache.bin and the ones compiled on the
// console into user/cache/dksh_local.bin (switch/deko). Its registry's "[cos] shaders:" line
// (switch/deko/shaders.cpp) comes with the memory report every 15 s and at exit.
#include "cos_switch_internal.h"

#include "dk_aurora.h"

int cos_switch_shader_cache_report(char* out, size_t size, int force) {
    (void)force;  // the registry's line is built every time
    out[0] = '\0';
    return aurora_switch_dk_shaders_report(out, size);
}
