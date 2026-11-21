#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

extern "C" char* APS5_VABI MkdtempSeeded_nid_no_patch(char* pattern, std::uint64_t seed);

#define Require(value) do { if (!(value)) throw std::runtime_error("mkdtemp check failed at line " + std::to_string(__LINE__)); } while (false)

int main() {
    char invalid[] = "anyps5-mkdtemp-invalid-XXX";
    *__error_nid_postfix() = 0;
    Require(mkdtemp_nid_postfix(invalid) == nullptr && *__error_nid_postfix() == 22);
    char mixed[] = "anyps5-mkdtemp-mixed-XXXXXY";
    *__error_nid_postfix() = 0;
    Require(mkdtemp_nid_postfix(mixed) == nullptr && *__error_nid_postfix() == 22);
    *__error_nid_postfix() = 0;
    Require(mkdtemp_nid_postfix(nullptr) == nullptr && *__error_nid_postfix() == 14);
    static const char readOnly[] = "anyps5-mkdtemp-readonly-XXXXXX";
    *__error_nid_postfix() = 0;
    Require(mkdtemp_nid_postfix(const_cast<char*>(readOnly)) == nullptr && *__error_nid_postfix() == 14);

    const std::string missingParent = "anyps5-mkdtemp-missing-parent";
    std::filesystem::remove_all(missingParent);
    char missing[] = "anyps5-mkdtemp-missing-parent/XXXXXX";
    *__error_nid_postfix() = 0;
    Require(mkdtemp_nid_postfix(missing) == nullptr && *__error_nid_postfix() == 2);

    const std::string parent = "anyps5-mkdtemp-parent";
    std::filesystem::remove(parent);
    { std::ofstream(parent) << "not a directory"; }
    char underFile[] = "anyps5-mkdtemp-parent/XXXXXX";
    *__error_nid_postfix() = 0;
    Require(mkdtemp_nid_postfix(underFile) == nullptr && *__error_nid_postfix() == 20);
    std::filesystem::remove(parent);

    char first[] = "anyps5-mkdtemp-test-XXXXXX";
    char* created = mkdtemp_nid_postfix(first);
    Require(created == first);
    const std::string path(created);
    Require(path.size() == std::strlen("anyps5-mkdtemp-test-XXXXXX"));
    Require(path.substr(0, path.size() - 6) == "anyps5-mkdtemp-test-");
    for (const char character : path.substr(path.size() - 6))
        Require(std::isalnum(static_cast<unsigned char>(character)) != 0);
    Require(std::filesystem::is_directory(path));

    char second[] = "anyps5-mkdtemp-test-XXXXXX";
    Require(mkdtemp_nid_postfix(second) == second);
    const std::string other(second);
    Require(other != path && std::filesystem::is_directory(other));

    char longer[] = "anyps5-mkdtemp-long-XXXXXXX";
    Require(mkdtemp_nid_postfix(longer) == longer);
    const std::string longPath(longer);
    Require(longPath.substr(0, longPath.size() - 7) == "anyps5-mkdtemp-long-");
    for (const char character : longPath.substr(longPath.size() - 7))
        Require(std::isalnum(static_cast<unsigned char>(character)) != 0);
    Require(std::filesystem::is_directory(longPath));

    const std::string collisionPrefix = "anyps5-mkdtemp-collision-";
    std::filesystem::remove_all(collisionPrefix + "000000");
    std::filesystem::remove_all(collisionPrefix + "100000");
    char seededFirst[] = "anyps5-mkdtemp-collision-XXXXXX";
    Require(MkdtempSeeded_nid_no_patch(seededFirst, 0) == seededFirst);
    Require(std::string(seededFirst) == collisionPrefix + "000000");
    char seededRetry[] = "anyps5-mkdtemp-collision-XXXXXX";
    Require(MkdtempSeeded_nid_no_patch(seededRetry, 0) == seededRetry);
    Require(std::string(seededRetry) == collisionPrefix + "100000");
    Require(std::filesystem::is_directory(seededFirst));
    Require(std::filesystem::is_directory(seededRetry));

    std::filesystem::remove_all(path);
    std::filesystem::remove_all(other);
    std::filesystem::remove_all(longPath);
    std::filesystem::remove_all(seededFirst);
    std::filesystem::remove_all(seededRetry);
    return 0;
}
