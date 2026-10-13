# 8. A real render node

Zero-copy Zink needed a render node, and `mediatek_drm` has none. A preload made `card0` look like one to libdrm. That works, but it's a fake device in every app.

vgem is mainline Linux's virtual GEM driver. It provides a render node and nothing else, which is exactly what was missing.

## Building it for a GKI kernel

`CONFIG_DRM_VGEM=m` built a module that failed to load with `Unknown symbol`. The GKI kernel doesn't export two of its imports to vendor modules:

- `drm_clflush_pages()`, which only flushes on x86 and powerpc. On arm64 it prints a warning and does nothing, so the patch skips the call there.
- `vma_set_file()`, which the patch open codes with `get_file()` and `fput()`, both exported.

Before loading, we compared the module's CRCs against all 376 stock vendor modules: 38 shared symbols, no mismatch. `insmod vgem.ko` gave `/dev/dri/renderD128`.

## Wiring it in

- Droidspaces' GPU Access copies `renderD*` nodes into the container. vgem's is a normal character device, so the copy works.
- libdrm's `drmGetDevices2()` skips vgem, a platform device without a device tree node, so KWin's own search fell back to `card0`. The KWin wrapper now names it with `ANLAND_DRM_DEVICE=/dev/dri/renderD128`.
- Apps get `HYBRIS_VK_DRM_NODE=/dev/dri/renderD128`, so Zink matches the Mali to the device KWin advertises.

The preload was deleted. es2gears presented dma-bufs at about 55 fps with correct colours.

Next: [X11](09-x11.md)
