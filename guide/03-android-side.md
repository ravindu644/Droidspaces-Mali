# 3. Android side

## Step 1: Droidspaces

You need a Droidspaces build with two fixes. Both are on the `dev` branch and will be in the next release.

| Commit | Why |
|---|---|
| `a91e5a49` | GPU Access binds the host's binder nodes into the container. Before it, they were copied with `mknod`, and a copy of a binderfs node can never be opened. libhybris needs `/dev/hwbinder` to reach the gralloc service |
| `50e483a9` | one SELinux rule: `allow hwservicemanager droidspacesd binder transfer`. Without it, gralloc fails under enforcing SELinux with `Failed to load matching allocator for mapper version 4` |

The rule ships in Droidspaces' boot module, so it takes effect after the app reinstalls the module and the phone reboots. To try it without a reboot:

```bash
su -c "ksud sepolicy patch 'allow hwservicemanager droidspacesd binder transfer'"     # KernelSU
su -c "magiskpolicy --live 'allow hwservicemanager droidspacesd binder transfer'"     # Magisk
```

That's the only rule this setup needs. SELinux stays enforcing.

## Step 2: The container

Create a Debian 13 container, then stop it and set these options:

| App option | Config key | Why |
|---|---|---|
| **GPU Access** | `enable_gpu_mode=1` | `/dev/mali0`, `/dev/dma_heap/*`, the binder nodes and `/dev/dri/renderD128` in the container |
| **Anland Display** | `enable_anland=1` | the display socket at `/run/display.sock` |
| **Allow Sandboxing** | `allow_sandboxing=1` | Plasma's system services use user namespaces. Without it, UPower and UDisks fail to start and Dolphin waits 50 seconds for them |
| **Limit memory** | `memory_limit=2147483648` | 2 GB is enough for Plasma. Turn the limit off while you build Mesa and KWin, see [Mesa](05-mesa.md) |

Then bind the Android system partitions in, read-only. The Mali driver and everything it loads lives there. In `/data/local/Droidspaces/Containers/<name>/container.config`:

```ini
bind_mounts=/vendor:/vendor:ro,/system:/system:ro,/system_ext:/system_ext:ro,/apex:/apex:ro,/dev/__properties__:/dev/__properties__:ro
```

`/dev/__properties__` lets Android libraries read system properties. `/system_ext` holds `libgpud_sys.so`, which Android's Vulkan loader needs.

## Step 3: The Anland app

On MediaTek phones, the official Anland 5.x app shows nothing. A MediaTek gralloc buffer's first file descriptor is a metadata handle, not the pixels, and the app sends that one. `patches/anland/0001-*.patch` picks the largest dma-buf in the handle instead. Until it's upstream, build the app from the fork:

```bash
git clone --depth 1 -b legacy https://github.com/ravindu644/anland
cd anland/consumers/anland_v5/android_consumer
./gradlew assemblePlainDebug
adb uninstall com.anland.consumer       # the release build is signed with another key
adb install app/build/outputs/apk/plain/debug/app-plain-debug.apk
```

The build pins NDK 29 in `app/build.gradle`. Install it with the SDK manager, or set `ndkVersion` to one you have. NDK 27 works.

Open the app once, grant it root and finish its first-run setup.

For the microphone, a debug build doesn't get the permission from its manifest alone:

```bash
adb shell pm grant com.anland.consumer android.permission.RECORD_AUDIO
```

Qualcomm phones don't need the fork for this. Their first descriptor is the pixels.

Next: [libhybris](04-libhybris.md)
