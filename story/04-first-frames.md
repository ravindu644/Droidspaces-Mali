# 4. First frames

The goal of this step was a Mali-rendered frame on the phone's screen, without a compositor yet.

## The container, configured properly

We dropped the mount injection tool and set the container up through `container.config`: **GPU Access** for `/dev/mali0` and `/dev/dma_heap`, **Anland Display** for the display socket, and read-only binds of `/vendor`, `/system`, `/apex` and `/dev/__properties__`. Binds are recursive, so the APEX submounts come along. `/proc/ged` needs nothing, because it's a global procfs entry.

## The demo

Anland's producer library, `libdisplay_producer`, compiles straight into a test program. Per frame, the demo takes Anland's current buffer, binds it as a framebuffer, renders a triangle on the Mali, commits it and waits for Anland to release a buffer.

**`no Anland daemon answered`.** `anland_device_open(NULL)` doesn't look at `/run/display.sock`. `ANLAND_SOCKET=/run/display.sock` fixed it.

**Connected, then `import of fb 0 failed 0x3003` (`EGL_BAD_ALLOC`).** strace showed that the received fd answered `lseek(SEEK_END)` with `ESPIPE`, and the kbase import ioctl returned `ENOMEM`. It wasn't a dma-buf at all.

The cause was in the Anland app. `collect_dmabufs()` sent `handle->data[0]`, assuming the first fd of a gralloc handle is the pixels. On Qualcomm it is. On MediaTek's gralloc 4, the handle is `[gralloc_extra metadata, the pixel dma-buf, a shared memory fd]`, and `data[0]` is `anon_inode:gralloc_extra`.

**Fix, Anland patch 1:** send the first fd that `lseek()` reports as large enough for the image. Only dma-bufs report a size. libhybris later hit the same handle layout, see [Vulkan and Zink](06-vulkan-and-zink.md).

With the fixed app, the triangle spun on the phone screen at a steady 60.1 fps.

## Installing a patched Anland app

- The patched debug build can't be installed over the official one, because the signing keys differ. Uninstall first. That wipes the app's settings.
- The app pins NDK 29. Building with NDK 27 works.
- Use the `plain` flavour. `tracy` is a profiling build.
- After a fresh install the app sits on its settings screen until its first-run setup is done.

Next: [Plasma on the Mali](05-plasma-on-the-mali.md)
