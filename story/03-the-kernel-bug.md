# 3. The kernel bug

## The cause

In kbase r38p1, `kbase_context_common_init()` in `context/mali_kbase_context.c` does:

```c
kctx->tgid = current->tgid;
...
pid_struct = find_get_pid(kctx->tgid);
```

`current->tgid` is the PID as the host sees it. `find_get_pid()` looks the number up in the caller's PID namespace. Inside a container the two differ, the lookup fails, and the driver gives up with `Failed to get pid pointer`. No process in any PID namespace could ever use this Mali, whether in Droidspaces, LXC or Docker.

The lookup only wanted the caller's thread group leader, which is `current->group_leader`. The fix takes a reference on that directly:

```c
kctx->task = current->group_leader;
get_task_struct(kctx->task);
```

The existing `put_task_struct()` at teardown balances it. The OOM notifier in `device/mali_kbase_device.c` had the same lookup, for printing the task's name, and now uses `kctx->task` too.

Only `mali_kbase_mt6835.ko` changes. Nothing imports its symbols, so every other MediaTek GPU module stays stock.

## Will it load?

The stock module says vermagic `5.15.148` and loads on our `5.15.178`, because with `CONFIG_MODVERSIONS` the kernel compares symbol CRCs, not the version string. So we compared the CRCs of every import between our build and the stock module. All 395 shared imports matched. The only difference was the three the patch drops: `find_get_pid`, `pid_task` and `put_pid`.

## Swapping it in, and the panic

The user flashes only the boot image, and the Mali module lives in `vendor_dlkm`. So we swapped it at runtime: stop Android, `rmmod` the stock module, `insmod` ours.

`stop` brought `/sys/module/mali_kbase_mt6835/refcnt` down to 0, and the swap worked. Seconds after the lock screen came back, the kernel panicked. `last_kmsg`:

```
composer@3.1-se ... Internal error: BRK handler
queued_spin_lock_slowpath < dma_fence_remove_callback < sync_file_release
```

`stop` doesn't stop vendor HALs. The hardware composer still held a sync file whose fence came from the old module, and the fence's lock lived in the module's freed memory. A refcount of 0 doesn't cover fences the module has handed out.

The second attempt also stopped the hardware composer, gralloc, the camera and the media HALs first. It worked, and the probe passed **inside the container's own PID namespace**, through a plain `droidspaces run`.

The swap is RAM only, so a reboot restores the stock module. For daily use, the patched module has to be loaded before anything uses the GPU, or replace the stock one in `vendor_dlkm`. See the [guide](../guide/02-kernel.md).

## Side notes from this phase

- adb disconnects every time the Android framework restarts, because system_server reconfigures USB. Run anything that stops and starts Android detached (`setsid ... &`) with its output in a file.
- fastfetch reported `GPU: Mesa llvmpipe` at first. The shell had no libhybris environment: Debian's `.bashrc` returns early for non-interactive shells, so variables appended at its end never apply to `droidspaces run`. With the environment, fastfetch found the Mali through OpenCL.
- Loading the driver twice in one process, once through libhybris' OpenCL and once through its EGL, segfaults.

Next: [First frames](04-first-frames.md)
