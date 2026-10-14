# 11. Fedora 44

The Debian setup is a hand build. The next step was a rootfs anyone could install in one go: Fedora 44, with every package built by CI, and a tarball that brings its own container settings. Most of it worked. The browsers did not, and that decided it: Debian 13 stays the supported base. This chapter records what Fedora took and where it stopped, so the next attempt starts here.

The work is kept, unmerged, on the [`fedora-packages`](https://github.com/ravindu644/Droidspaces-Mali/tree/fedora-packages) branch of this repository and the `feat/fedora-mali` branch of the Droidspaces rootfs builder. Its packages are the [`fedora-44`](https://github.com/ravindu644/Droidspaces-Mali/releases/tag/fedora-44) release.

## What was built

- **Packages, built in CI** on GitHub's ARM runners inside `fedora:44` containers:
  - Fedora's own KWin 6.7.5, rebuilt with Anland's backend;
  - libhybris with the session, as `droidspaces-mali`;
  - a Mesa for apps, explained below.
- **A rootfs template** that installs them, version-locks them, creates the user `fedora` and enables Plasma for it.
- **A `container.config` inside the tarball**, so the Droidspaces installer turns on GPU access and the Android partition binds by itself.

## Walls we got past

| Symptom | Cause | Fix |
|---|---|---|
| every app rendered on llvmpipe, with no error | Zink in Mesa 25.2 and later requires `nullDescriptor` from `VK_EXT_robustness2` ([`0a2c3953`](https://gitlab.freedesktop.org/mesa/mesa/-/commit/0a2c3953948874c8d30bac857ce22fd29be0a968)). The Mali has no robustness2 at all, and Fedora ships Mesa 26.2 | a second Mesa, 25.1.9, built with Zink only under `/opt/hy/mesa`. Apps reach it through `/opt/hy/vk`, and Fedora's Mesa stays as it ships |
| Mesa 25.1 didn't build | glibc 2.43 declares C23's `call_once` itself | upstream [`179e744f`](https://gitlab.freedesktop.org/mesa/mesa/-/commit/179e744f7577d98df7c79d7324c22acfb32a0154) |
| KWin found no `/run/user/1000` | `pam_systemd` lives in `systemd-pam`, only a weak dependency on Fedora | install it by name |
| KWin crashed at startup, even for `--version` | KWin 6.7 links `libvulkan` itself, and hybris' `libvulkan` crashes it at load | KWin gets `/opt/hy/egl`, with hybris' EGL and GLES only |
| `Required client extension EGL_KHR_platform_gbm is not supported` | KWin 6.7 always opens its EGL display on a GBM device, even for an offscreen backend like Anland's. hybris sent GBM to Lindroid's window system | a hybris patch: without Lindroid's evdi card, GBM is treated like surfaceless |
| `unknown identifier 'GL_OES_standard_derivatives'` | KWin 6.7.5's shaders test an undefined macro in `#if` and put an `#extension` after `precision` statements. GLSL ES forbids both. Mesa lets it pass, the Mali compiler doesn't | upstream [`21f875de`](https://invent.kde.org/plasma/kwin/-/commit/21f875def7ee210b4920fceb90b379160eb38593) and [`bf74abc6`](https://invent.kde.org/plasma/kwin/-/commit/bf74abc685eb4a2c8654b33f7b24690e9d214f05), backported. Both are in KWin 6.8, which Fedora 44 doesn't ship yet |
| Firefox died at startup with `importing the supplied dmabufs failed` | see below | **libhybris patch 23**, which also fixes Debian |
| the installer failed with `invalid tar magic` | GNU `tar --delete` corrupts the PAX archive BuildKit writes. GNU tar didn't notice, busybox did | rewrite the archive in one pass with Python's `tarfile` |

Two more checks before anything ran on the phone: the installer reads a tarball's `container.config` from its first 64 KiB only, so the builder puts it first, and CI lists the finished tarball with busybox, which is what the app extracts with.

### Windows of a page or less

Firefox's first window is 1x1. hybris sent KWin a buffer KWin couldn't import, and Firefox exited. `vkcube` reproduced it: every window of 4 KiB or less failed (32x32 failed, 33x33 worked, 64x1 failed, 512x8 worked).

A MediaTek gralloc handle holds three fds: `gralloc_extra`, the pixel dma-buf, and `gralloc_shared_memory`. Patch 8 picks the largest fd that answers `lseek(SEEK_END)`. `gralloc_shared_memory` is a memfd of about 4.4 KiB, and a memfd answers `lseek` too. So for any buffer of a page or less, the memfd won. Patch 23 counts only fds on the dma-buf filesystem (`fstatfs` reports `DMA_BUF_MAGIC`). The bug is in the Debian build as well. Plasma's windows are never that small, which is why it never showed there.

## Where it stopped

### Overview and Present Windows crash KWin

Opening either one killed KWin. The crash is inside the Mali driver, in `glTexSubImage2D`, on the first upload into a 512x512 texture that Qt Quick allocates for its atlas. The driver locks a mutex at offset `0x18` of an object whose pointer reads `0x3f80000000000000`: the bit pattern of the float 1.0, not an address. It looks like memory reused after being freed.

Ruled out, one at a time:

| Tried | Result |
|---|---|
| uploading BGRA as RGBA, then allocating the texture as RGBA too | still crashed (a bus error the second time) |
| no robustness or priority in the context attributes | still crashed. An earlier "fix" only worked because the contexts failed with `EGL_BAD_MATCH` and the effect never drew |
| Qt Quick's texture atlas off | still crashed |
| Qt Quick's contexts not shared with KWin's | still crashed |
| Qt's shader caches off | still crashed |
| Qt Quick's software backend for KWin | works, but window previews are empty boxes |

Debian's KWin 6.3 with Qt 6.8 runs the same effects on the same driver and libhybris, so something in the newer KWin or Qt's GL call pattern triggers a driver bug. Finding it means tracing every GL call around the crash.

### Browsers

| Browser | On the GPU | On the CPU |
|---|---|---|
| Firefox 157 | text and icons were white boxes until `gfx.webrender.pbo-uploads=false`. After that, CSS gradients came out as garbage that changed from run to run, and rounded-corner clip masks drew dark strips | correct, with `gfx.webrender.software=true` |
| Brave 1.97 | the GPU process aborts. ANGLE on GLES crashes in Zink, and ANGLE on Vulkan rejects the driver: `The requested version of Vulkan is not supported by the driver` | correct, with `--disable-gpu` |

`ZINK_DEBUG=flushsync`, which makes Zink wait for the GPU after every submit, fixed Firefox's gradient once, but not reliably, and not the clip masks. Zink itself warns at every start that the Mali lacks `logicOp`, `fillModeNonSolid` and `shaderClipDistance`, and that incorrect rendering may follow. A browser engine is the kind of program that finds those gaps.

## Conclusion

Debian 13 stays the base. Its Mesa 25.0 and KWin 6.3 predate both requirements that broke Fedora, and the guide is built on it.

Newer distributions run into the Mali's Vulkan feature set. Zink has moved on to requiring features this driver doesn't have, and at least one driver memory bug shows up under newer KWin and Qt. Everything on the KWin side could be fixed by backporting upstream commits. What's left needs one of these:

- `nullDescriptor` emulated in libhybris' Vulkan layer, so current Mesa runs;
- the GL call pattern that breaks the driver in Qt Quick, found and avoided;
- the Zink features browsers depend on, emulated or worked around.

Lessons from this round:

- **A silent fallback is the worst failure.** Mesa dropped to llvmpipe without a word, and the desktop looked fine. Check the renderer string and `/proc/<pid>/maps` after every change to the stack.
- **"It survived" is not "it works".** A test passed because the feature under test failed to start. Check the log for the feature's own errors before believing a pass.
- **Read the source when a library goes quiet.** Zink's reasons for rejecting a device are `debug_printf` calls, compiled out of release builds. Diffing Zink 25.0 against 26.2 found the requirement in minutes.
