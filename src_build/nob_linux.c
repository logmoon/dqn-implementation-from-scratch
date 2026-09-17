#define LF_TARGET_NAME "linux"

#ifdef LF_DEBUG
void add_debug_flags(Nob_Cmd *cmd) {
    nob_cmd_append(cmd, "-g");
    nob_cmd_append(cmd, "-O0");
    nob_cmd_append(cmd, "-Wall");
    nob_cmd_append(cmd, "-Wextra");
}
#else
void add_debug_flags(Nob_Cmd *cmd) {
}
#endif // LF_DEBUG

bool build_sokol(Nob_Cmd *cmd) {
#ifdef LF_HOTRELOAD
    if (nob_needs_rebuild1(BUILD_FOLDER "libsokol.so", THIRDPARTY_FOLDER "sokol/sokol.c")) {
        nob_log(NOB_INFO, "Building libsokol.so");
        nob_cmd_append(cmd, "gcc");
        add_debug_flags(cmd);
        nob_cmd_append(cmd, "-I.");
        nob_cmd_append(cmd, "-DSOKOL_GLCORE");
        nob_cmd_append(cmd, "-DSOKOL_DLL");
        nob_cmd_append(cmd, "-DSOKOL_NO_ENTRY");
        nob_cmd_append(cmd, "-fPIC");
        nob_cmd_append(cmd, "-shared");
        nob_cmd_append(cmd, "-o", BUILD_FOLDER "libsokol.so");
        nob_cmd_append(cmd, THIRDPARTY_FOLDER "sokol/sokol.c");
        nob_cmd_append(cmd, "-lGL", "-lX11", "-lXcursor", "-lXrandr", "-lXinerama", "-lXi");
        nob_cmd_append(cmd, "-lasound", "-lpulse", "-lm", "-ldl", "-lpthread");
        if (!nob_cmd_run_sync_and_reset(cmd)) return false;
    }
#else
    if (nob_needs_rebuild1(BUILD_FOLDER "sokol.o", THIRDPARTY_FOLDER "sokol/sokol.c")) {
        nob_log(NOB_INFO, "Building sokol.o");
        nob_cmd_append(cmd, "gcc", "-c");
        add_debug_flags(cmd);
        nob_cmd_append(cmd, "-I.");
        nob_cmd_append(cmd, "-DSOKOL_GLCORE");
        nob_cmd_append(cmd, "-o", BUILD_FOLDER "sokol.o");
        nob_cmd_append(cmd, THIRDPARTY_FOLDER "sokol/sokol.c");
        if (!nob_cmd_run_sync_and_reset(cmd)) return false;
    }
#endif // LF_HOTRELOAD
    return true;
}

bool compile_shaders(Nob_Cmd* cmd) {
    nob_log(NOB_INFO, "Compiling main shader");
    nob_cmd_append(cmd, "./sokol-shdc");
    nob_cmd_append(cmd, "-i", SRC_FOLDER "main_shader.glsl");
    nob_cmd_append(cmd, "-o", SRC_FOLDER "main_shader.h");
    nob_cmd_append(cmd, "-l", "glsl430");
    if (!nob_cmd_run_sync_and_reset(cmd)) return false;

    nob_log(NOB_INFO, "Compiling lighting shader");
    nob_cmd_append(cmd, "./sokol-shdc");
    nob_cmd_append(cmd, "-i", SRC_FOLDER "lighting_shader.glsl");
    nob_cmd_append(cmd, "-o", SRC_FOLDER "lighting_shader.h");
    nob_cmd_append(cmd, "-l", "glsl430");
    if (!nob_cmd_run_sync_and_reset(cmd)) return false;

    return true;
}

bool build_libplug(Nob_Cmd* cmd) {
#ifdef LF_HOTRELOAD
    nob_log(NOB_INFO, "Building libplug.so");
    nob_cmd_append(cmd, "gcc");
    add_debug_flags(cmd);
    nob_cmd_append(cmd, "-I.");
    nob_cmd_append(cmd, "-I" THIRDPARTY_FOLDER "sokol");
    nob_cmd_append(cmd, "-DSOKOL_GLCORE");
    nob_cmd_append(cmd, "-DSOKOL_DLL");
    nob_cmd_append(cmd, "-fPIC");
    nob_cmd_append(cmd, "-shared");
    nob_cmd_append(cmd, "-o", BUILD_FOLDER "libplug_temp.so");
    nob_cmd_append(cmd, SRC_FOLDER "plug.c");
    nob_cmd_append(cmd, "-L"BUILD_FOLDER, "-l:libsokol.so");
        nob_cmd_append(cmd, "-lGL", "-lX11", "-lXcursor", "-lXrandr", "-lXinerama", "-lXi");
        nob_cmd_append(cmd, "-lasound", "-lpulse", "-lm", "-ldl", "-lpthread");
        if (!nob_cmd_run_sync_and_reset(cmd)) return false;
        return true;
#else
    return true;
#endif
}

