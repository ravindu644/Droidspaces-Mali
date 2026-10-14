# 4. libhybris

The Mali driver is an Android library, built against bionic, Android's C library. The container runs glibc. libhybris loads bionic libraries into a glibc process and connects the two.

We build Droidian's `feature/next/lindroid-drm` branch, which already has a TLS patcher and a dma-buf Wayland path, plus the 24 patches in `patches/libhybris/`. They are also the [`feature/mali-desktop`](https://github.com/ravindu644/libhybris/tree/feature/mali-desktop) branch of our fork.

Run everything in this part inside the container, as root, except the shim in step 4. The rest of the guide expects this repository at `~/Droidspaces-Mali` in the container.

## Step 1: Dependencies and sources

```bash
apt-get install autoconf automake autopoint libtool pkg-config build-essential git \
  libwayland-dev libwayland-egl-backend-dev wayland-protocols libegl-dev libgles-dev \
  libdrm-dev libvulkan-dev
git clone --depth 1 https://github.com/ravindu644/Droidspaces-Mali ~/Droidspaces-Mali
mkdir -p /opt/hy && cd /opt/hy
git clone --depth 1 -b herrie/halium-14.0 https://github.com/Halium/android-headers
git clone --depth 1 -b feature/mali-desktop https://github.com/ravindu644/libhybris dhy
```

To apply the patches yourself instead of cloning the fork:

```bash
git init dhy && cd dhy
git config user.name you && git config user.email you@example.com   # git am records a committer
git fetch --depth 1 https://github.com/droidian/libhybris 91f805b604f95089dd1ac476b93c19162490efc3 && git checkout FETCH_HEAD
git am ~/Droidspaces-Mali/patches/libhybris/*.patch
```

Android 14's headers use clang-only annotations that gcc doesn't know. `shim/compat.h` defines them away, and the build includes it into every file:

```bash
cp ~/Droidspaces-Mali/shim/compat.h /opt/hy/compat.h
```

## Step 2: Build

```bash
cd /opt/hy/dhy/hybris
./autogen.sh
./configure --prefix=/opt/hy/inst --enable-arch=arm64 --enable-wayland --enable-lindroid-drm \
  --enable-mali-quirks --enable-debug --enable-property-cache=no \
  --with-android-headers=/opt/hy/android-headers \
  --with-default-hybris-ld-library-path=/vendor/lib64/egl:/vendor/lib64:/system/lib64:/apex/com.android.vndk.v33/lib64
make -j$(nproc) CFLAGS="-O1 -g -DDEBUG -include /opt/hy/compat.h" \
                CXXFLAGS="-O1 -g -DDEBUG -include /opt/hy/compat.h"
make -k install
```

- Change `v33` to your phone's VNDK version.
- `tests/` fails to build against Android 14's headers. That's harmless, and `make -k install` installs everything else.
- Setting `CFLAGS` replaces configure's own, so `-DDEBUG` is added back by hand. Without it, libhybris can't log anything, even with `HYBRIS_LOGGING_LEVEL=debug`.
- After changing `CFLAGS`, run `make clean`. make doesn't notice flag changes.

## Step 3: Library directories for apps and KWin

KWin uses libhybris' EGL. Apps must keep Mesa's EGL and only take Vulkan and OpenCL from libhybris. They get a directory with just those:

```bash
mkdir -p /opt/hy/vk && cd /opt/hy/vk
for l in libvulkan.so.1 libOpenCL.so.1 libhybris-common.so.1 libhardware.so.2 libandroid-properties.so.1; do
    ln -sf /opt/hy/inst/lib/$l .
done
ln -sf libvulkan.so.1 libvulkan.so && ln -sf libOpenCL.so.1 libOpenCL.so
```

KWin gets the opposite, hybris' EGL and GLES without its Vulkan. Newer KWin links Vulkan itself, and hybris' Vulkan crashes it:

```bash
mkdir -p /opt/hy/egl && cd /opt/hy/egl
for l in libEGL.so.1 libGLESv2.so.2 libGLESv1_CM.so.1; do
    ln -sf /opt/hy/inst/lib/$l .
done
```

## Step 4: The gralloc shim

libhybris allocates window buffers through `libui_compat_layer.so`, which is normally built inside an AOSP tree. `shim/ui_compat_ahb.c` is a small replacement on top of `AHardwareBuffer`, which reaches the same gralloc service. Build it on your PC with the NDK. It's a bionic library, so gcc in the container can't build it:

```bash
$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android30-clang \
  -shared -fPIC -O2 -o libui_compat_layer.so ui_compat_ahb.c -lnativewindow -llog
```

Copy it into the container as `/opt/hy/android/libui_compat_layer.so`, after `mkdir -p /opt/hy/android` there. The rootfs is mounted on the phone at `/mnt/Droidspaces/<name>` while the container runs, so `adb push` it to `/data/local/tmp` and copy it from there with `su`.

## Step 5: Check

```bash
LD_LIBRARY_PATH=/opt/hy/vk HYBRIS_PATCH_TLS=1 HYBRIS_VULKANPLATFORM=null \
HYBRIS_LD_LIBRARY_PATH=/opt/hy/android:/vendor/lib64/egl:/vendor/lib64/hw:/vendor/lib64:/system/lib64:/system_ext/lib64:/apex/com.android.vndk.v33/lib64 \
vulkaninfo --summary
```

`vulkaninfo` comes from the `vulkan-tools` package. It should list your Mali GPU with `DRIVER_ID_ARM_PROPRIETARY`. If it crashes or prints `Failed to get pid pointer` in dmesg, see [Troubleshooting](09-troubleshooting.md).

Next: [Mesa](05-mesa.md)
