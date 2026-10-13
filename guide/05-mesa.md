# 5. Mesa

Apps render through Zink, Mesa's OpenGL driver on top of Vulkan, running on the Mali's Vulkan driver. Debian's Mesa needs five patches for that. We rebuild Debian's own source package with them, so the result installs and upgrades like any other package.

| Patch | Why |
|---|---|
| `0001` zink: don't leak the dma-buf fd when exporting its sync file fails | on kernels before 6.0 the export always fails, and every X11 app died after about 1000 frames. Backported from upstream |
| `0002` zink: only declare GeometryStreams when a shader uses more than stream 0 | backported from upstream |
| `0003` kopper: add `LIBGL_KOPPER_DISABLE_X11` | libhybris has no Vulkan X11 surface. X11 apps present through Mesa's DRI3 path instead, while Wayland apps keep kopper |
| `0004` zink: don't split fragment inputs by component on gaps | the Mali compiler misreads split inputs, and old fixed-function GL games sampled the wrong textures |
| `0005` zink: advertise instance divisors without `VK_EXT_vertex_attribute_divisor` | the Mali lacks the extension, which held OpenGL ES at 2.0. With the patch it's 3.1 |

The patches are tested on Debian's `25.0.7-2+deb13u1`. They are also the [`feature/mali-desktop`](https://github.com/ravindu644/mesa-mali/tree/feature/mali-desktop) branch of our fork, on top of Mesa 25.0.7 with Debian's own patches.

## Step 1: Get the source

Inside the container, as root. Enable source packages first: add `deb-src` to the `Types:` line in `/etc/apt/sources.list.d/debian.sources`.

```bash
apt-get update
apt-get install dpkg-dev devscripts quilt
mkdir -p /root/mesa-src && cd /root/mesa-src
apt-get source mesa
cd mesa-25.0.7
apt-get build-dep -y .
```

## Step 2: Add the patches

```bash
export QUILT_PATCHES=debian/patches
for p in ~/Droidspaces-Mali/patches/mesa/*.patch; do quilt import "$p"; done
quilt push -a
dch --local +mali "zink fixes for the Mali through libhybris"
```

`dch --local` gives the package its own version, `25.0.7-2+deb13u1+mali1`, so you can always tell it apart from Debian's.

## Step 3: Build and install

Each compiler job takes up to 500 MB. Turn **Limit memory** off for the container before you build, and stop Plasma if it's running.

> [!WARNING]
> Don't build inside the memory limit. Droidspaces' limit counts swap too, so the kernel keeps an over-limit build alive by swapping. It fills the phone's swap, Android runs out of memory, and even `droidspaces enter` fails with `Out of memory`.

```bash
DEB_BUILD_OPTIONS="nocheck parallel=7" dpkg-buildpackage -b -uc -us
```

This builds all of Mesa: about an hour on the Galaxy A16, with 7 of its 8 cores. If the build stops halfway, add `-nc` to resume it without starting over. Then install the packages that replace ones you already have, and hold them so `apt upgrade` doesn't swap Debian's back in:

```bash
cd /root/mesa-src
PKGS=$(for d in *.deb; do p=${d%%_*}; dpkg -s "$p" >/dev/null 2>&1 && echo "$p"; done)
dpkg -i $(for p in $PKGS; do ls ${p}_*.deb; done)
apt-mark hold $PKGS
```

> [!NOTE]
> When Debian ships a Mesa security update, `apt` will report the held packages. Rebuild from the new source with the same patches, then install it the same way.

Next: [KWin](06-kwin.md)
