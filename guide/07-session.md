# 7. Session

Plasma starts with the container, as your desktop user, through a systemd service. The pieces:

| File | Installed as | Why |
|---|---|---|
| `mali-gpu.env` | `/opt/hy/etc/mali-gpu.env` | the environment every app gets: Mesa with Zink on the Mali's Vulkan driver through libhybris, and the Mali workarounds |
| `kwin_wayland` | `/opt/hy/bin/kwin_wayland` | starts KWin on the Mali's own EGL instead |
| `scrubenv.c` | `/opt/hy/bin/scrubenv.so` | gives the processes KWin starts, Xwayland and the lock screen, the app environment back |
| `plasma-mali@.service` | `/etc/systemd/system/` | starts Plasma when the container boots |
| `llvmpipe-run` | `/usr/local/bin/llvmpipe-run` | runs one app on the CPU, if it misbehaves on the GPU |

Two of these exist because of how KWin is packaged:

- Debian gives `/usr/bin/kwin_wayland` the `cap_sys_nice` file capability, for realtime scheduling. A binary with file capabilities makes glibc ignore `LD_LIBRARY_PATH`, so KWin would never find libhybris. The service keeps a copy without the capability in `/opt/hy/libexec`, refreshed after a KWin upgrade, and the wrapper runs that. KWin loses realtime scheduling.
- KWin needs the Mali's EGL, while the processes it starts need Mesa's. A child inherits KWin's environment, so `scrubenv.so`, preloaded into KWin only, swaps the app environment in at `exec`.

## Step 1: Install the files

Inside the container, as root:

```bash
cd ~/Droidspaces-Mali/session
mkdir -p /opt/hy/bin /opt/hy/etc
install -m644 mali-gpu.env /opt/hy/etc/
install -m755 kwin_wayland /opt/hy/bin/
gcc -O2 -Wall -shared -fPIC -o /opt/hy/bin/scrubenv.so scrubenv.c -ldl
install -m755 llvmpipe-run /usr/local/bin/
install -m644 plasma-mali@.service /etc/systemd/system/
```

## Step 2: The desktop user

Create a user in the `droidspaces-gpu` group, which owns `/dev/mali0` in the container:

```bash
useradd -m -s /bin/bash -G sudo,audio,video,render,droidspaces-gpu alice
passwd alice
```

Keep Plasma's whole session in the service, instead of starting parts of it as systemd user units:

```bash
sudo -u alice kwriteconfig6 --file startkderc --group General --key systemdBoot false
```

## Step 3: Audio

The Anland backend plays sound through PipeWire and creates two devices, **Anland remote speaker** and **Anland remote microphone**:

```bash
apt-get install pipewire pipewire-pulse wireplumber pipewire-audio plasma-pa
```

## Step 4: Start it

```bash
systemctl enable --now plasma-mali@alice
```

In the Droidspaces app, open the container's details and tap **Launch** on the **Anland** card. The desktop appears. From now on, Plasma starts every time the container does.

To restart the session, run `systemctl restart plasma-mali@alice`.

Next: [Check it](08-check-it.md)
