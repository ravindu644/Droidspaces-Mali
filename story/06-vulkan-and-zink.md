# 6. Vulkan and Zink

KWin was on the Mali, but every app still rendered on the CPU. The plan: Vulkan through libhybris, then Mesa's Zink on top of it for OpenGL.

## vulkaninfo

libhybris builds its own `libvulkan.so.1` when Vulkan headers are installed. It wraps Android's Vulkan loader, `/system/lib64/libvulkan.so`, which opens the Mali's Vulkan HAL.

| Symptom | Cause | Fix |
|---|---|---|
| segfault on a NULL `vkGetInstanceProcAddr` | Android's loader failed to load: `libgpud_sys.so` lives in `/system_ext/lib64` | bind `/system_ext`, and add it to `HYBRIS_LD_LIBRARY_PATH` |
| `vulkan.mali.so` not found | the HAL directory wasn't on libhybris' search path | add `/vendor/lib64/hw` |
| `Binder driver could not be opened`, `openat("/dev/hwbinder")` returns `ENXIO` | **a Droidspaces bug.** GPU Access copied the binder nodes with `mknod`, but the host's are binderfs nodes, and a copy of one can never be opened | bind the real nodes. Droidspaces now does that itself (`a91e5a49`) |
| the Wayland platform asserts in gralloc | the same gralloc gap as EGL | `HYBRIS_VULKANPLATFORM=null` for testing |

Then `vulkaninfo` saw `Mali-G57 MC2`, Vulkan 1.3.219, driver `v1.r38p1`.

## vkcube on KWin

vkcube's swapchain images come from gralloc, and libhybris hands them to KWin as dma-bufs. It took a gralloc shim, eight libhybris patches and one SELinux rule.

| Symptom | Cause | Fix |
|---|---|---|
| `failed to allocate gralloc buffer` | libhybris allocates through `libui_compat_layer.so`, which is built inside an AOSP tree | a small replacement on `AHardwareBuffer`, built with the NDK (`shim/ui_compat_ahb.c`) |
| `the server doesn't advertise the android_wlegl global` | libhybris' Wayland path needs a protocol KWin 6 doesn't have | patches 5 and 6: send buffers over the standard `zwp_linux_dmabuf_v1` instead |
| segfault in `zwp_linux_dmabuf_v1_create_params` | buffers were created before the protocol was bound | patch 7 |
| `importing the supplied dmabufs failed` | KWin's table of importable formats was empty: `eglQueryDmaBufFormatsEXT` failed through libhybris | patch 9: ask the Mali driver, which implements it |
| the window was 1x1 and stalled after two frames | Android's swapchain reports the placeholder window size as fixed | patch 10: report "the client chooses" |
| stall in `dequeueBuffer` after a resize | Android's swapchain dequeues every image at creation, while one was still on screen | patch 11, later replaced by patch 20 |
| the window showed TV static | gralloc picked a compressed, tiled layout, and KWin read it as linear | patch 12: allocate with `SW_READ_RARELY`, which makes gralloc choose linear |
| tiny buffers picked the wrong fd | a MediaTek gralloc handle is `[metadata, pixels, shared memory]`, and "large enough" matched the metadata on 1x1 buffers | patch 8: the largest dma-buf in the handle. Patch 23 later fixed it for buffers of a page or less, see [Fedora 44](11-fedora.md#windows-of-a-page-or-less) |
| `Failed to load matching allocator for mapper version 4` | SELinux, see below | one rule |

vkcube then spun at about 59 fps.

### Finding the one SELinux rule

With SELinux permissive, gralloc worked. Under enforcing, it didn't, and nothing showed in logcat: the denial is a `dontaudit`. We started from six candidate rules for `hwservicemanager` and the allocator HAL, then removed them one at a time with `ksud sepolicy patch 'deny ...'`, testing vkcube after each. One rule was left: `allow hwservicemanager droidspacesd binder transfer`. hwservicemanager passes the allocator's binder handle to us. It never calls into us.

## Zink, offscreen

`/opt/hy/vk` holds libhybris' Vulkan and OpenCL libraries without its EGL. With that on `LD_LIBRARY_PATH` and `MESA_LOADER_DRIVER_OVERRIDE=zink`, Mesa keeps its own EGL and GL and runs them on the Mali:

```
OpenGL core profile renderer: zink Vulkan 1.3(Mali-G57 MC2 (ARM_PROPRIETARY))
OpenGL core profile version:  3.1 (Core Profile) Mesa 25.0.7
OpenGL ES profile version:    OpenGL ES 2.0
```

## Zink's draws produced nothing

es2gears came out in inverted colours. A readback probe made it clear: clears were right, but every draw produced nothing, whatever the colour source or vertex source. No Mesa error, and no `ZINK_DEBUG` switch changed it.

A validation layer would have helped, but it can't run here. Registering libhybris as an ICD under the Khronos loader died with a bus error: both loaders store their dispatch pointer in the first word of every dispatchable object.

So we bisected the driver's extensions instead. **Patch 15** adds `HYBRIS_VK_HIDE_EXTS`, which hides extensions from apps. Hiding them one at a time, only `VK_EXT_extended_dynamic_state` fixed it. With it hidden, every probe case matched llvmpipe exactly. Which dynamic state the Mali gets wrong is still open.

## Without copies

Zink first presented through shared memory: one CPU copy per frame. Mesa's Wayland EGL wants a DRM device from the compositor, and Zink then refuses any device without a render node. It also matches its Vulkan device through `VkPhysicalDeviceDrmPropertiesEXT`, which the Mali leaves empty.

- **Patch 13:** `HYBRIS_VK_DRM_NODE` fills in the DRM properties.
- **Patch 14:** map each gralloc format to its real DRM fourcc, instead of always `ABGR8888`.
- A preload made `card0` look like a render node to libdrm. It was a stand-in until vgem, see [A real render node](08-a-real-render-node.md).

es2gears then sent dma-bufs only.

## Wrong colours, twice

The frames were right offscreen, but wrong on screen. A preload that read back each frame at `eglSwapBuffers` showed the rendered pixel was correct, so presentation was at fault.

1. **10-bit configs.** Mesa lists 10:10:10 configs first, and both renderers picked one. Android's swapchain is 8-bit RGBA, so 10-bit rendering landed in 8-bit images. **Fix:** Mesa's `allow_rgb10_configs=false`, set in the environment.
2. **BGRA in RGBA.** Zink's kopper always creates its swapchain as `B8G8R8A8`. Android's swapchain doesn't know BGRA and allocates RGBA buffers, while the Mali writes BGRA bytes into them. libhybris labelled them `ABGR8888`, so red and blue swapped. **Patches 16 and 17:** catch BGRA swapchains and label their buffers `ARGB8888`, which is what the bytes really are.

glmark2-es2-wayland went from 61 on llvmpipe to a partial 124 on the Mali. The run hit a timeout in the buffer scenes, so the score covers only the scenes it reached.

Next: [Zink for the whole session](07-zink-for-the-whole-session.md)
