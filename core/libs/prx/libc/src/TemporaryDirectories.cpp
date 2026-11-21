#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/HostEntropy.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <new>
#include <string>
#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace {

constexpr char Alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
constexpr std::size_t MinimumSuffix = 6;
constexpr std::uint64_t AlphabetSize = sizeof(Alphabet) - 1;
constexpr std::uint64_t Attempts = AlphabetSize * AlphabetSize * AlphabetSize * AlphabetSize * AlphabetSize * AlphabetSize;

char* Fail(int error) {
    errno = error;
    return nullptr;
}

std::size_t TrailingXCount(const std::string& value) {
    std::size_t count = 0;
    while (count < value.size() && value[value.size() - count - 1] == 'X') ++count;
    return count;
}

char* MakeTemporaryDirectory(char* pattern, const std::string& copy, std::size_t suffixLength, std::uint64_t seed) {
    auto* destination = pattern + copy.size() - suffixLength;
    const GuestArena::HostWrite write(destination, suffixLength);
    if (!write.Open() || !GuestWritable_nid_no_patch(destination, suffixLength)) return Fail(14);

    std::string candidate(suffixLength, '0');
    for (std::uint64_t attempt = 0; attempt < Attempts; ++attempt) {
        auto value = seed + attempt;
        for (std::size_t index = 0; index < suffixLength; ++index) {
            candidate[index] = Alphabet[value % AlphabetSize];
            value /= AlphabetSize;
        }
        std::memcpy(destination, candidate.data(), candidate.size());
        std::filesystem::path resolved;
        if (const int error = ResolvePathError_nid_no_patch(pattern, resolved, false, true)) return Fail(error);
        if (HasNonDirectoryPrefix_nid_no_patch(resolved)) return Fail(20);
        std::error_code error;
#ifdef _WIN32
        if (std::filesystem::create_directory(resolved, error)) return pattern;
#else
        if (::mkdir(resolved.c_str(), 0700) == 0) return pattern;
        error = std::error_code(errno, std::generic_category());
#endif
        if (!error) continue;
        const int guestError = FilesystemError(error);
        if (guestError == 17) continue;
        return Fail(guestError);
    }
    return Fail(17);
}

}

extern "C" char* APS5_VABI MkdtempSeeded_nid_no_patch(char* pattern, std::uint64_t seed) {
    if (!pattern) return Fail(14);
    try {
        std::string copy;
        if (const int error = CopyGuestPath_nid_no_patch(pattern, copy)) return Fail(error);
        const auto suffixLength = TrailingXCount(copy);
        if (suffixLength < MinimumSuffix) return Fail(22);
        return MakeTemporaryDirectory(pattern, copy, suffixLength, seed);
    } catch (const std::bad_alloc&) {
        return Fail(12);
    }
}

extern "C" char* APS5_VABI mkdtemp_nid_postfix(char* pattern) {
    if (!pattern) return Fail(14);
    std::uint64_t seed = 0;
    HostEntropy_nid_no_patch(&seed, sizeof(seed));
    return MkdtempSeeded_nid_no_patch(pattern, seed);
}
