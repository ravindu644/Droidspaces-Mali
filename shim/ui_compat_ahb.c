/*
 * libui_compat_layer.so for devices whose only gralloc is the gralloc4 HIDL HAL,
 * built with the NDK on top of AHardwareBuffer instead of inside an AOSP tree.
 *
 * hybris's gralloc layer (hybris/gralloc/gralloc.c, "GRALLOC_COMPAT") calls six of the
 * functions from include/hybris/ui/ui_compatibility_layer.h. AHardwareBuffer reaches the
 * same allocator service libui would, and its format and usage values are the gralloc
 * ones, so they pass straight through.
 *
 * Build: aarch64-linux-android30-clang -shared -fPIC -O2 -o libui_compat_layer.so \
 *        ui_compat_ahb.c -lnativewindow -llog
 */
#include <android/hardware_buffer.h>
#include <android/log.h>
#include <android/rect.h>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

typedef int32_t status_t;
typedef int32_t PixelFormat;
typedef const struct native_handle *buffer_handle_t;

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "ui_compat_ahb", __VA_ARGS__)

/* Platform-only exports of libnativewindow: present on the device, absent from the NDK. */
static buffer_handle_t (*get_native_handle)(const AHardwareBuffer *);

/* ponytail: linear table of live buffers; a hash if a client ever holds hundreds */
#define MAX_BUFFERS 256
static struct { buffer_handle_t handle; AHardwareBuffer *ahb; } table[MAX_BUFFERS];
static pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;

static AHardwareBuffer *lookup(buffer_handle_t handle, int take)
{
    AHardwareBuffer *ahb = NULL;
    pthread_mutex_lock(&table_lock);
    for (int i = 0; i < MAX_BUFFERS; i++) {
        if (table[i].handle == handle) {
            ahb = table[i].ahb;
            if (take)
                table[i].handle = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&table_lock);
    return ahb;
}

status_t graphic_buffer_allocator_allocate(uint32_t width, uint32_t height, PixelFormat format,
                                           uint32_t layerCount, uint64_t usage,
                                           buffer_handle_t *handle, uint32_t *stride,
                                           uint64_t graphicBufferId, const char *requestorName)
{
    (void)graphicBufferId;
    (void)requestorName;
    /* By handle, not RTLD_DEFAULT: under libhybris dlsym goes to its own linker,
     * whose default scope doesn't cover the Android libraries we were linked against. */
    if (!get_native_handle) {
        void *nw = dlopen("libnativewindow.so", RTLD_NOW);
        if (nw)
            get_native_handle = dlsym(nw, "AHardwareBuffer_getNativeHandle");
    }
    if (!get_native_handle) {
        LOGE("AHardwareBuffer_getNativeHandle not found in libnativewindow.so");
        return -ENOSYS;
    }

    AHardwareBuffer_Desc desc = {
        .width = width, .height = height, .layers = layerCount ? layerCount : 1,
        .format = (uint32_t)format, .usage = usage,
    };
    AHardwareBuffer *ahb = NULL;
    int ret = AHardwareBuffer_allocate(&desc, &ahb);
    if (ret) {
        LOGE("AHardwareBuffer_allocate %ux%u fmt %d usage 0x%llx: %d", width, height, format,
             (unsigned long long)usage, ret);
        return ret;
    }
    AHardwareBuffer_describe(ahb, &desc);

    pthread_mutex_lock(&table_lock);
    int slot = -1;
    for (int i = 0; i < MAX_BUFFERS && slot < 0; i++)
        if (!table[i].handle)
            slot = i;
    if (slot >= 0) {
        table[slot].handle = get_native_handle(ahb);
        table[slot].ahb = ahb;
    }
    pthread_mutex_unlock(&table_lock);
    if (slot < 0) {
        AHardwareBuffer_release(ahb);
        return -ENOMEM;
    }
    *handle = table[slot].handle;
    *stride = desc.stride; /* pixels, like GraphicBufferAllocator */
    return 0;
}

status_t graphic_buffer_allocator_free(buffer_handle_t handle)
{
    AHardwareBuffer *ahb = lookup(handle, 1);
    if (!ahb)
        return -EINVAL;
    AHardwareBuffer_release(ahb);
    return 0;
}

status_t graphic_buffer_mapper_import_buffer(buffer_handle_t rawHandle, uint32_t width,
                                             uint32_t height, uint32_t layerCount,
                                             PixelFormat format, uint64_t usage, uint32_t stride,
                                             buffer_handle_t *outHandle)
{
    /* ponytail: only buffers this process allocated are known; importing a foreign
     * handle needs AHardwareBuffer_createFromHandle, add it when a compositor needs it */
    (void)rawHandle; (void)width; (void)height; (void)layerCount;
    (void)format; (void)usage; (void)stride; (void)outHandle;
    LOGE("import_buffer not supported");
    return -ENOSYS;
}

status_t graphic_buffer_mapper_import_buffer_no_size(buffer_handle_t rawHandle,
                                                     buffer_handle_t *outHandle)
{
    return graphic_buffer_mapper_import_buffer(rawHandle, 0, 0, 0, 0, 0, 0, outHandle);
}

status_t graphic_buffer_mapper_free_buffer(buffer_handle_t handle)
{
    return graphic_buffer_allocator_free(handle);
}

status_t graphic_buffer_mapper_lock(buffer_handle_t handle, uint32_t usage, const ARect *bounds,
                                    void **vaddr, int32_t *outBytesPerPixel,
                                    int32_t *outBytesPerStride)
{
    AHardwareBuffer *ahb = lookup(handle, 0);
    if (!ahb)
        return -EINVAL;
    int32_t bpp = -1, bps = -1;
    int ret = AHardwareBuffer_lockAndGetInfo(ahb, usage, -1, bounds, vaddr, &bpp, &bps);
    if (outBytesPerPixel)
        *outBytesPerPixel = bpp;
    if (outBytesPerStride)
        *outBytesPerStride = bps;
    return ret;
}

status_t graphic_buffer_mapper_unlock(buffer_handle_t handle)
{
    AHardwareBuffer *ahb = lookup(handle, 0);
    if (!ahb)
        return -EINVAL;
    return AHardwareBuffer_unlock(ahb, NULL);
}
