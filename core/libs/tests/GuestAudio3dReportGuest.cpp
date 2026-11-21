#include "prx/libSceAudio3d/include/Audio3d.hpp"
#include <array>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "Audio3d report guest: %s\n", message);
    std::abort();
}

int callReports = 0;

void APS5_VABI ReportHandler(const Audio3dReport*, void*) { ++callReports; }

struct ReportGate {
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false;
    bool release = false;
    int calls = 0;
};

void APS5_VABI BlockingReportHandler(const Audio3dReport*, void* data) {
    auto& gate = *static_cast<ReportGate*>(data);
    std::unique_lock lock(gate.mutex);
    ++gate.calls;
    gate.entered = true;
    gate.condition.notify_all();
    gate.condition.wait(lock, [&] { return gate.release; });
}

void APS5_VABI CountingReportHandler(const Audio3dReport*, void* data) {
    auto& gate = *static_cast<ReportGate*>(data);
    std::lock_guard lock(gate.mutex);
    ++gate.calls;
}

bool WaitForEntry(ReportGate& gate) {
    std::unique_lock lock(gate.mutex);
    return gate.condition.wait_for(lock, std::chrono::seconds(2), [&] { return gate.entered; });
}

void Release(ReportGate& gate) {
    {
        std::lock_guard lock(gate.mutex);
        gate.release = true;
    }
    gate.condition.notify_all();
}

bool ReturnedBeforeRelease(std::future<int>& result) {
    return result.wait_for(std::chrono::milliseconds(300)) == std::future_status::ready;
}

bool CheckUnregisterDuringHandler() {
    ReportGate gate;
    std::uint64_t handle = 0;
    Require(sceAudio3dReportRegisterHandler(0, BlockingReportHandler, &gate, &handle) == 0, "gated handler registration failed");
    std::thread reporter([] { sceAudio3dPortGetStatus(1, nullptr, nullptr, 0); });
    const bool entered = WaitForEntry(gate);
    std::promise<int> promise;
    auto result = promise.get_future();
    std::thread unregisterer([&] { promise.set_value(sceAudio3dReportUnregisterHandler(handle)); });
    const bool early = entered && ReturnedBeforeRelease(result);
    Release(gate);
    unregisterer.join();
    reporter.join();
    const int unregistered = result.get();
    Require(entered, "the in-flight report did not reach the handler");
    Require(unregistered == 0, "unregister of the gated handler failed");
    sceAudio3dPortGetStatus(1, nullptr, nullptr, 0);
    std::lock_guard lock(gate.mutex);
    Require(gate.calls == 1, "an unregistered handler received a later report");
    return early;
}

bool CheckUnregisterAfterSelection() {
    ReportGate blocker;
    ReportGate target;
    std::uint64_t blockerHandle = 0;
    std::uint64_t targetHandle = 0;
    Require(sceAudio3dReportRegisterHandler(0, BlockingReportHandler, &blocker, &blockerHandle) == 0, "blocking handler registration failed");
    Require(sceAudio3dReportRegisterHandler(0, CountingReportHandler, &target, &targetHandle) == 0, "target handler registration failed");
    std::thread reporter([] { sceAudio3dPortGetStatus(1, nullptr, nullptr, 0); });
    const bool entered = WaitForEntry(blocker);
    std::promise<int> promise;
    auto result = promise.get_future();
    std::thread unregisterer([&] { promise.set_value(sceAudio3dReportUnregisterHandler(targetHandle)); });
    const bool early = entered && ReturnedBeforeRelease(result);
    Release(blocker);
    unregisterer.join();
    reporter.join();
    const int unregistered = result.get();
    Require(entered, "the selected dispatch did not reach its first handler");
    Require(unregistered == 0, "unregister of the selected handler failed");
    {
        std::lock_guard lock(target.mutex);
        Require(target.calls == 1, "the selected target handler was not invoked");
    }
    Require(sceAudio3dReportUnregisterHandler(blockerHandle) == 0, "blocking handler cleanup failed");
    sceAudio3dPortGetStatus(1, nullptr, nullptr, 0);
    std::lock_guard lock(target.mutex);
    Require(target.calls == 1, "an unregistered handler received a later report");
    return early;
}

template <typename Action>
void RequireThrows(Action action, const char* message) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, message);
}

struct ProtectedPages {
    std::uint8_t* bytes;
    std::size_t size;

    ProtectedPages() {
#if defined(_WIN32)
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        bytes = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, size * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        Require(bytes != nullptr, "VirtualAlloc failed");
        DWORD previous = 0;
        Require(VirtualProtect(bytes + size, size, PAGE_NOACCESS, &previous) != 0, "guard page failed");
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0, "page size unavailable");
        size = static_cast<std::size_t>(pageSize);
        void* mapping = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapping != MAP_FAILED, "mmap failed");
        bytes = static_cast<std::uint8_t*>(mapping);
        Require(mprotect(bytes + size, size, PROT_NONE) == 0, "mprotect guard failed");
