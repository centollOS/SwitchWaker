#pragma once
// Tracy's macros as no-ops: dksh_cache compiles two Aurora sources that include tracy/Tracy.hpp
// (Aurora builds Tracy with TRACY_ENABLE off, which expands them to nothing as well).
#define ZoneScoped
#define ZoneScopedN(name)
#define ZoneScopedC(color)
#define ZoneScopedNC(name, color)
#define ZoneText(text, size)
#define ZoneName(text, size)
#define FrameMark
#define FrameMarkNamed(name)
#define TracyPlot(name, value)
#define TracyMessage(text, size)
#define TracyMessageL(text)
