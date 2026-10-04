// Shared by the Dawn GL tests (switch/dawn/gltest/README.md): a Dawn device on the OpenGL ES
// backend, as Aurora creates it (compatibility mode, 64 bytes of immediates, two storage buffers
// per stage).
#pragma once

#include <dawn/webgpu_cpp.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>

namespace gltest {

struct Context {
    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
};

inline bool Init(Context& ctx, bool exitOnError = true) {
    static const std::array kInstanceFeatures{wgpu::InstanceFeatureName::TimedWaitAny};
    wgpu::InstanceDescriptor instanceDesc{};
    instanceDesc.requiredFeatureCount = kInstanceFeatures.size();
    instanceDesc.requiredFeatures = kInstanceFeatures.data();
    ctx.instance = wgpu::CreateInstance(&instanceDesc);
    if (!ctx.instance) {
        fprintf(stderr, "no instance\n");
        return false;
    }
    wgpu::RequestAdapterOptions options{};
    options.featureLevel = wgpu::FeatureLevel::Compatibility;
    options.backendType = wgpu::BackendType::OpenGLES;
    wgpu::Future f = ctx.instance.RequestAdapter(
        &options, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message) {
            if (status == wgpu::RequestAdapterStatus::Success) {
                ctx.adapter = std::move(adapter);
            } else {
                fprintf(stderr, "adapter: %.*s\n", int(message.length), message.data);
            }
        });
    ctx.instance.WaitAny(f, UINT64_MAX);
    if (!ctx.adapter) {
        return false;
    }
    wgpu::AdapterInfo info{};
    ctx.adapter.GetInfo(&info);
    printf("adapter: %.*s / %.*s\n", int(info.device.length), info.device.data, int(info.description.length),
           info.description.data);

    wgpu::Limits supported{};
    ctx.adapter.GetLimits(&supported);
    static wgpu::CompatibilityModeLimits compat{};
    compat.maxStorageBuffersInVertexStage = 2;
    compat.maxStorageBuffersInFragmentStage = 2;
    wgpu::Limits limits{};
    limits.nextInChain = &compat;
    limits.maxStorageBuffersPerShaderStage = 2;
    limits.maxImmediateSize = 64;
    limits.minUniformBufferOffsetAlignment = std::max<uint32_t>(supported.minUniformBufferOffsetAlignment, 64);
    // GLTEST_DUMP=1: print the GLSL Dawn generates (Dawn's dump_shaders toggle).
    static const std::array kToggles{"disable_symbol_renaming", "enable_immediate_error_handling"};
    static const std::array kTogglesDump{"disable_symbol_renaming", "enable_immediate_error_handling",
                                         "dump_shaders"};
    const bool dump = getenv("GLTEST_DUMP") != nullptr;
    wgpu::DawnTogglesDescriptor toggles{};
    toggles.enabledToggleCount = dump ? kTogglesDump.size() : kToggles.size();
    toggles.enabledToggles = dump ? kTogglesDump.data() : kToggles.data();
    wgpu::DeviceDescriptor deviceDesc{};
    deviceDesc.nextInChain = &toggles;
    deviceDesc.requiredLimits = &limits;
    static bool sExitOnError = true;
    sExitOnError = exitOnError;
    deviceDesc.SetUncapturedErrorCallback(
        [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView message) {
            fprintf(stderr, "device error %d: %.*s\n", int(type), int(message.length), message.data);
            if (sExitOnError) {
                exit(2);
            }
        });
    wgpu::Future df = ctx.adapter.RequestDevice(
        &deviceDesc, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message) {
            if (status == wgpu::RequestDeviceStatus::Success) {
                ctx.device = std::move(device);
            } else {
                fprintf(stderr, "device: %.*s\n", int(message.length), message.data);
            }
        });
    ctx.instance.WaitAny(df, UINT64_MAX);
    if (ctx.device && dump) {
        ctx.device.SetLoggingCallback([](wgpu::LoggingType, wgpu::StringView message) {
            printf("%.*s\n", int(message.length), message.data);
        });
    }
    return bool(ctx.device);
}


}  // namespace gltest