#endif
    }

    void MakeReadOnly() {
#if defined(_WIN32)
        DWORD previous = 0;
        Require(VirtualProtect(bytes, size, PAGE_READONLY, &previous) != 0, "read-only guard failed");
#else
        Require(mprotect(bytes, size, PROT_READ) == 0, "read-only guard failed");
#endif
    }

    ~ProtectedPages() {
#if defined(_WIN32)
        VirtualFree(bytes, 0, MEM_RELEASE);
#else
        munmap(bytes, size * 2);
#endif
    }
};

}

int main() {
    std::uint64_t invalid = 7;
    Require(sceAudio3dReportRegisterHandler(4, ReportHandler, nullptr, &invalid) == -4 && invalid == 7, "invalid report type order changed");
    Require(sceAudio3dReportRegisterHandler(3, nullptr, nullptr, &invalid) == -2 && invalid == 7, "null report handler order changed");

    errno = 71;
    RequireThrows([&] {
        sceAudio3dReportRegisterHandler(3, ReportHandler, nullptr, reinterpret_cast<std::uint64_t*>(0x10));
    }, "unmapped output accepted");
    Require(errno == 71, "unmapped output changed errno");

    std::array<std::uint64_t, 8> handles{};
    for (auto& handle : handles) Require(sceAudio3dReportRegisterHandler(3, ReportHandler, nullptr, &handle) == 0, "slot leaked after invalid guest output");
    for (std::size_t i = 1; i < handles.size(); ++i) Require(handles[i] == handles[0] + i * 0x10, "report handle order changed");
    Require(sceAudio3dReportRegisterHandler(3, ReportHandler, nullptr, &invalid) == -3 && invalid == 0xffffffffu, "full report slots did not return their measured handle");
    for (auto handle : handles) Require(sceAudio3dReportUnregisterHandler(handle) == 0, "handle cleanup failed");
    Require(sceAudio3dReportUnregisterHandler(handles[0]) == -1, "double unregister accepted");

    ProtectedPages pages;
    auto* atEnd = reinterpret_cast<std::uint64_t*>(pages.bytes + pages.size - sizeof(std::uint64_t));
    Require(sceAudio3dReportRegisterHandler(3, ReportHandler, nullptr, atEnd) == 0, "valid output at guard-page end rejected");
    Require(sceAudio3dReportUnregisterHandler(*atEnd) == 0, "guard-page handle did not unregister");
    auto* crossBoundary = reinterpret_cast<std::uint64_t*>(pages.bytes + pages.size - sizeof(std::uint64_t) + 1);
    RequireThrows([&] { sceAudio3dReportRegisterHandler(3, ReportHandler, nullptr, crossBoundary); }, "cross-page output accepted");
    auto* readOnly = reinterpret_cast<std::uint64_t*>(pages.bytes);
    *readOnly = 0x123456789abcdef0ULL;
    pages.MakeReadOnly();
    RequireThrows([&] { sceAudio3dReportRegisterHandler(3, ReportHandler, nullptr, readOnly); }, "read-only output accepted");
    Require(*readOnly == 0x123456789abcdef0ULL, "read-only output modified");
    Require(errno == 71, "guard-page operations changed errno");

    std::uint64_t handle = 0;
    Require(sceAudio3dReportRegisterHandler(3, ReportHandler, nullptr, &handle) == 0, "guard-page failures leaked slots");
    Require(handle == handles[0], "guard-page failures changed first free slot");
    Require(sceAudio3dReportUnregisterHandler(handle) == 0, "final report cleanup failed");
    Require(sceAudio3dReportRegisterHandler(3, ReportHandler, nullptr, nullptr) == 0, "null optional handle rejected");
    Require(sceAudio3dReportUnregisterHandler(handle) == 0, "null-output registration cleanup failed");

    RequireThrows([&] {
        sceAudio3dReportRegisterHandler(0, ReportHandler, nullptr, reinterpret_cast<std::uint64_t*>(0x10));
    }, "unmapped call-handler output accepted");
    const int invalidPort = static_cast<int>(0x80EA0002u);
    Require(sceAudio3dPortGetStatus(1, nullptr, nullptr, 0) == invalidPort && callReports == 0,
        "rejected call handler remained registered");
    Require(sceAudio3dReportRegisterHandler(0, ReportHandler, nullptr, &handle) == 0, "valid call-handler registration failed");
    Require(sceAudio3dPortGetStatus(1, nullptr, nullptr, 0) == invalidPort && callReports == 1,
        "registered call handler did not receive a report");
    Require(sceAudio3dReportUnregisterHandler(handle) == 0, "call-handler cleanup failed");
    Require(sceAudio3dPortGetStatus(1, nullptr, nullptr, 0) == invalidPort && callReports == 1,
        "unregistered call handler received a report");
    Require(errno == 71, "call-handler failures changed errno");

    const bool earlyDuring = CheckUnregisterDuringHandler();
    const bool earlyAfter = CheckUnregisterAfterSelection();
    std::fprintf(stderr, "earlyDuring=%d earlyAfter=%d\n", earlyDuring ? 1 : 0, earlyAfter ? 1 : 0);
    Require(!earlyDuring, "unregister returned while the handler was still executing");
    Require(!earlyAfter, "unregister returned before the selected handler was invoked");

    std::puts("Audio3d report guest handle tests passed");
}
