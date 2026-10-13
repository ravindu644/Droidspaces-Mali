# 10. Daily driving

## Plasma at container boot

A systemd service starts Plasma as the desktop user. Three things made it work:

- `PAMName=login` gives the service a real logind session: a seat, `/run/user/<uid>`, the user's systemd manager and polkit, as a console login would.
- `pam_systemd` moves the session into its own scope, outside the service. Stopping the service alone left KWin running, and every new start failed with "already running". `ExecStopPost=loginctl terminate-user` ends it on every stop, restart or crash.
- With a user manager present, Plasma started half of itself as systemd user units, out of the service's reach. `systemdBoot=false` in `startkderc` keeps the whole session in one place.

## Audio

The Anland backend's audio is a PipeWire client. With `pipewire`, `pipewire-pulse`, `wireplumber` and `pipewire-audio` installed, it creates **Anland remote speaker** as the default output and **Anland remote microphone**.

The microphone recorded pure silence. Our debug build of the Anland app never got the record permission. `pm grant com.anland.consumer android.permission.RECORD_AUDIO` fixed it.

A systemd user manager only loads units that exist when it starts. After installing PipeWire into a running session, run `systemctl --user daemon-reload` and start the sockets once.

## The lock screen ran at 10 fps

Moving the cursor on the lock screen brought up the unlock prompt with a slow, stuttering blur. The greeter was on llvmpipe.

KWin starts the greeter itself, and Xwayland too. KWin's environment is libhybris' EGL, which is wrong for its children, so a preload in KWin, `scrubenv.so`, removed `LD_LIBRARY_PATH` and `LD_PRELOAD` before they could leak. With nothing left, the greeter fell back to the CPU.

KWin's own environment can't simply be swapped, because it reads `LIBEGL`, `KWIN_COMPOSE` and the GBM driver override after startup. So `scrubenv.so` now wraps `execve()` and friends: each child gets the app environment from `mali-gpu.env`, without KWin's own variables. The new environment is built on the stack, because the child may share the parent's memory (`vfork`) and must not call `malloc`. The greeter now runs on Zink.

## An idle desktop that never idled

The Overview effect opened at what felt like 10 fps. Measuring first:

- `dumpsys SurfaceFlinger --latency` on Anland's surface: Anland presented 60 frames a second on a desktop where nothing moved.
- KWin used about 35% CPU while idle. The GPU sat mostly idle at its lowest clock.
- strace of KWin: one commit per frame, a single byte plus an acquire fence passed over the socket.

Three loops in the Anland backend kept the frames coming, and each was enough on its own once the others were fixed:

1. **`present()` committed every composite.** KWin 6.3 calls it even when nothing was painted, so the slot's old content was committed, and Anland's "presented" reply started the next round. Now the backend commits only frames it painted.
2. **`onConsumerReady()` forced a repaint after every presented frame.** KWin clears a layer's "repaint scheduled" flag only when the layer also has a repaint region, and the software cursor reports its rectangle as damage every time. KWin's render loop already reschedules after a frame when something asked for one, so the backend no longer forces it.
3. **The catch-up from the fade blink fix** made each buffer slot repaint what it owed, and that frame's cursor damage became new debt for the other slots. The wake is gone. The owed region is still painted by the next real frame. The blink itself had come from loop 1.

Freezing every app with `SIGSTOP` to find the source misled us for a while: any one of these loops keeps frames coming with every client stopped.

After the fix, an idle desktop commits one frame in five seconds, at about 0.6% CPU. Animations still present every 16.7 ms. This is **Anland patch 3**.

What remains is a pause of about 300 ms before the Overview appears. That's KWin's main thread building the Overview's QML scene: theme lookups (it reads Plasma's theme files about 100 times) and QML instantiation. Pinning KWin to the big cores, or moving it into Android's `top-app` cgroups, changed nothing. KDE users report [the same delay on slow CPUs](https://discuss.kde.org/t/noticeable-delay-to-trigger-window-overview-effect-in-cpu-power-save-mode/42351).

## Mesa as a package, and a phone out of memory

The first package build ran 5 compiler jobs inside the container's 2 GB limit. Droidspaces' limit counts swap too, so the kernel kept the build alive by swapping, until the phone's whole 4 GB of swap was full. Android ran out of memory, and `droidspaces enter` failed with `Out of memory` while opening a namespace. Killing the compilers brought the phone back. With the limit lifted and Plasma stopped, 7 jobs finished the rest in 30 minutes.

It left one more trap. The system D-Bus had restarted under the pressure, and logind lost its connection without noticing. `systemctl stop` then couldn't end the Plasma session, so the old KWin, still holding the deleted Mesa libraries, kept running behind a service that said it had stopped. Restarting logind fixed it.

## Taking the duct tape off

Writing this repository was a chance to remove what the working setup no longer needed:

| Was | Now |
|---|---|
| Python scripts and loose diffs that patched the libhybris tree | a `git format-patch` series and a fork branch |
| patched Mesa libraries copied over Debian's, with backups in `/opt/hy/deb-orig` | Debian's Mesa package rebuilt with the patches, installed with `dpkg` and held |
| a `card0` render node preload | vgem |
| a start script that set the session environment | the systemd unit sets it, from `mali-gpu.env` |
| an Xwayland wrapper | nothing: `scrubenv.so` already gives Xwayland the app environment |
| a service with the user name inside | a template, `plasma-mali@<user>` |
