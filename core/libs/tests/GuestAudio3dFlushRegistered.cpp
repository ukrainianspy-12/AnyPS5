#include "prx/libSceAudio3d/include/Audio3d.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
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
    std::fprintf(stderr, "Audio3d registered flush: %s\n", message);
    std::abort();
}

}

int main() {
    SetEnvironment("SDL_AUDIODRIVER", "disk");
    SetEnvironment("SDL_DISKAUDIOFILE", (std::filesystem::temp_directory_path() /
        ("anyps5-audio3d-flush-registered-" + std::to_string(12345) + ".raw")).string());

    Audio3dOpenParameters parameters{};
    sceAudio3dGetDefaultOpenParameters(&parameters);
    Require(sceAudio3dInitialize(0) == 0, "initialize");
    std::uint32_t id = 7;
    Require(sceAudio3dPortOpen(0xFF, &parameters, &id) == 0 && id == 0, "open");
    const int handle = sceAudio3dAudioOutOpen(0, 0xFF, 0, 0, parameters.granularity, 48000, 1);
    Require(handle > 0, "open registered AudioOut");

    const std::array<std::int16_t, 512> samples{};
    Require(sceAudio3dBedWrite(0, 2, 0, samples.data(), 256) == 0, "bed write");
    Require(sceAudio3dPortAdvance(0) == 0, "advance");
    std::uint32_t level = 0;
    std::uint32_t available = 0;
    Require(sceAudio3dPortGetQueueLevel(0, &level, &available) == 0, "queue level before flush");
    Require(level == 1 && available == 1, "one frame queued before flush");
    Require(sceAudio3dPortFlush(0) == 0, "flush with registered handle");
    Require(sceAudio3dPortGetQueueLevel(0, &level, &available) == 0, "queue level after flush");
    Require(level == 1 && available == 1, "registered-handle flush must not drain mixed frames");

    Require(sceAudio3dPortClose(0) == 0, "close port");
    Require(sceAudio3dAudioOutClose(handle) == 0, "close registered AudioOut");
    Require(sceAudio3dTerminate() == 0, "terminate");
    std::puts("Audio3d registered flush tests passed");
}
