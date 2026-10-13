# 8. Check it

## Every process on the right driver

Inside the container, as root, while Plasma runs:

```bash
for c in kwin_wayland plasmashell Xwayland; do
    p=$(pgrep -x $c | head -1)
    echo "$c: $(grep -oE 'libGLES_mali|hybris/vulkan|libgallium|swrast' /proc/$p/maps | sort -u | tr '\n' ' ')"
done
```

| Process | Must show | Means |
|---|---|---|
| `kwin_wayland` | `libGLES_mali` | KWin composites on the Mali |
| `plasmashell` | `hybris/vulkan`, `libgallium`, `libGLES_mali` | Zink on the Mali's Vulkan driver |
| `Xwayland` | the same as plasmashell | X11 apps render on the Mali too |

`swrast` anywhere means that process fell back to the CPU.

## From an app's point of view

In Konsole on the desktop:

```bash
eglinfo -B -p wayland | grep -m1 renderer    # zink Vulkan 1.3(Mali-G57 MC2 (ARM_PROPRIETARY))
glxinfo -B | grep renderer                   # the same, through Xwayland
vulkaninfo --summary | grep deviceName       # Mali-G57 MC2
```

`eglinfo` and `glxinfo` are in `mesa-utils`, `vulkaninfo` in `vulkan-tools`.

## SELinux

On the phone:

```bash
adb shell getenforce
```

It should print `Enforcing`.

## A real workload

```bash
apt-get install glmark2-es2-wayland
glmark2-es2-wayland
```

The Galaxy A16 scores about 133. llvmpipe, Mesa's CPU renderer, scores 61.

Next: [Troubleshooting](09-troubleshooting.md)
