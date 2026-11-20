# libc

## change [The scanf family](../../../core/libs/prx/libc/include/WindowsScanning.hpp) (libc) on Windows returns 0

- [The scanf family](../../../core/libs/prx/libc/src/Formatting.cpp) (libc) on Windows returns 0, as C17 and FreeBSD do, when input fails after only suppressed conversions have completed. On Linux, formats without a narrow `%c` still go to glibc, which returns `EOF` there. A Windows host stops `%x`, `%X`, `%i` or `%p` after the `x` of a `0x` that no hex digit follows, where C17 and FreeBSD stop before the `x`. A floating conversion whose decimal exponent has no digits (`1e`, `1e+`, `1eZ`) now stops before the `e` for contiguous inputs, as C17 and FreeBSD do: the host directive width covers only the mantissa, leaving the `e` for the next conversion. Stream forms keep their earlier behavior. Other glibc float differences and console behavior remain unmeasured.
