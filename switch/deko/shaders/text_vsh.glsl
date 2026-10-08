// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// From SwitchWakerHD (https://github.com/centollOS/SwitchWakerHD) at df8fbde:
// runtime/src/gfx/deko/shaders/text_vsh.glsl.
// One triangle covering the viewport (the text box): text_fsh.glsl draws from gl_FragCoord.
#version 460
void main() {
    vec2 p = vec2(float((gl_VertexID & 1) * 4 - 1), float((gl_VertexID & 2) * 2 - 1));
    gl_Position = vec4(p, 0.0, 1.0);
}
