# 5. Plasma on the Mali

## Why Debian 13

Anland's KDE backend supports KWin 6 only. Ubuntu 24.04 ships Plasma 5.27. Debian 13 ships KWin 6.3, which takes the backend's pre-6.6 path: a surfaceless EGL context, and Anland's dma-bufs imported as textures. So the work moved to a Debian 13 container with 2 GB of memory.

## Building

Anland's `producers/kde/Debian13_v5/build.sh` rebuilds Debian's KWin and Xwayland with the backend. With 7 jobs and the memory limit raised to 4 GB for the build, it took about 50 minutes. Memory peaked at the limit and swapped, but nothing was killed.

Two traps:

- `kwin` in that directory is a symlink to the shared backend sources, and the script copies through it. An `rm -rf` of a hand-prepared tree followed the symlink and deleted the backend sources once.
- To resume a stopped build without losing objects, run `dpkg-buildpackage -b -uc -us -d -nc` in the source tree.

libhybris was rebuilt for Debian's glibc with one more patch: `EGL_MESA_platform_surfaceless` on libhybris' null platform, which KWin asks for.

## KWin needs a DRM device

The backend refuses to start without one. It tries `$ANLAND_DRM_DEVICE`, then render nodes. This phone has no render node, so we bound `/dev/dri/card0` in and named it. KWin opens a GBM device on it but does no modesetting through it.

## The failures

| Symptom | Cause | Fix |
|---|---|---|
| `waiting for display daemon` | the Anland app wasn't showing the display | open it. KWin connects by itself |
| `couldn't find dev node for drm device`, `eglQueryDmaBufFormatsEXT failed` | libhybris answered neither query | harmless for the output. Apps fell back to shared memory buffers. Fixed later |
| Konsole: `Program to run not set` | no `SHELL` in a `droidspaces run` environment | a real login session has it |
| As a normal user, KWin silently ran on Mesa: no libhybris in its maps | Debian ships `/usr/bin/kwin_wayland` with `cap_sys_nice=ep`, and file capabilities make glibc ignore `LD_LIBRARY_PATH` and `LD_PRELOAD`. As root it worked, because the capability changes nothing for root | run a copy without the capability, made with `cp`, which drops the xattr. KWin loses realtime scheduling |
| `Could not load the Qt platform plugin "wayland-org.kde.kwin.qpa"` | KWin's own Qt platform plugin loads only in an executable whose path ends in `kwin_wayland`. The first copy was `kwin_wayland.nocaps` | keep the copy named `kwin_wayland`, in its own directory |
| `failed to start audio engine` | no PipeWire yet | fixed later, see [Daily driving](10-daily-driving.md) |

The desktop came up. KWin composited on the Mali, apps still rendered on the CPU with llvmpipe, and touch worked.

## The fade blink

Closing a window made it blink several times before it vanished, and the Plasma splash blinked about 10 times. Everything else was smooth.

It wasn't GPU sync. The native fence from `EGL_ANDROID_native_fence_sync` was valid on every frame, and `ANLAND_SKIP_IMPLICIT_SYNC_WAIT`, copied from Anland's Turnip setup, isn't read by this backend at all.

Two log lines settled it: the buffer slot the backend rendered into, and the slot it committed. Paired up with awk, 2937 renders faced 3056 commits. 119 commits had no render, and 100 of them committed a different slot from the last one painted: render slot 0, then commit 1, 2, 3, 4, 5.

When Anland hands back a slot that still owes damage, the backend schedules a catch-up frame. On KWin 6.6 and later it marks the layer dirty. On older KWin it only woke the render loop, so KWin presented without painting, and the slot's stale content went to the screen. Each stale slot held the fading window at an older opacity: one blink per buffer.

**Fix, Anland patch 2:** add the owed region as a repaint before waking the loop. After it, 2396 renders and 2396 commits.

### Iterating on KWin quickly

A full package build takes most of an hour. For backend work, edit under `src/backends/anland/` in the build tree, run `make -j7 kwin` in `obj-aarch64-linux-gnu` (minutes, not an hour), strip `bin/libkwin.so.6.3.6` into `/usr/lib/aarch64-linux-gnu/`, and restart the session.

Next: [Vulkan and Zink](06-vulkan-and-zink.md)
