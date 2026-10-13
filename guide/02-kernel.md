# 2. Kernel

Two kernel modules need a change. Neither touches the kernel image itself, so the boot image can stay as it is.

| Patch | Module | Why |
|---|---|---|
| `patches/kernel/0001-*.patch` | the Mali driver (`mali_kbase`) | without it, no process inside a container can use the GPU |
| `patches/kernel/0002-*.patch` | vgem | a DRM render node, which Mesa needs and MediaTek's display driver doesn't provide |

## Step 1: Fix the Mali driver

When a process opens the GPU, kbase looks up the process by its PID. It takes the PID as the host sees it, but searches the caller's PID namespace. Inside a container the two differ, so the lookup fails. The driver then prints `Failed to get pid pointer` to dmesg and refuses to create a GPU context.

The patch takes the caller's thread group leader directly instead (`current->group_leader`). That's the task the lookup was after, and there's no failure path left.

1. Apply it in your kernel tree:

   ```bash
   git am /path/to/Droidspaces-Mali/patches/kernel/0001-*.patch
   ```

   On another phone the path to the driver differs, and `git am` will fail. Apply the change by hand: in `context/mali_kbase_context.c`, replace the `find_get_pid()` and `pid_task()` block with the two lines from the patch. If `device/mali_kbase_device.c` also calls `find_get_pid()`, change it the same way.

2. Build the kernel as usual. The Mali driver is an external module, so it comes out next to the others, for example `out/.../dist/mali_kbase_mt6835.ko`.
3. Strip the debug info:

   ```bash
   llvm-strip --strip-debug mali_kbase_mt6835.ko
   ```

## Step 2: Build vgem

vgem is a small, virtual GEM driver from mainline Linux. It gives the container `/dev/dri/renderD128`.

1. Set `CONFIG_DRM_VGEM=m` in your defconfig.
2. Apply the second patch:

   ```bash
   git am /path/to/Droidspaces-Mali/patches/kernel/0002-*.patch
   ```

   A GKI kernel doesn't export `drm_clflush_pages()` or `vma_set_file()` to vendor modules, so vgem fails to load with `Unknown symbol`. The patch skips the first on arm64, where it does nothing anyway, and open codes the second.
3. Build. The module is `vgem.ko`.

## Step 3: Check the modules against your running kernel

A module only loads when the CRCs of every kernel symbol it uses match the running kernel. The vermagic string doesn't matter. Compare against the stock module:

```bash
modprobe --dump-modversions new.ko   | awk '{print $2,$1}' | LC_ALL=C sort > new.ver
modprobe --dump-modversions stock.ko | awk '{print $2,$1}' | LC_ALL=C sort > stock.ver
LC_ALL=C join new.ver stock.ver | awk '$2!=$3'
```

The last command must print nothing: every symbol both modules import has the same CRC. The patched Mali driver imports three symbols fewer, `find_get_pid`, `pid_task` and `put_pid`, which `join` leaves out.

> [!TIP]
> `adb pull` can't read `/vendor_dlkm`. Copy the stock module to `/data/local/tmp` with `su` first.

## Step 4: Load them at boot

**vgem** loads at any time:

```bash
insmod /path/to/vgem.ko
```

`/dev/dri/renderD128` appears. Load it from a boot script, for example the `service.sh` of a KernelSU or Magisk module.

**The Mali driver** is in use from the moment Android draws its boot animation, so it can't simply be replaced later. Two ways:

- **Replace it in `vendor_dlkm`.** Rebuild the `vendor_dlkm` image with the patched module in place of the stock one. This is the clean way.
- **Load it before anything uses the GPU.** A KernelSU module's `post-fs-data.sh` runs before SurfaceFlinger. It can `rmmod` the stock driver and `insmod` the patched one, and it should load the stock one back if that fails.

> [!WARNING]
> Never unload the Mali driver while Android is running. Even with every user of `/dev/mali0` stopped, the hardware composer can still hold a GPU fence. When the old module goes away, the kernel panics on the next frame. [The story](../story/03-the-kernel-bug.md) has the details.

Check that the patched driver is running:

```bash
su -c 'dmesg | grep -i "pid pointer"'
```

This should print nothing once a container has used the GPU.

Next: [Android side](03-android-side.md)
