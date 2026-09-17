#include "thirdparty/sokol/sokol_app.h"
#include "thirdparty/sokol/sokol_gfx.h"
#include "thirdparty/sokol/sokol_audio.h"
#include "thirdparty/sokol/sokol_log.h"
#include "thirdparty/sokol/sokol_glue.h"

#include <stdio.h>
#include <stdbool.h>

#include "./hotreload.h"

#ifdef LF_HOTRELOAD
#if defined(_WIN32)
#include <Windows.h>
// TODO: Support for posix systems
#endif // PLATFORM
#endif // LF_HOTRELOAD

static void init(void) {
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger.func = slog_func,
    });
    if (!reload_libplug()) {
        printf("HOTRELOAD: Failed to first-load libplug\n");
        return;
    }
    plug_init();
}

#ifdef LF_HOTRELOAD
static bool hotreload = false;
#endif // LF_HOTRELOAD

static void frame(void) {
#ifdef LF_HOTRELOAD
    if (hotreload) {
        printf("HOTRELOAD: Starting hotreload\n");
        void* state = plug_pre_reload();
        if (reload_libplug()) {
            plug_post_reload(state);
            printf("HOTRELOAD: Hotreload Successful\n");
        }
        hotreload = false;
    }
#endif // LF_HOTRELOAD
    plug_frame();
}

static void cleanup(void) {
    plug_cleanup();
    sg_shutdown();
}

static void event(const sapp_event* ev) {
#ifdef LF_HOTRELOAD
    if (ev->key_code == SAPP_KEYCODE_R && ev->type == SAPP_EVENTTYPE_KEY_DOWN)
    {
        hotreload = true;
    }
#endif // LF_HOTRELOAD
    plug_event(ev);
}

#ifdef LF_HOTRELOAD
#if defined(_WIN32)
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;
// TODO: Entry for posix
#else
int main() {
#endif // PLATFORM
    sapp_run(&(sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .window_title = LF_WINDOW_TITLE,
        .width = LF_WINDOW_WIDTH,
        .height = LF_WINDOW_HEIGHT,
        .icon.sokol_default = true,
        .logger.func = slog_func,
    });
}
#else
sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .window_title = LF_WINDOW_TITLE,
        .width = LF_WINDOW_WIDTH,
        .height = LF_WINDOW_HEIGHT,
        .icon.sokol_default = true,
        .logger.func = slog_func,
    };
}
#endif // LF_HOTRELOAD
