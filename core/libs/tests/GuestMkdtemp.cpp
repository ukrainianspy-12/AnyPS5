#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

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
    char* underResult = mkdtemp_nid_postfix(underFile);
    int underError = *__error_nid_postfix();
    Require(underResult == nullptr && underError != 0);
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

    std::filesystem::remove_all(path);
    std::filesystem::remove_all(other);
    return 0;
}
