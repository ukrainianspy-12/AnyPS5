#include "prx/libSceAudio3d/include/Audio3d.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <system_error>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace {

constexpr std::uint32_t frames = 256;
const char* g_stage = "";

void SetEnvironment(const char* name, const std::string& value) {
#ifdef _WIN32
    SetEnvironmentVariableA(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "Audio3d flush: %s\n", message);
    std::abort();
}

std::int16_t Expected(int value) {
    return static_cast<std::int16_t>(std::lround(static_cast<double>(value) / 32768.0 * 32767.0));
}

std::vector<std::int16_t> Stereo(int left, int right) {
    std::vector<std::int16_t> result;
    result.reserve(frames * 2);
    for (std::uint32_t i = 0; i < frames; ++i) {
        const bool even = i % 2 == 0;
        result.push_back(Expected(even ? left : -left));
        result.push_back(Expected(even ? right : -right));
    }
    return result;
}

void AppendExpected(std::vector<std::int16_t>& expected, int left, int right) {
    const auto frame = Stereo(left, right);
    expected.insert(expected.end(), frame.begin(), frame.end());
}

void WriteBed(int left, int right) {
    const auto samples = Stereo(left, right);
    Require(sceAudio3dBedWrite(0, 2, 0, samples.data(), frames) == 0, "bed write");
}

bool CheckLevel(std::uint32_t level) {
    std::uint32_t got = 99;
    std::uint32_t available = 99;
    Require(sceAudio3dPortGetQueueLevel(0, &got, &available) == 0, g_stage);
    return got == level && available == 2 - level;
}

void RequireLevel(std::uint32_t level) {
    Require(CheckLevel(level), g_stage);
}

}

int main() {
    const auto path = std::filesystem::temp_directory_path() /
        ("anyps5-audio3d-flush-" + std::to_string(std::random_device{}()) + ".raw");
    std::filesystem::remove(path);
    SetEnvironment("SDL_AUDIODRIVER", "disk");
    SetEnvironment("SDL_DISKAUDIOFILE", path.string());

    Audio3dOpenParameters parameters{};
    sceAudio3dGetDefaultOpenParameters(&parameters);
    Require(sceAudio3dInitialize(0) == 0, "initialize");
    std::uint32_t id = 7;
    Require(sceAudio3dPortOpen(0xFF, &parameters, &id) == 0 && id == 0, "open");

    std::vector<std::int16_t> expected;
    g_stage = "single frame flush";
    WriteBed(12000, -6000);
    Require(sceAudio3dPortAdvance(0) == 0, "advance first bed");
    RequireLevel(1);
    Require(sceAudio3dPortFlush(0) == 0, "flush first frame");
    RequireLevel(0);
    AppendExpected(expected, 12000, -6000);

    g_stage = "two frame flush";
    WriteBed(8000, 4000);
    Require(sceAudio3dPortAdvance(0) == 0, "advance second bed");
    WriteBed(6000, -2000);
    Require(sceAudio3dPortAdvance(0) == 0, "advance third bed");
    RequireLevel(2);
    Require(sceAudio3dPortFlush(0) == 0, "flush two frames");
    RequireLevel(0);
    AppendExpected(expected, 8000, 4000);
    AppendExpected(expected, 6000, -2000);

    Require(sceAudio3dPortClose(0) == 0, "close port");
    Require(sceAudio3dTerminate() == 0, "terminate");

    g_stage = "captured output";
    std::ifstream file(path, std::ios::binary);
    const std::vector<char> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    std::error_code removeError;
    std::filesystem::remove(path, removeError);
    Require(bytes.size() % sizeof(std::int16_t) == 0, "partial captured sample");
    std::vector<std::int16_t> captured(bytes.size() / sizeof(std::int16_t));
    std::memcpy(captured.data(), bytes.data(), bytes.size());
    std::size_t first = 0;
    std::size_t last = captured.size();
    while (first < last && captured[first] == 0) ++first;
    while (last > first && captured[last - 1] == 0) --last;
    captured.assign(captured.begin() + static_cast<std::ptrdiff_t>(first), captured.begin() + static_cast<std::ptrdiff_t>(last));
    Require(captured == expected, "captured output");
    std::puts("Audio3d flush tests passed");
}
