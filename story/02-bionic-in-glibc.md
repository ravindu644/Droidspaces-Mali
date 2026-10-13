# 2. Bionic in a glibc world

The first container was Ubuntu 24.04, because it had more memory. We didn't use Droidspaces' GPU option yet. A small tool cloned the host's `/vendor`, `/system`, `/apex`, `/dev/__properties__`, `/dev/mali0` and `/dev/dma_heap` into the running container's mount namespace with `open_tree()` and `move_mount()`.

Then we built libhybris and ran the same `eglprobe`, this time as a glibc program.

## Building libhybris against Android 14

| Error | Cause | Fix |
|---|---|---|
| `hardware_buffer.h: expected ':' ... before 'data'` | Android 14 headers use clang's nullability keywords (`_Nullable`) | a forced include, `compat.h`, that defines them away |
| `sync.h: expected declaration specifiers before '__INTRODUCED_IN'` | bionic's availability macros | the same `compat.h`. Passing `-D__INTRODUCED_IN(x)=` on the command line breaks the shell quoting |
| `wayland-egl-backend.h: No such file` | Debian splits the header into its own package | `libwayland-egl-backend-dev` |

## The crashes, one after another

**1. Segfault before any output, on libhybris master.** Android 14's libc++ calls bionic's `__ctype_get_mb_cur_max()`, which reads the thread's bionic TLS block from `tpidr_el0[-1]`. Under glibc, that slot belongs to glibc, so the read hit NULL plus 0x820.

We switched to Droidian's `feature/next/lindroid-drm` branch, which has a TLS patcher. Same crash. That patcher only rewrites the driver's reads of slots 3 and 4, where Android keeps the GL context. Bionic's own reads of slot 1 and slot -1 are untouched.

How big was the problem? A script over `llvm-objdump` of bionic's `libc.so`, `libm.so` and `libdl.so` found that 339 of the 433 bionic functions libhybris doesn't replace can reach a TLS read. Halium and Sailfish never hit this, because they ship their own `/system` with a patched bionic. We use Samsung's stock one.

**Fix, libhybris patch 1:** rewrite every `mrs xN, tpidr_el0` in stock bionic into `adrp xN, fake_tcb`. The fake thread control block is placed within `adrp` range of the library. Its slot 1 points to a fake `pthread_internal_t` and its slot -1 to a fake `bionic_tls`. That patched 836 reads in `libc.so` and 47 in `libm.so`. The known cost is one fake thread shared by all threads, so bionic's internal errno and scratch buffers race. Nobody reads them, because errno goes through libhybris' own `__errno` hook.

**2. `failed to open gralloc module with both version 0 and 1 methods`, then an assertion.** libhybris' null EGL platform, the one without windows, initialised gralloc anyway. This phone has only the gralloc 4 HIDL service, which libhybris' gralloc layer can't open. **Patch 2:** the null platform doesn't touch gralloc.

**3. Segfault in glibc's `__vfscanf_internal`, called from the driver's `sscanf("r0p1", "r%up%u")`.** With the GL slot patcher off, the driver wrote its GL context to `tp+24` and `tp+32`, which in glibc is static TLS. It had overwritten glibc's locale pointer. **Fix:** `HYBRIS_PATCH_TLS=1`.

**4. A crash in `libGLES_meow.so`.** Android's `libEGL` loads MediaTek's wrapper, because `ro.hardware.egl=meow`, and the wrapper reads TLS slot 3 with a register offset the patcher doesn't handle. **Fix:** `LIBEGL` and `LIBGLESV2` point libhybris straight at `libGLES_mali.so`, so neither Android's `libEGL` nor the wrapper is ever loaded.

**5. `Null anb`, then a segfault in `eglCreateImageKHR`.** "Null anb" was a red herring. A gdb script on the driver's `osup_thread_data_get()` showed that the driver keeps its per-thread EGL state behind `pthread_getspecific(key)`, and its key was 0. glibc hands out key 0 to the first caller in a fresh process. In bionic, key 0 is never valid, and libhybris' `pthread_getspecific` hook returns NULL for it. So the driver rebuilt empty thread state on every call. **Patch 3:** `pthread_key_create` never returns key 0.

**6. `Failed creating base context during initializing of kernel context flags`.** strace showed `KBASE_IOCTL_SET_FLAGS` failing with `ENOMEM`, and dmesg said `Failed to get pid pointer for eglprobe`. That one wasn't libhybris. See [the next chapter](03-the-kernel-bug.md).

To keep moving, we ran the probe in the container's mount namespace but the host's PID namespace, with `nsenter` and a private `/proc`. It passed: the Mali driver rendered into a dma-buf from inside a glibc container.

## Debugging bionic code under gdb

gdb doesn't see libraries loaded by libhybris' own linker. We mapped addresses by hand: find the library whose offset-0 mapping in `info proc mappings` contains the PC, then `llvm-symbolizer --obj=<pulled library> <offset>`. The Mali driver keeps no frame pointers, so we unwound by scanning the stack for return addresses.

Next: [The kernel bug](03-the-kernel-bug.md)
