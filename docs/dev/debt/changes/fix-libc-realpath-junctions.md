# libc

## change [realpath]

- [realpath](../../../core/libs/prx/libc/src/RealPath.cpp) (libc) follows FreeBSD 11's [`realpath.c`](https://github.com/freebsd/freebsd-src/blob/stable/11/lib/libc/stdlib/realpath.c) with two gaps. A host symbolic link with a relative target under the guest root is expanded as `realpath1` does (a dangling link fails with `ENOENT`, more than 32 links with `ELOOP`); absolute targets whose host path lies under the guest root or a path alias are now expanded through `GuestPathForHost_nid_no_patch`, including paths below the link and across `..`; an absolute target outside those roots stays unexpanded. Windows junctions use the same mapping. Expansion works on the guest path, so a host link whose text climbs above a path alias root is resolved against the guest tree, not the host one. Otherwise `..` drops the previous component lexically, as `Resolve` does for every guest path.
