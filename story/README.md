# The story

How a stock Mali driver ended up rendering a KDE Plasma desktop inside a container, in the order it happened. Every wall we hit is here, with what caused it and how we found out. The [guide](../README.md#guide) is the clean result.

It all happened on one Samsung Galaxy A16 5G (SM-A166P, MediaTek MT6835, Mali-G57 MC2, Android 14, kernel 5.15), between 2026-10-13 and 2026-10-14.

1. [Picking a route](01-picking-a-route.md): three ways to reach a Mali GPU, and proving the driver can do the job
2. [Bionic in a glibc world](02-bionic-in-glibc.md): getting the Android driver to start inside the container
3. [The kernel bug](03-the-kernel-bug.md): why no container could ever use the Mali, and a kernel panic on the way to the fix
4. [First frames](04-first-frames.md): Mali frames on the phone screen through Anland
5. [Plasma on the Mali](05-plasma-on-the-mali.md): KWin composites on the GPU
6. [Vulkan and Zink](06-vulkan-and-zink.md): apps on the GPU, and two colour bugs
7. [Zink for the whole session](07-zink-for-the-whole-session.md): plasmashell, popups and a slow Dolphin
8. [A real render node](08-a-real-render-node.md): vgem
9. [X11](09-x11.md): Xwayland and old OpenGL games on the Mali
10. [Daily driving](10-daily-driving.md): autostart, audio, the lock screen and an idle desktop that never idled
11. [Fedora 44](11-fedora.md): a CI-built rootfs, what it took, and why browsers sent us back to Debian

## Lessons that apply everywhere

- **Measure before you guess.** Most bugs here looked like something else. Two "tearing" reports were a wrong library and wrong textures. The fade blink wasn't GPU sync. Each was settled by a measurement, not by trying fixes.
- **Check what a process really loaded.** `/proc/<pid>/maps` answers whether KWin is on the Mali or an app on Zink. Environment variables and config files only tell you what should have happened.
- **`ldconfig` treats any `lib*.so*` file in a library directory as a candidate**, whatever its suffix. Backups named `.pkg` or `.deb-orig` silently replaced patched libraries twice.
- **A bionic library under glibc fails far from the cause.** A crash inside the driver was a pthread key, a crash in `sscanf` was a TLS slot. Read the faulting instruction and the registers, then work backwards.
- **One change at a time under enforcing SELinux.** Some denials are `dontaudit` and never reach a log, so the only way to find a missing rule is to add or remove rules one at a time and test.
- **Freezing processes with `SIGSTOP` to find a render loop misleads**, when more than one loop can keep the frames coming on its own.
- **A silent fallback is the worst failure.** Mesa can drop to llvmpipe without a word. Check the renderer after every change to the stack.
- **"It survived" is not "it works".** A crash test can pass because the feature under test never started. Read the log before believing a pass.

## Still open

- Find what in Zink's generated geometry shaders crashes the Mali compiler, so OpenGL 3.2 and OpenGL ES 3.2 can come back.
- Narrow `VK_EXT_extended_dynamic_state` down to the one dynamic state the Mali gets wrong, and report it to ARM.
- AFBC buffers instead of linear ones.
- Instance divisors above 1.
- The 300 ms pause before KWin's Overview opens.
- Newer distributions, see [Fedora 44](11-fedora.md#conclusion): `nullDescriptor` emulation for current Zink, the driver bug KWin 6.7's Qt Quick scenes trigger, and Zink's gradients and clip masks in browsers.
