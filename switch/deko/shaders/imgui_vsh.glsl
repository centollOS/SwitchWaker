// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// From SwitchWakerHD (https://github.com/centollOS/SwitchWakerHD) at df8fbde:
// runtime/src/gfx/deko/shaders/imgui_vsh.glsl.
// ImGui (overlay.cpp): its pixel coordinates to normalized ones with y down; deko3d's clip-space y
// points up (dk.h), so y is negated.
#version 460
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec4 inColor;
layout(location = 0) out vec2 outUv;
layout(location = 1) out vec4 outColor;
layout(std140, binding = 0) uniform Xform {
    vec4 xform;  // scale.xy, translate.xy
};
void main() {
    outUv = inUv;
    outColor = inColor;
    vec2 p = inPos * xform.xy + xform.zw;
    gl_Position = vec4(p.x, -p.y, 0.0, 1.0);
}
