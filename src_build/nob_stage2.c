#include <stdbool.h>

#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX

#define BUILD_FOLDER "build/"
#define SRC_FOLDER "src/"
#define THIRDPARTY_FOLDER "thirdparty/"
#define RES_FOLDER "res/"

#include "../nob.h-main/nob.h"
#include "../build/config.h"
#include "./configurer.c"

#define STB_RECT_PACK_IMPLEMENTATION
#include "../thirdparty/stb_rect_pack.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../thirdparty/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../thirdparty/stb_image_write.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "../thirdparty/stb_truetype.h"
#include "../thirdparty/stb_vorbis.c"
#include "./asset_generator.c"


#if defined(LF_TARGET_LINUX)
#include "nob_linux.c"
#elif defined(LF_TARGET_EMSCRIPTEN)
#include "nob_emscripten.c"
#elif defined(LF_TARGET_MACOS)
#include "nob_macos.c"
#elif defined(LF_TARGET_WIN64_MINGW)
#include "nob_win64_mingw.c"
#elif defined(LF_TARGET_WIN64_MSVC)
#include "nob_win64_msvc.c"
#else
#error "No Target is defined. Check your ./build/config.h."
#endif // LF_TARGET

#include "../build/config_logger.c"

void log_available_subcommands(const char *program, Nob_Log_Level level)
{
    nob_log(level, "Usage: %s [subcommand]", program);
    nob_log(level, "Subcommands:");
    nob_log(level, "    build (default)");
    nob_log(level, "    build_plug (only when hotreloading is enabled)");
    nob_log(level, "    build_assets");
    nob_log(level, "    dist");
    nob_log(level, "    help");
}

int main(int argc, char **argv)
{
    nob_log(NOB_INFO, "--- STAGE 2 ---");
    log_config(NOB_INFO);
    nob_log(NOB_INFO, "---");

    const char *program = nob_shift_args(&argc, &argv);

    const char *subcommand = NULL;
    if (argc <= 0) {
        subcommand = "build";
    } else {
        subcommand = nob_shift_args(&argc, &argv);
    }

    if (strcmp(subcommand, "build") == 0) {
        Nob_Cmd cmd = {0};
        if (!generate_assets_config(SRC_FOLDER "assets.h")) return 1;
        if (!compile_shaders(&cmd)) return 1;
        if (!build_sokol(&cmd)) return 1;
#ifdef LF_HOTRELOAD
        // When hot-reloading is enabled, build libplug first
        if (!build_libplug(&cmd)) return 1;
#endif // LF_HOTRELOAD
        if (!build_log_frame(&cmd)) return 1;
    } else if (strcmp(subcommand, "build_plug") == 0) {
#ifdef LF_HOTRELOAD
        Nob_Cmd cmd = {0};
        if (!generate_assets_config(SRC_FOLDER "assets.h")) return 1;
        if (!compile_shaders(&cmd)) return 1;
        if (!build_libplug(&cmd)) return 1;
#else
        nob_log(NOB_ERROR, "'build_plug' is only available when hotreloading is enabled");
#endif // LF_HOTRELOAD
    } else if (strcmp(subcommand, "build_assets") == 0) {
        if (!generate_assets_config(SRC_FOLDER "assets.h")) return 1;
    } else if (strcmp(subcommand, "dist") == 0) {
        if (!build_dist()) return 1;
    } else if (strcmp(subcommand, "help") == 0) {
        log_available_subcommands(program, NOB_INFO);
    } else {
        nob_log(NOB_ERROR, "Unknown subcommand %s", subcommand);
        log_available_subcommands(program, NOB_ERROR);
    }
    // TODO: it would be nice to check for situations like building TARGET_WIN64_MSVC on Linux and report that it's not possible.
    return 0;
}