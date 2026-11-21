#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/HostEntropy.hpp"
#include "prx/libc/include/HostErrno.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace {

constexpr char Alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
constexpr std::size_t SuffixLength = 6;
constexpr std::uint64_t AlphabetSize = sizeof(Alphabet) - 1;
constexpr std::uint64_t Attempts = AlphabetSize * AlphabetSize * AlphabetSize * AlphabetSize * AlphabetSize * AlphabetSize;

char* Fail(int error) {
    errno = error;
    return nullptr;
}

}

extern "C" char* APS5_VABI mkdtemp_nid_postfix(char* pattern) {
    if (!pattern) return Fail(14);
    std::string copy;
    if (const int error = CopyGuestPath_nid_no_patch(pattern, copy)) return Fail(error);
    if (copy.size() < SuffixLength) return Fail(22);
    const auto suffix = std::string_view(copy).substr(copy.size() - SuffixLength);
    if (suffix.find_first_not_of('X') != std::string_view::npos) return Fail(22);

    auto* destination = pattern + copy.size() - SuffixLength;
    const GuestArena::HostWrite write(destination, SuffixLength);
    if (!write.Open() || !GuestWritable_nid_no_patch(destination, SuffixLength)) return Fail(14);

    std::uint64_t seed = 0;
    HostEntropy_nid_no_patch(&seed, sizeof(seed));
    std::array<char, SuffixLength> candidate{};
    for (std::uint64_t attempt = 0; attempt < Attempts; ++attempt) {
        auto value = seed + attempt;
        for (std::size_t index = 0; index < SuffixLength; ++index) {
            candidate[index] = Alphabet[value % AlphabetSize];
            value /= AlphabetSize;
        }
        std::memcpy(destination, candidate.data(), candidate.size());
        const auto resolved = ResolvePath_nid_no_patch(pattern);
        std::error_code error;
        if (std::filesystem::create_directory(resolved, error)) {
            std::error_code ignored;
            std::filesystem::permissions(resolved, std::filesystem::perms::owner_all, ignored);
            return pattern;
        }
        if (!error) continue;
        const int guestError = GuestErrnoFromHost_nid_no_patch(error.value());
        if (guestError != 17) return Fail(guestError);
    }
    return Fail(17);
}
