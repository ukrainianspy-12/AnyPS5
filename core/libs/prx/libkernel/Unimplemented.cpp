#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <future>
#include <memory>
#include <pthread.h>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {

constexpr int kRforkProc = 1 << 4;
constexpr int kRforkMem = 1 << 5;

struct RforkThreadArguments {
    int (APS5_VABI* Function)(void*);
    void* Argument;
    std::promise<int> Child;
};

void* RforkThreadStart(void* opaque) {
    std::unique_ptr<RforkThreadArguments> arguments(static_cast<RforkThreadArguments*>(opaque));
#ifdef _WIN32
    arguments->Child.set_value(static_cast<int>(GetCurrentThreadId()));
#else
    arguments->Child.set_value(0);
#endif
    (void)arguments->Function(arguments->Argument);
    return nullptr;
}

}

extern "C" {

int APS5_VABI physhm_open_nid_postfix(void) {
    NotImplemented_nid_no_patch("physhm_open");
    return 0;
}

int APS5_VABI physhm_unlink_nid_postfix(void) {
    NotImplemented_nid_no_patch("physhm_unlink");
    return 0;
}

int APS5_VABI rfork_thread_nid_postfix(int flags, void* stack, int (APS5_VABI* function)(void*), void* argument) {
    if ((flags & (kRforkProc | kRforkMem)) != (kRforkProc | kRforkMem) || stack == nullptr || function == nullptr)
        NotImplemented_nid_no_patch(__func__);
#ifdef _WIN32
    auto* const top = static_cast<char*>(stack);
    MEMORY_BASIC_INFORMATION region{};
    if (VirtualQuery(top - 1, &region, sizeof(region)) == 0)
        NotImplemented_nid_no_patch(__func__);
    auto* const base = static_cast<char*>(region.BaseAddress);
    if (top <= base || top != base + region.RegionSize)
        NotImplemented_nid_no_patch(__func__);
#else
    NotImplemented_nid_no_patch(__func__);
#endif
    auto arguments = std::make_unique<RforkThreadArguments>();
    arguments->Function = function;
    arguments->Argument = argument;
    auto child = arguments->Child.get_future();
    pthread_attr_t attributes{};
    const int initialized = pthread_attr_init(&attributes);
    if (initialized != 0)
        throw std::system_error(initialized, std::generic_category(), "Initializing rfork thread attributes");
#ifdef _WIN32
    int created = pthread_attr_setstack(&attributes, base, region.RegionSize);
#else
    int created = 0;
#endif
    pthread_t thread{};
    if (created == 0) {
        created = pthread_create(&thread, &attributes, RforkThreadStart, arguments.get());
        if (created == 0) {
            pthread_detach(thread);
            arguments.release();
        }
    }
    pthread_attr_destroy(&attributes);
    if (created != 0)
        throw std::system_error(created, std::generic_category(), "Creating rfork thread");
    return child.get();
}

int APS5_VABI sceKernelRdup(int pid, int fd) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("aAQZSjb2fho", sceKernelUnknown16);
int APS5_VABI sceKernelUnknown16(void) {
    NotImplemented_nid_no_patch("aAQZSjb2fho");
    return 0;
}

int APS5_VABI sceKernelPrintBacktraceWithModuleInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelProtectDirectMemoryForPID(int64_t offset, size_t len, int prot, int pid) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelStreamWriteActivate(void) {
    return SCE_KERNEL_ERROR_EBADF;
}

int APS5_VABI sceKernelStreamWriteCreate(void) {
    return SCE_KERNEL_ERROR_ENOSYS;
}

int APS5_VABI sceKernelStreamWriteDeactivate(void) {
    return SCE_KERNEL_ERROR_EBADF;
}

int APS5_VABI sceKernelStreamWriteDelete(void) {
    return SCE_KERNEL_ERROR_EBADF;
}

int APS5_VABI sceKernelStreamWriteRead(void) {
    return SCE_KERNEL_ERROR_EBADF;
}

int APS5_VABI sceKernelStreamWriteStat(void) {
    return SCE_KERNEL_ERROR_EBADF;
}

int APS5_VABI sceKernelStreamWriteWrite(void) {
    return SCE_KERNEL_ERROR_EBADF;
}

}

extern "C" {

int APS5_VABI __Ux86_64_setcontext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
