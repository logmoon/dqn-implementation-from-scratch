#include <stdio.h>
#include <dlfcn.h>

#include "thirdparty/sokol/sokol_app.h"
#include "hotreload.h"

#if defined(LF_TARGET_MACOS)
    static const char *libplug_file_name = "libplug.dylib";
#else
    static const char *libplug_file_name = "libplug.so";
#endif

static void *libplug = NULL;

#define PLUG(name, ...) name##_t *name = NULL;
LIST_OF_PLUGS
#undef PLUG

bool check_libplug_modified()
{
    return false;
}

bool reload_libplug(void)
{
    if (libplug != NULL) dlclose(libplug);

    libplug = dlopen(libplug_file_name, RTLD_NOW);
    if (libplug == NULL) {
        sapp_log(SAPP_LOG_LEVEL_ERROR, "HOTRELOAD: could not load %s: %s", libplug_file_name, dlerror());
        return false;
    }

    #define PLUG(name, ...) \
        name = dlsym(libplug, #name); \
        if (name == NULL) { \
            sapp_log(SAPP_LOG_LEVEL_ERROR, "HOTRELOAD: could not find %s symbol in %s: %s", \
                     #name, libplug_file_name, dlerror()); \
            return false; \
        }
    LIST_OF_PLUGS
    #undef PLUG

    return true;
}