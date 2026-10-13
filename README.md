# Droidspaces Mali

Native Mali GPU acceleration for a full Linux desktop in a [Droidspaces](https://github.com/ravindu644/Droidspaces-OSS) container. The phone's own Mali driver, the one Android uses, renders KDE Plasma inside a Debian 13 container, and the [Anland](https://github.com/SuperTurtleDev/anland) app shows it on the phone screen.

Qualcomm devices get this through Mesa Turnip. Mali devices had nothing but VirGL. This repository is the Mali route: the stock driver is loaded into the container through [libhybris](https://github.com/droidian/libhybris), and Mesa's Zink runs OpenGL on top of the driver's Vulkan.

> [!IMPORTANT]
> This was built and tested on one phone, the Samsung Galaxy A16 5G (SM-A166P, MediaTek MT6835, Mali-G57 MC2). The userspace side should carry over to other Mali phones. The kernel side needs a fix for each phone's Mali kernel driver. [Part 1](guide/01-before-you-start.md) lists what changes from phone to phone.

## What works

| Part | Result |
|---|---|
| Compositor | KWin composites on the Mali |
| Wayland apps (Qt, GTK, plasmashell) | OpenGL 3.1 and OpenGL ES 3.1 through Zink, no buffer copies |
| X11 apps | GLX and EGL through Xwayland, also on the Mali |
| Vulkan | Vulkan 1.3 |
| OpenCL | OpenCL 3.0 |
| Session | starts with the container, audio and microphone through Anland |
| Security | SELinux stays enforcing |

glmark2-es2-wayland scores about 133 on the Mali against 61 on llvmpipe. Over X11, glmark2 scores 243, and its refract scene runs at 110 fps against 8 on llvmpipe.

## The stack

```
Android SurfaceFlinger  <-  Anland app  <-  dma-bufs  <-  KWin (Anland backend)
                                                            | hybris EGL
                                                            v
                                          Mali driver (libGLES_mali.so, via libhybris)
                                                            ^
Wayland app  ->  Mesa Zink  ->  hybris Vulkan  -------------+
X11 app      ->  Xwayland   ->  Mesa Zink  ->  hybris Vulkan
```

KWin uses the Mali's EGL directly. Every app goes through Mesa Zink on the Mali's Vulkan driver. Buffers travel between them as dma-bufs.

## Guide

Follow the parts in order.

1. [Before you start](guide/01-before-you-start.md): what you need, and what to change on another phone
2. [Kernel](guide/02-kernel.md): two kernel fixes and how to load them
3. [Android side](guide/03-android-side.md): Droidspaces, SELinux, the container and the Anland app
4. [libhybris](guide/04-libhybris.md): load the Android driver inside the container
5. [Mesa](guide/05-mesa.md): Zink fixes for the Mali
6. [KWin](guide/06-kwin.md): the Anland KWin backend
7. [Session](guide/07-session.md): start Plasma on the GPU at container boot
8. [Check it](guide/08-check-it.md): confirm every process is on the GPU
9. [Troubleshooting](guide/09-troubleshooting.md)

## How we got here

[The story](story/README.md) is the full log, from the first probe to a daily desktop. It covers every wall we hit, why it happened and how we got past it. Read it before you port this to another phone. Most of the failures will look the same.

## Repository layout

| Path | What |
|---|---|
| `patches/kernel/` | Mali kernel driver fix for PID namespaces, vgem as a vendor module |
| `patches/libhybris/` | 22 patches on Droidian's `feature/next/lindroid-drm` |
| `patches/mesa/` | 5 patches on Debian's Mesa 25.0.7 |
| `patches/anland/` | 3 patches on Anland's `legacy` branch |
| `shim/` | the gralloc shim on `AHardwareBuffer`, and the header libhybris needs with Android 14 headers |
| `session/` | the systemd unit, environment, KWin wrapper and its preload |

Each patch series is also a branch on a fork, if you prefer `git clone` to `git am`:

| Project | Fork and branch |
|---|---|
| libhybris | [ravindu644/libhybris](https://github.com/ravindu644/libhybris/tree/feature/mali-desktop) `feature/mali-desktop` |
| Mesa | [ravindu644/mesa-mali](https://github.com/ravindu644/mesa-mali/tree/feature/mali-desktop) `feature/mali-desktop` |
| Anland | [ravindu644/anland](https://github.com/ravindu644/anland/tree/legacy) `legacy` |

## Known limits

- **OpenGL 3.1 and OpenGL ES 3.1 at most.** Geometry shaders are hidden, because the Mali compiler crashes on the ones Zink generates. The Mali also lacks `logicOp`, `fillModeNonSolid`, `shaderClipDistance` and `wideLines`.
- **Linear buffers only.** No AFBC compression, so more memory bandwidth than needed.
- **KWin runs without realtime scheduling.** See [Session](guide/07-session.md).
- **The Overview effect pauses about 300 ms before it opens.** That's KWin building its QML scene on the phone CPU. Upstream reports the same on slow CPUs.
- **Instance divisors above 1** are treated as 1.

## Credits

- [libhybris](https://github.com/libhybris/libhybris) and [Droidian's fork](https://github.com/droidian/libhybris), whose `lindroid-drm` branch carries the TLS patcher and the dma-buf Wayland path this builds on
- [Halium android-headers](https://github.com/Halium/android-headers)
- [Anland](https://github.com/SuperTurtleDev/anland)
- [bionic-vulkan-wrapper](https://github.com/leegao/bionic-vulkan-wrapper), for the dma-heap import idea behind the export emulation
- [Mesa](https://gitlab.freedesktop.org/mesa/mesa)
