# 6. KWin

KWin needs Anland's backend to send its frames to the Anland app. Anland's `producers/kde/Debian13_v5/build.sh` rebuilds Debian's KWin and Xwayland packages with it. Our fork's `legacy` branch adds two fixes to the backend:

| Patch | Why |
|---|---|
| `patches/anland/0002-*.patch` | fades blinked once per buffer on KWin before 6.6: a catch-up frame committed a buffer without repainting it |
| `patches/anland/0003-*.patch` | an idle desktop rendered at 60 fps and kept KWin at about 35% CPU. Now it renders only what changed |

## Step 1: Build and install

Inside the container, as root:

```bash
apt-get install --no-install-recommends kde-plasma-desktop konsole dbus-user-session sudo
git clone --depth 1 -b legacy https://github.com/ravindu644/anland /opt/anland-src
cd /opt/anland-src/producers/kde/Debian13_v5
ANLAND_INSTALL=1 ./build.sh
```

The script enables source packages, installs the build dependencies, builds both packages under `~/anland-debbuild` and installs them. With 7 jobs it takes about 50 minutes and peaks above 4 GB, so build with **Limit memory** off, as for Mesa.

> [!WARNING]
> `kwin` in that directory is a symlink to the backend sources, and the script copies through it. Don't prepare or clean the build tree by hand: an `rm -rf` through the symlink deletes the backend sources.

## Step 2: Hold the packages

The script keeps Debian's version numbers, so `apt upgrade` would replace them with Debian's own. Hold them:

```bash
cd ~/anland-debbuild
apt-mark hold $(for d in kwin/*.deb xwayland/*.deb; do p=$(basename ${d%%_*}); dpkg -s "$p" >/dev/null 2>&1 && echo "$p"; done)
```

## Step 3: Check

```bash
kwin_wayland --help | grep anland
```

It should list `--anland`.

Next: [Session](07-session.md)
