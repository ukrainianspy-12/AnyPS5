#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <source_location>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Kernel process export check failed at line %u\n", location.line());
        std::abort();
    }
}

static void* LoadLibraryFile(const std::filesystem::path& path) {
#ifdef _WIN32
    auto* module = reinterpret_cast<void*>(LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH));
#else
    auto* module = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr);
    return module;
}

static void* FindSymbol(void* module, const char* name) {
#ifdef _WIN32
    auto* symbol = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(module), name));
#else
    auto* symbol = dlsym(module, name);
#endif
    if (!symbol) std::fprintf(stderr, "libkernel.prx does not export %s\n", name);
    return symbol;
}

int main(int argc, char** argv) {
    Require(argc == 3);
    const auto directory = std::filesystem::absolute(argv[1]);
    const bool patched = std::strcmp(argv[2], "patched") == 0;
    auto* kernel = LoadLibraryFile(directory / "libkernel.prx");
    struct Export {
        const char* name;
        const char* nid;
    };
    const Export exports[] = {
        {"cpuset_nid_postfix", "itioQwmJGys"},
        {"cpuset_getid_nid_postfix", "DGY7qWLcHGA"},
        {"cpuset_setid_nid_postfix", "BiHHifIvX88"},
        {"rfork_thread_nid_postfix", "bSDxEpGzmUE"},
        {"sceKernelRdup", "LxIacrWiJ3w"},
        {"getpid_nid_postfix", "HoLVWNanBBc"},
        {"close_nid_postfix", "bY-PO6JhzhQ"},
    };
    bool found = true;
    for (const auto& entry : exports) found = FindSymbol(kernel, patched ? entry.nid : entry.name) != nullptr && found;
    Require(found);
    using GetId = int (APS5_VABI*)(int, int, std::int64_t, int*);
    const auto getId = reinterpret_cast<GetId>(FindSymbol(kernel, patched ? "DGY7qWLcHGA" : "cpuset_getid_nid_postfix"));
    constexpr int LevelRoot = 1;
    constexpr int LevelCpuset = 2;
    constexpr int WhichProcess = 2;
    int setid = -1;
    Require(getId(LevelRoot, WhichProcess, -1, &setid) == 0 && setid == 0);
    Require(getId(LevelCpuset, WhichProcess, -1, &setid) == 0 && setid == 1);
    using Self = int (APS5_VABI*)();
    using Rdup = int (APS5_VABI*)(int, int);
    using Close = int (APS5_VABI*)(int);
    const auto self = reinterpret_cast<Self>(FindSymbol(kernel, patched ? "HoLVWNanBBc" : "getpid_nid_postfix"));
    const auto rdup = reinterpret_cast<Rdup>(FindSymbol(kernel, patched ? "LxIacrWiJ3w" : "sceKernelRdup"));
    const auto close = reinterpret_cast<Close>(FindSymbol(kernel, patched ? "bY-PO6JhzhQ" : "close_nid_postfix"));
    Require(self != nullptr && rdup != nullptr && close != nullptr);
    constexpr int StandardOutput = 1;
    const int duplicate = rdup(self(), StandardOutput);
    Require(duplicate >= 0 && duplicate != StandardOutput);
    Require(close(duplicate) == 0);
    Require(rdup(self(), -1) < 0);
    return 0;
}
