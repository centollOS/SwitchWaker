// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
// From SwitchWakerHD (https://github.com/centollOS/SwitchWakerHD) at df8fbde:
// runtime/src/gfx/deko/shaders/text_fsh.glsl.
// The test pattern's 3x5 pixel font (pattern.cpp draw_text): a bit pattern per character, row 0
// (top) in bits 14-12, column 0 (left) the highest bit of a row. Window coordinates from the top
// left (OriginUpperLeft): with the other origin the text shows mirrored upside down.
#version 460
layout(location = 0) out vec4 outColor;
layout(std140, binding = 0) uniform Text {
    vec4 box;           // left, top (window pixels), window pixels per font pixel, unused
    ivec4 grid;         // columns, rows
    vec4 fg;
    vec4 bg;
    uvec4 glyphs[192];  // up to 768 characters, row by row, four per entry
};
void main() {
    ivec2 p = ivec2(floor((gl_FragCoord.xy - box.xy) / box.z)) - ivec2(1);
    bool on = false;
    if (p.x >= 0 && p.y >= 0) {
        ivec2 cell = p / ivec2(4, 6), sub = p - cell * ivec2(4, 6);
        if (cell.x < grid.x && cell.y < grid.y && sub.x < 3 && sub.y < 5) {
            int i = cell.y * grid.x + cell.x;
            // the component by explicit selects: uam drops a dynamic component index of a
            // uniform-block vector and reads .x (SwitchWakerHD's FPS counter showed "3333")
            uvec4 q = glyphs[i >> 2];
            int c = i & 3;
            uint g = c == 0 ? q.x : (c == 1 ? q.y : (c == 2 ? q.z : q.w));
            on = ((g >> uint((4 - sub.y) * 3 + (2 - sub.x))) & 1u) != 0u;
        }
    }
    outColor = on ? fg : bg;
}
