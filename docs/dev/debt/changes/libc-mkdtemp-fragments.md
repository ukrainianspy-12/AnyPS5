# libc

## add Functional

- [mkdtemp](../../../core/libs/prx/libc/src/TemporaryDirectories.cpp) (libc) - replaces every trailing `X` after at least six with alphanumeric characters from host entropy and creates the directory with mode `0700` at creation, following POSIX and the FreeBSD `mkdtemp(3)` description; each candidate resolves the full guest path, retries `EEXIST`, and maps host failures through `FilesystemError`. Fewer than six trailing `X` characters returns `EINVAL`, an unwritable guest template returns `EFAULT`, and a non-directory parent returns `ENOTDIR` as host-safety fallbacks. The NID `Qn+tBdUwoBs` is computed from the exported name; console behaviour on those fallbacks and on collision exhaustion is not measured.