bool build_log_frame(Nob_Cmd* cmd) {
#ifdef LF_HOTRELOAD
    if (nob_needs_rebuild1(BUILD_FOLDER "hotreload_posix.o", SRC_FOLDER "hotreload_posix.c")) {
        nob_log(NOB_INFO, "Rebuilding hotreload_posix.o");
        nob_cmd_append(cmd, "gcc");
        nob_cmd_append(cmd, "-c");
        add_debug_flags(cmd);
        nob_cmd_append(cmd, "-I.");
        nob_cmd_append(cmd, "-o", BUILD_FOLDER "hotreload_posix.o");
        nob_cmd_append(cmd, SRC_FOLDER "hotreload_posix.c");
        if (!nob_cmd_run_sync_and_reset(cmd)) return false;
    }
#endif

#ifdef LF_HOTRELOAD
    if (nob_needs_rebuild1(BUILD_FOLDER "log_frame_hr.o", SRC_FOLDER "log_frame.c")) {
        nob_log(NOB_INFO, "Rebuilding log_frame_hr.o");
#else
    if (nob_needs_rebuild1(BUILD_FOLDER "log_frame.o", SRC_FOLDER "log_frame.c")) {
        nob_log(NOB_INFO, "Rebuilding log_frame.o");
#endif
        nob_cmd_append(cmd, "gcc");
        nob_cmd_append(cmd, "-c");
        add_debug_flags(cmd);
        nob_cmd_append(cmd, "-I.");
        nob_cmd_append(cmd, "-I" THIRDPARTY_FOLDER "sokol");
#ifdef LF_HOTRELOAD
        nob_cmd_append(cmd, "-DSOKOL_GLCORE");
        nob_cmd_append(cmd, "-DSOKOL_DLL");
        nob_cmd_append(cmd, "-o", BUILD_FOLDER "log_frame_hr.o");
#else
        nob_cmd_append(cmd, "-DSOKOL_GLCORE");
        nob_cmd_append(cmd, "-o", BUILD_FOLDER "log_frame.o");
#endif
        nob_cmd_append(cmd, SRC_FOLDER "log_frame.c");
        if (!nob_cmd_run_sync_and_reset(cmd)) return false;
    }

#ifdef LF_HOTRELOAD
    const char *main_deps[] = {
        BUILD_FOLDER "log_frame_hr.o",
        BUILD_FOLDER "hotreload_posix.o",
    };

    if (nob_needs_rebuild(BUILD_FOLDER "log_frame", main_deps, sizeof(main_deps)/sizeof(main_deps[0]))) {
        nob_log(NOB_INFO, "Linking log_frame (hotreload)");
        nob_cmd_append(cmd, "gcc");
#ifdef LF_DEBUG
        nob_cmd_append(cmd, "-g");
#endif
        nob_cmd_append(cmd, "-o", BUILD_FOLDER "log_frame");
        for (size_t i = 0; i < sizeof(main_deps)/sizeof(main_deps[0]); i++) {
            nob_cmd_append(cmd, main_deps[i]);
        }
        nob_cmd_append(cmd, "-Wl,-rpath="BUILD_FOLDER, "-Wl,-rpath=./");
        nob_cmd_append(cmd, "-L"BUILD_FOLDER, "-l:libsokol.so");
        nob_cmd_append(cmd, "-lGL", "-lX11", "-lXcursor", "-lXrandr", "-lXinerama", "-lXi");
        nob_cmd_append(cmd, "-lasound", "-lpulse", "-lm", "-ldl", "-lpthread");
        if (!nob_cmd_run_sync_and_reset(cmd)) return false;
    }
#else
    if (nob_needs_rebuild1(BUILD_FOLDER "plug.o", SRC_FOLDER "plug.c")) {
        nob_log(NOB_INFO, "Rebuilding plug.o (static)");
        nob_cmd_append(cmd, "gcc");
        nob_cmd_append(cmd, "-c");
        add_debug_flags(cmd);
        nob_cmd_append(cmd, "-I.");
        nob_cmd_append(cmd, "-I" THIRDPARTY_FOLDER "sokol");
        nob_cmd_append(cmd, "-DSOKOL_GLCORE");
        nob_cmd_append(cmd, "-o", BUILD_FOLDER "plug.o");
        nob_cmd_append(cmd, SRC_FOLDER "plug.c");
        if (!nob_cmd_run_sync_and_reset(cmd)) return false;
    }

    nob_cmd_append(cmd, "gcc");
#ifdef LF_DEBUG
    nob_cmd_append(cmd, "-g");
#endif
    nob_cmd_append(cmd, "-o", BUILD_FOLDER "log_frame");
    nob_cmd_append(cmd, BUILD_FOLDER "log_frame.o");
    nob_cmd_append(cmd, BUILD_FOLDER "plug.o");
    nob_cmd_append(cmd, BUILD_FOLDER "sokol.o");
    nob_cmd_append(cmd, "-lGL", "-lX11", "-lXcursor", "-lXrandr", "-lXinerama", "-lXi");
    nob_cmd_append(cmd, "-lasound", "-lpulse", "-lm", "-ldl", "-lpthread");
    if (!nob_cmd_run_sync_and_reset(cmd)) return false;
#endif

    return true;
}

bool build_dist() {
#ifdef LF_HOTRELOAD
    nob_log(NOB_ERROR, "We do not ship with hotreload enabled");
    return false;
#else
    return false;
#endif
}
