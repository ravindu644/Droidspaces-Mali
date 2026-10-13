# 9. Troubleshooting

Start with the logs. Plasma's are in the service's journal:

```bash
journalctl -u plasma-mali@alice -b
```

| Symptom | Cause | Fix |
|---|---|---|
| dmesg says `Failed to get pid pointer`, apps fail to create a GPU context | the stock Mali driver is loaded | load the patched one, see [Kernel](02-kernel.md) |
| `Binder driver could not be opened`, `openat("/dev/hwbinder")` returns `ENXIO` | Droidspaces without `a91e5a49` copied the binder nodes with `mknod` | update Droidspaces, see [Android side](03-android-side.md) |
| `Failed to load matching allocator for mapper version 4` | the SELinux rule is missing | see [Android side](03-android-side.md). The denial never shows in logcat, because Android hides it |
| `vulkaninfo` segfaults with a NULL `vkGetInstanceProcAddr` | Android's Vulkan loader couldn't load `libgpud_sys.so` | bind `/system_ext` into the container |
| KWin waits with `waiting for display daemon` | the Anland app isn't showing the display | tap **Launch** on the container's **Anland** card |
| The Anland app stays black on MediaTek | the official app sends the gralloc metadata fd | build the app from our fork, see [Android side](03-android-side.md) |
| KWin hangs after a container restart, `consumer disconnected` | the Anland app kept its connection to the old container | force stop the app, then open it again from Droidspaces |
| `/proc/<kwin>/maps` has no `libGLES_mali` | KWin started from `/usr/bin` instead of the wrapper | check that `/opt/hy/bin` is first in the service's `PATH` and that `/opt/hy/bin/kwin_wayland` is executable |
| An app shows wrong colours, flickers or crashes after an `apt install` | `ldconfig` linked a library's soname to a backup copy you left next to it | never keep `lib*.so*` backups in a library directory, whatever the suffix. Check `/proc/<pid>/maps` for what really loaded |
| `systemctl stop` leaves KWin running, and the next start fails. The journal says `Failed to activate service 'org.freedesktop.login1': timed out` | the system D-Bus restarted, for example under memory pressure, and logind lost its connection | `systemctl restart systemd-logind`, then `loginctl terminate-user alice`, then start the service |
| Dolphin takes almost a minute to open | UPower and UDisks can't start without user namespaces | turn on **Allow Sandboxing** |
| No microphone | the debug-built Anland app has no record permission | `adb shell pm grant com.anland.consumer android.permission.RECORD_AUDIO` |
| One app renders wrongly on the GPU | the Mali lacks a feature the app needs | run it with `llvmpipe-run <app>` |

Something new? [The story](../story/README.md) shows how each of these was tracked down. The same tools usually work.
