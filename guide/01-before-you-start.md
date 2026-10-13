# 1. Before you start

## What you need

1. **A Mali phone you can build a kernel for.** Two kernel modules need a fix, so you need the phone's kernel source and a way to load your own modules. See [Kernel](02-kernel.md).
2. **Root with KernelSU or Magisk**, and Droidspaces installed.
3. **A Debian 13 container** with about 15 GB of free space. KWin, Xwayland and Mesa are built inside it. 2 GB of memory is enough for daily use, but the builds need more, see [Mesa](05-mesa.md). Debian 13 ships KWin 6.3, which the Anland KWin backend supports.
4. **A Linux PC** with the Android NDK (r27 works) for the gralloc shim, and `adb`.

Building everything on the phone takes a few hours. Most of it is Mesa and KWin.

## Facts to collect from your phone

The paths and names below are from the SM-A166P. Look up your own before you start, and use them wherever the guide uses ours.

| What | How to find it | SM-A166P |
|---|---|---|
| Mali kernel driver and its version | `lsmod \| grep mali`, then the driver's source in the kernel tree | `mali_kbase_mt6835`, r38p1 |
| Where the module is loaded from | `find /vendor_dlkm /vendor/lib/modules -name "mali*.ko"` as root | `/vendor_dlkm/lib/modules/` |
| The Mali driver library | `ls -l /vendor/lib64/egl/` | `/vendor/lib64/egl/libGLES_mali.so` |
| The VNDK APEX | `ls /apex \| grep vndk` | `com.android.vndk.v33` |
| Gralloc | `lshal \| grep allocator` | allocator 4.0 (HIDL) only |
| Android version | `getprop ro.build.version.release` | 14 |

## What changes on another phone

- **The kernel fix.** Find the same lookup in your Mali driver: `grep -rn find_get_pid` in its `context/` and `device/` directories. Most kbase versions have it. The fix in `patches/kernel/0001-*.patch` applies by hand to any of them.
- **The search path.** `HYBRIS_LD_LIBRARY_PATH` in `session/mali-gpu.env` names the VNDK APEX. Change `v33` to yours.
- **The libhybris build.** `--with-default-hybris-ld-library-path` in [libhybris](04-libhybris.md) names the same directories.
- **Older Android.** The TLS patch in libhybris was written for Android 14's bionic. Older bionic is closer to what libhybris already handles, so it may need less.

Everything else, from the Mesa patches to the session, is about the Mali driver itself, not about this phone.

## A word on the paths

Everything we build goes under `/opt/hy` in the container, so nothing in Debian's own directories is replaced by hand:

| Path | What |
|---|---|
| `/opt/hy/inst` | libhybris |
| `/opt/hy/vk` | the libhybris Vulkan and OpenCL libraries, without its EGL, for apps |
| `/opt/hy/android` | the gralloc shim, loaded by libhybris |
| `/opt/hy/bin` | the KWin wrapper and its preload |
| `/opt/hy/libexec` | a copy of KWin without its file capability, see [Session](07-session.md) |
| `/opt/hy/etc/mali-gpu.env` | the environment every app gets |

Mesa, KWin and Xwayland are built as Debian packages and installed with `dpkg`.

Next: [Kernel](02-kernel.md)
