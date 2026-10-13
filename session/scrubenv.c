/* Preloaded into kwin_wayland only. KWin runs on hybris's EGL; every process it starts
 * (the lock screen greeter, Xwayland, helpers) is a client and needs the session's zink
 * environment instead. Dropping KWin's variables alone left those on llvmpipe, and the
 * greeter's blur ran at 10 fps on the CPU.
 *
 * KWin's own environment can't simply be swapped: it reads LIBEGL, KWIN_COMPOSE and
 * MESA_LOADER_DRIVER_OVERRIDE (for its GBM device) after startup. So the client
 * environment is applied at exec, in the child only. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CLIENT_ENV "/opt/hy/etc/mali-gpu.env"
#define MAX_VARS 64

extern char **environ;

/* KWin's own settings that a client must not inherit. */
static const char *const kwin_only[] = { "LIBEGL=", "LIBGLESV2=", "KWIN_COMPOSE=",
                                         "ANLAND_DRM_DEVICE=", NULL };

static char *client[MAX_VARS];
static int nclient;

static int has_prefix(const char *s, const char *const *list)
{
    for (; *list; list++)
        if (!strncmp(s, *list, strlen(*list)))
            return 1;
    return 0;
}

static int set_by_client(const char *s)
{
    size_t n = strcspn(s, "=");
    for (int i = 0; i < nclient; i++)
        if (!strncmp(s, client[i], n + 1))
            return 1;
    return 0;
}

/* Built on the stack: the child may share the parent's memory (vfork), so no malloc. */
#define WITH_CLIENT_ENV(envp, out)                                              \
    size_t n_ = 0;                                                              \
    while ((envp)[n_])                                                          \
        n_++;                                                                   \
    char *out[n_ + nclient + 1];                                                \
    {                                                                           \
        size_t j_ = 0;                                                          \
        for (size_t i_ = 0; i_ < n_; i_++)                                      \
            if (!has_prefix((envp)[i_], kwin_only) && !set_by_client((envp)[i_])) \
                out[j_++] = (envp)[i_];                                         \
        for (int i_ = 0; i_ < nclient; i_++)                                    \
            out[j_++] = client[i_];                                             \
        out[j_] = NULL;                                                         \
    }

int execve(const char *path, char *const argv[], char *const envp[])
{
    static int (*real)(const char *, char *const[], char *const[]);
    if (!real)
        real = dlsym(RTLD_NEXT, "execve");
    WITH_CLIENT_ENV(envp, env);
    return real(path, argv, env);
}

int execv(const char *path, char *const argv[])
{
    return execve(path, argv, environ);
}

int execvpe(const char *file, char *const argv[], char *const envp[])
{
    static int (*real)(const char *, char *const[], char *const[]);
    if (!real)
        real = dlsym(RTLD_NEXT, "execvpe");
    WITH_CLIENT_ENV(envp, env);
    return real(file, argv, env);
}

int execvp(const char *file, char *const argv[])
{
    return execvpe(file, argv, environ);
}

__attribute__((constructor)) static void scrub(void)
{
    char line[1024];
    FILE *f;

    /* glibc has already parsed these for KWin; children must not get them. */
    unsetenv("LD_LIBRARY_PATH");
    unsetenv("LD_PRELOAD");

    /* Plain KEY=VALUE lines, the same file plasma-mali@.service reads. */
    f = fopen(CLIENT_ENV, "re");
    if (!f)
        return;
    while (nclient < MAX_VARS && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] != '#' && strchr(line, '='))
            client[nclient++] = strdup(line);
    }
    fclose(f);
}
