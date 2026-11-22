#include "prx/libSceAudio3d/include/Audio3d.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace {

void SetEnvironment(const char* name, const std::string& value) {
#ifdef _WIN32
    SetEnvironmentVariableA(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "Audio3d flush report: %s\n", message);
    std::abort();
}

struct State {
    std::atomic<int> calls{0};
    std::atomic<bool> flushed{false};
};

void APS5_VABI FlushFromReport(const Audio3dReport*, void* data) {
    auto& state = *static_cast<State*>(data);
    state.calls.fetch_add(1);
    if (state.flushed.exchange(true)) return;
    Require(sceAudio3dPortFlush(0) == 0, "flush from report handler");
}

}

int main() {
    SetEnvironment("SDL_AUDIODRIVER", "disk");
    SetEnvironment("SDL_DISKAUDIOFILE", (std::filesystem::temp_directory_path() /
        "anyps5-audio3d-flush-report.raw").string());

    Audio3dOpenParameters parameters{};
    sceAudio3dGetDefaultOpenParameters(&parameters);
    Require(sceAudio3dInitialize(0) == 0, "initialize");
    std::uint32_t id = 7;
    Require(sceAudio3dPortOpen(0xFF, &parameters, &id) == 0 && id == 0, "open");
    const std::array<std::int16_t, 512> samples{};
    Require(sceAudio3dBedWrite(0, 2, 0, samples.data(), 256) == 0, "bed write");

    State state;
    std::uint64_t handler = 0;
    Require(sceAudio3dReportRegisterHandler(0, FlushFromReport, &state, &handler) == 0, "register handler");
    Require(sceAudio3dPortGetStatus(1, nullptr, nullptr, 0) == static_cast<int>(0x80EA0002), "status report");
    Require(sceAudio3dReportUnregisterHandler(handler) == 0, "unregister handler");

    const int calls = state.calls.load();
    std::uint32_t level = 99;
    std::uint32_t available = 99;
    Require(sceAudio3dPortGetQueueLevel(0, &level, &available) == 0, "queue level");
    Require(level == 0 && available == 2, "flush from report did not drain the mixed frame");
    Require(calls == 2, "flush reported once through internal advancement instead of only its own export");
    Require(sceAudio3dPortClose(0) == 0, "close port");
    Require(sceAudio3dTerminate() == 0, "terminate");
    std::puts("Audio3d flush report tests passed");
}
