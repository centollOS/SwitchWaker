// Builds a render pipeline from every WGSL file in a directory with Aurora's GX pipeline layout
// (switch/dawn/gltest/README.md): group 0 two read-only storage buffers, group 1 one uniform
// buffer at a dynamic offset, group 2 the texture/sampler pairs the shader declares, 64 bytes of
// immediates. Dawn translates each to GLSL and Mesa compiles and links it, so a Switch GL patch
// that changes the generated GLSL (the uniform window) is checked against every shader the
// game has. The WGSL comes from a Mac run (build/wgsl-dump: Aurora's generated shaders).
//   gl_shader_test <dir> [--draw]
#include <dawn/webgpu_cpp.h>

#include <dirent.h>

#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "gltest_common.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: gl_shader_test <dir with .wgsl files>\n");
        return 2;
    }
    gltest::Context ctx;
    if (!gltest::Init(ctx, /*exitOnError=*/false)) {
        return 1;
    }
    wgpu::Device& device = ctx.device;

    std::vector<std::string> files;
    if (DIR* d = opendir(argv[1])) {
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n.size() > 5 && n.substr(n.size() - 5) == ".wgsl") {
                files.push_back(std::string(argv[1]) + "/" + n);
            }
        }
        closedir(d);
    }
    std::sort(files.begin(), files.end());

    std::array<wgpu::BindGroupLayoutEntry, 2> staticEntries{};
    staticEntries[0].binding = 0;
    staticEntries[0].visibility = wgpu::ShaderStage::Vertex;
    staticEntries[0].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    staticEntries[1].binding = 1;
    staticEntries[1].visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    staticEntries[1].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    wgpu::BindGroupLayoutDescriptor staticDesc{};
    staticDesc.entryCount = staticEntries.size();
    staticDesc.entries = staticEntries.data();
    wgpu::BindGroupLayout staticLayout = device.CreateBindGroupLayout(&staticDesc);
    wgpu::BindGroupLayoutEntry uniformEntry{};
    uniformEntry.binding = 0;
    uniformEntry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    uniformEntry.buffer.type = wgpu::BufferBindingType::Uniform;
    uniformEntry.buffer.hasDynamicOffset = true;
    wgpu::BindGroupLayoutDescriptor uniformDesc{};
    uniformDesc.entryCount = 1;
    uniformDesc.entries = &uniformEntry;
    wgpu::BindGroupLayout uniformLayout = device.CreateBindGroupLayout(&uniformDesc);

    const std::regex bindingRe(R"(@group\(2\)\s*@binding\((\d+)\)\s*var\s+\w+\s*:\s*([\w<>]+))");
    int failed = 0;
    size_t glslBytes = 0;
    for (const std::string& path : files) {
        std::ifstream in(path);
        std::stringstream ss;
        ss << in.rdbuf();
        const std::string code = ss.str();
        std::vector<wgpu::BindGroupLayoutEntry> texEntries;
        for (auto it = std::sregex_iterator(code.begin(), code.end(), bindingRe); it != std::sregex_iterator(); ++it) {
            wgpu::BindGroupLayoutEntry e{};
            e.binding = std::stoul((*it)[1]);
            e.visibility = wgpu::ShaderStage::Fragment;
            if ((*it)[2] == "sampler") {
                e.sampler.type = wgpu::SamplerBindingType::Filtering;
            } else {
                e.texture.sampleType = wgpu::TextureSampleType::Float;
                e.texture.viewDimension = wgpu::TextureViewDimension::e2D;
            }
            texEntries.push_back(e);
        }
        wgpu::BindGroupLayoutDescriptor texDesc{};
        texDesc.entryCount = texEntries.size();
        texDesc.entries = texEntries.data();
        wgpu::BindGroupLayout texLayout = device.CreateBindGroupLayout(&texDesc);
        const std::array layouts{staticLayout, uniformLayout, texLayout};
        wgpu::PipelineLayoutDescriptor plDesc{};
        plDesc.bindGroupLayoutCount = layouts.size();
        plDesc.bindGroupLayouts = layouts.data();
        plDesc.immediateSize = 64;
        wgpu::PipelineLayout layout = device.CreatePipelineLayout(&plDesc);

        device.PushErrorScope(wgpu::ErrorFilter::Internal);
        device.PushErrorScope(wgpu::ErrorFilter::Validation);
        wgpu::ShaderSourceWGSL wgsl{};
        wgsl.code = code.c_str();
        wgpu::ShaderModuleDescriptor smDesc{};
        smDesc.nextInChain = &wgsl;
        wgpu::ShaderModule module = device.CreateShaderModule(&smDesc);
        wgpu::ColorTargetState target{};
        target.format = wgpu::TextureFormat::RGBA8Unorm;
        wgpu::FragmentState frag{};
        frag.module = module;
        frag.entryPoint = "fs_main";
        frag.targetCount = 1;
        frag.targets = &target;
        wgpu::DepthStencilState ds{};
        ds.format = wgpu::TextureFormat::Depth24Plus;
        ds.depthWriteEnabled = wgpu::OptionalBool::True;
        ds.depthCompare = wgpu::CompareFunction::Less;
        wgpu::RenderPipelineDescriptor desc{};
        desc.layout = layout;
        desc.vertex.module = module;
        desc.vertex.entryPoint = "vs_main";
        desc.depthStencil = &ds;
        desc.fragment = &frag;
        wgpu::RenderPipeline pipeline = device.CreateRenderPipeline(&desc);
        std::string error;
        for (int i = 0; i < 2; i++) {
            wgpu::Future f = device.PopErrorScope(
                wgpu::CallbackMode::WaitAnyOnly,
                [&](wgpu::PopErrorScopeStatus, wgpu::ErrorType type, wgpu::StringView message) {
                    if (type != wgpu::ErrorType::NoError) {
                        error += std::string(message.data, message.length);
                    }
                });
            ctx.instance.WaitAny(f, UINT64_MAX);
        }
        if (!error.empty()) {
            if (failed < 3) {
                printf("FAIL %s:\n%s\n", path.c_str(), error.substr(0, 4000).c_str());
            }
            failed++;
        }
        glslBytes += code.size();
    }
    printf("%zu shaders, %d failed: %s\n", files.size(), failed, failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 3;
}
