# 1. Picking a route

Qualcomm users get native GPU acceleration in containers through Mesa's Turnip driver, which talks to the Adreno kernel driver directly. Mali users had nothing but VirGL, which sends every OpenGL call to a server in Termux.

There were three ways to reach the Mali:

1. **Proxy the Android driver** from the container: VirGL, Venus or Vortek. Droidspaces already supports VirGL, and it's slow.
2. **Load the Android Mali driver inside the container** through libhybris, which lets a glibc program use bionic libraries.
3. **Teach Mesa's open source Mali drivers**, Panfrost and PanVK, to speak to ARM's kernel driver (kbase) instead of the mainline Panfrost one. Too much work for one person.

We took route 2.

## The phone

| | |
|---|---|
| Phone | Samsung Galaxy A16 5G, SM-A166P |
| SoC | MediaTek MT6835 |
| GPU | Mali-G57 MC2, Valhall, job manager (not CSF) |
| Android | 14 |
| Kernel | 5.15 GKI, our own build, with KernelSU-Next |
| Mali kernel driver | `mali_kbase_mt6835.ko`, r38p1, loaded from `/vendor_dlkm` |
| Mali userspace driver | `/vendor/lib64/egl/libGLES_mali.so`, 42 MB: EGL, GLES, Vulkan and OpenCL in one file |
| Gralloc | allocator 4.0 HIDL only |
| DRM | `/dev/dri/card0` only. `mediatek_drm` has no render node |

## Proving the driver can do the job

Before touching a container, we checked on Android itself that the driver can do what a Linux compositor needs: render into a buffer that came from outside, and let someone else read it.

`eglprobe`, built with the NDK and run as root on the phone, creates a surfaceless OpenGL ES 3.2 context, allocates a buffer from `/dev/dma_heap/system`, imports it as an EGL image, renders into it and reads the pixels back. It passed.

`vkprobe` loaded the Vulkan driver through Android's loader: Vulkan 1.3.219, with `VK_EXT_external_memory_dma_buf`, `VK_EXT_image_drm_format_modifier`, external fences and semaphores.

`strace` of `eglprobe` showed everything the driver touches:

- `/dev/mali0`: about 17 kbase ioctls, all stock r38p1, nothing vendor-specific
- `/proc/ged`: MediaTek's GPU extension driver
- `/dev/hwbinder`: one lookup of the gralloc mapper
- `/dev/__properties__`, `/vendor/etc/mali_platform.config`, the log sockets

The binder lookup turned out to come from Android's `libEGL` and MediaTek's wrapper around it, not from the Mali driver. Loaded directly, the driver needs no binder at all. That made route 2 look possible.

Next: [Bionic in a glibc world](02-bionic-in-glibc.md)
