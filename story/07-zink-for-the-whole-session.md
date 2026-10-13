# 7. Zink for the whole session

Next, every app in the session should default to Zink, not only the ones started through a wrapper. The session got an environment file, and plasmashell drew only the wallpaper: no panel, no launcher. With llvmpipe it was fine.

vkcube and es2gears have one window. plasmashell has many, and it found three libhybris bugs.

## The panel never drew

`WAYLAND_DEBUG` showed the panel's surface configured and acknowledged, a libhybris window created, and then no buffer, ever. The window's constructor bound the dma-buf protocol with `wl_display_roundtrip()` on the app's default queue, from Qt's render thread. Qt's own Wayland thread raced it for the reply.

**Patch 18:** do the registry roundtrip on the window's own queue. Destroy the protocol object before the queue, or libwayland aborts with `Tried to add event to destroyed queue`.

## plasmashell hung or crashed closing a surface

gdb showed the main thread blocked in `wl_display_dispatch_queue()` under `vkDestroySurfaceKHR()`. libhybris' Vulkan Wayland platform waited for `android_wlegl` at both surface creation and teardown. KWin never offers it, so teardown waited forever. When an event did wake it, it crashed destroying a NULL proxy.

**Patch 19:** both waits wait for the sync callback instead, and `android_wlegl` is destroyed only when it was bound.

## The shell froze on popups

Opening the launcher froze plasmashell half drawn. Zink's driver thread spun at 100% CPU, and a dropped log line said `dequeueBuffer returned unrecognized buffer`. That message is from Android's own swapchain code.

When a swapchain is recreated, Android disconnects and reconnects the window, so the old buffers are dropped. libhybris ignored the disconnect. A buffer KWin released after the recreation came back from `dequeueBuffer`, the swapchain answered `OUT_OF_DATE`, Zink recreated it, and the loop repeated forever.

**Patch 20:** on disconnect, idle buffers are destroyed and busy ones are parked until they come back. We toggled the launcher 39 times over D-Bus to confirm it. Anland's touch input acts as a touchpad, so `adb shell input tap` doesn't reach Plasma.

## Dolphin took 55 seconds to open

strace showed two 25 second D-Bus timeouts, for UPower and then UDisks. Their systemd units use `PrivateUsers=`, and without **Allow Sandboxing**, Droidspaces' seccomp filter refuses new user namespaces. The services failed with `status 217/USER`. With the option on, Dolphin's first frame arrives in 1.6 seconds.

## After a container restart

The Anland app keeps its connection to the old container. KWin then logs `consumer disconnected, entering fallback` and hangs. Force stop the app and open it again from Droidspaces.

Next: [A real render node](08-a-real-render-node.md)
