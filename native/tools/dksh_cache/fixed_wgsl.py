#!/usr/bin/env python3
"""Writes the WGSL of the renderer's fixed shaders (the ones Aurora does not generate per GX config)
as one .wgsl file per module, for `dksh_cache wgsl` (docs/DEKO3D_MIGRATION_PLAN.md, phase 1):

    native/tools/dksh_cache/fixed_wgsl.py <Aurora copy> <imgui source> <out dir>

<Aurora copy> is the patched Aurora that dksh_cache builds against (build/dksh_cache/aurora), <imgui
source> the Switch build's ImGui (build/switch-native/_deps/imgui-src; "-" to skip ImGui). Modules:
clear (colour, depth only), tex_copy_conv (blit, 14 colour formats, Z8, Z16, depth snapshot, the
multisampled one), tex_palette_conv (direct, from 8-bit, from 4-bit), the XFB copy and the present
resample of webgpu/gpu.cpp, and ImGui's two shaders. The text is read from the C++ raw strings and
put together as the sources do; a source that changes shape makes this script fail loudly.
"""
import os
import re
import sys

# R"delim(...)delim" (a delimiter may contain quotes: R"""(...)""")
RAW = re.compile(r'R"([^()\\\s]{0,16})\((.*?)\)\1"', re.S)


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def named_raw(src, name):
    """The raw string literal assigned to `name` (the first one after `name =`)."""
    m = re.search(r"\b" + re.escape(name) + r"\s*(?:\[\])?\s*=\s*", src)
    if not m:
        raise SystemExit("fixed_wgsl: %s not found" % name)
    r = RAW.search(src, m.end())
    if not r or src[m.end():r.start()].strip():
        raise SystemExit("fixed_wgsl: %s is not a raw string" % name)
    return r.group(2), r.end()


def main():
    if len(sys.argv) != 4:
        print(__doc__, file=sys.stderr)
        return 2
    aurora, imgui, out = sys.argv[1:]
    lib = os.path.join(aurora, "lib")
    os.makedirs(out, exist_ok=True)
    modules = {}

    # gfx/tex_copy_conv.cpp: preamble + one fragment shader per format
    src = read(os.path.join(lib, "gfx", "tex_copy_conv.cpp"))
    preamble, _ = named_raw(src, "ShaderPreamble")
    depth, end = named_raw(src, "DepthShaderPreamble")
    # `)"s + (gx::UseReversedZ ? R"(reversed)"s : R"(forward)"s);`: Aurora uses reversed Z
    if "UseReversedZ" not in src[end:end + 80] or "UseReversedZ = true" not in read(os.path.join(lib, "gx", "gx.hpp")):
        raise SystemExit("fixed_wgsl: DepthShaderPreamble is no longer `... + (gx::UseReversedZ ? ...)`")
    depth += RAW.search(src, end).group(2)
    for name, frag in re.findall(r"ConvPipeline\{(\w+), (\w+),", src):
        body, _ = named_raw(src, frag)
        is_depth = name in ("GX_TF_Z8", "GX_TF_Z16")
        modules["tex_copy_conv_" + name.split("_", 2)[-1].lower()] = (depth if is_depth else preamble) + body
    modules["tex_copy_conv_blit"] = preamble + named_raw(src, "FragPassthrough")[0]
    modules["tex_copy_conv_depth_snapshot"] = named_raw(src, "DepthSnapshotShader")[0]
    modules["tex_copy_conv_depth_snapshot_ms"] = named_raw(src, "DepthSnapshotShaderMS")[0]

    # gfx/tex_palette_conv.cpp
    src = read(os.path.join(lib, "gfx", "tex_palette_conv.cpp"))
    vtx, _ = named_raw(src, "ShaderPreambleVtx")
    for frag in ("ShaderDirect", "ShaderFromFloat8", "ShaderFromFloat4"):
        modules["tex_palette_conv_" + frag[6:].lower()] = vtx + named_raw(src, frag)[0]

    # gfx/clear.cpp shader_source(): the vertex part, then a colour output or none (depth only; the
    # normal buffer is off on the Switch)
    src = read(os.path.join(lib, "gfx", "clear.cpp"))
    m = re.search(r"std::string shader_source\(.*?\{(.*?)return source;\n\}", src, re.S)
    raws = [r.group(2) for r in RAW.finditer(m.group(1))]
    if len(raws) != 3 or "fs_main() -> FragmentOutput" not in raws[1]:
        raise SystemExit("fixed_wgsl: clear.cpp shader_source() changed shape")
    colour = raws[1].replace("{{", "{").replace("}}", "}").replace("{0}", "    @location(0) color: vec4f,\n")
    colour = colour.replace("{1}", "    out.color = vec4f(1.0);\n")
    modules["clear_color"] = raws[0] + colour
    modules["clear_depth"] = raws[0] + raws[2]

    # webgpu/gpu.cpp: the present resample and the XFB copy
    src = read(os.path.join(lib, "webgpu", "gpu.cpp"))
    modules["present_resample"] = named_raw(src, "resampleShaderSource")[0]
    m = re.search(r"void create_copy_pipeline\(\)", src)
    modules["xfb_copy"] = RAW.search(src, m.end()).group(2)

    # ImGui's WebGPU backend
    if imgui != "-":
        src = read(os.path.join(imgui, "backends", "imgui_impl_wgpu.cpp"))
        modules["imgui_vert"] = named_raw(src, "__shader_vert_wgsl")[0]
        modules["imgui_frag"] = named_raw(src, "__shader_frag_wgsl")[0]

    for name, text in sorted(modules.items()):
        with open(os.path.join(out, name + ".wgsl"), "w", encoding="utf-8") as f:
            f.write(text)
    print("%d fixed modules written to %s" % (len(modules), out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
