#define LF_TARGET_NAME "emscripten"

bool build_sokol(Nob_Cmd *cmd) {
    nob_cmd_append(cmd, "emcc", "-c");
    nob_cmd_append(cmd, "-I.");
    nob_cmd_append(cmd, "-I" THIRDPARTY_FOLDER "cimgui");
    nob_cmd_append(cmd, "-DSOKOL_GLES3");
    nob_cmd_append(cmd, "-o", BUILD_FOLDER "sokol.o");
    nob_cmd_append(cmd, THIRDPARTY_FOLDER "sokol/sokol.c");
    if (!nob_cmd_run_sync_and_reset(cmd)) return false;
    return true;
}

bool build_cimgui(Nob_Cmd *cmd) {
    const char *cimgui_sources[] = {
        THIRDPARTY_FOLDER "cimgui/cimgui.cpp",
        THIRDPARTY_FOLDER "cimgui/imgui.cpp",
        THIRDPARTY_FOLDER "cimgui/imgui_widgets.cpp",
        THIRDPARTY_FOLDER "cimgui/imgui_draw.cpp",
        THIRDPARTY_FOLDER "cimgui/imgui_tables.cpp",
        THIRDPARTY_FOLDER "cimgui/imgui_demo.cpp"
    };
    
    for (size_t i = 0; i < sizeof(cimgui_sources)/sizeof(cimgui_sources[0]); i++) {
        nob_cmd_append(cmd, "em++", "-c");
        nob_cmd_append(cmd, "-I.");
        nob_cmd_append(cmd, "-I" THIRDPARTY_FOLDER "cimgui");
        nob_cmd_append(cmd, "-o", nob_temp_sprintf(BUILD_FOLDER "%s.o", nob_path_name(cimgui_sources[i])));
        nob_cmd_append(cmd, cimgui_sources[i]);
        
        if (!nob_cmd_run_sync_and_reset(cmd)) {
            return false;
        }
    }
    
    return true;
}

bool build_log_frame(Nob_Cmd* cmd) {
    nob_cmd_append(cmd, "emcc");
    nob_cmd_append(cmd, "-c");
    nob_cmd_append(cmd, "-I.");
    nob_cmd_append(cmd, "-I" THIRDPARTY_FOLDER "cimgui");
    nob_cmd_append(cmd, "-I" THIRDPARTY_FOLDER "sokol");
    nob_cmd_append(cmd, "-o", BUILD_FOLDER "main.o");
    nob_cmd_append(cmd, SRC_FOLDER "main.c");

    if (!nob_cmd_run_sync_and_reset(cmd)) return false;

    nob_cmd_append(cmd, "emcc");
    nob_cmd_append(cmd, "-o", BUILD_FOLDER "demo.html");
    nob_cmd_append(cmd, BUILD_FOLDER "main.o");
    nob_cmd_append(cmd, BUILD_FOLDER "sokol.o");
    nob_cmd_append(cmd, BUILD_FOLDER "cimgui.cpp.o");
    nob_cmd_append(cmd, BUILD_FOLDER "imgui.cpp.o");
    nob_cmd_append(cmd, BUILD_FOLDER "imgui_widgets.cpp.o");
    nob_cmd_append(cmd, BUILD_FOLDER "imgui_draw.cpp.o");
    nob_cmd_append(cmd, BUILD_FOLDER "imgui_tables.cpp.o");
    nob_cmd_append(cmd, BUILD_FOLDER "imgui_demo.cpp.o");
    nob_cmd_append(cmd, "--shell-file", THIRDPARTY_FOLDER "sokol/shell.html");
    nob_cmd_append(cmd, "-sUSE_WEBGL2=1");
    nob_cmd_append(cmd, "-sNO_FILESYSTEM=1", "-sASSERTIONS=0", "-sMALLOC=emmalloc", "--closure=1");

    if (!nob_cmd_run_sync_and_reset(cmd)) return false;

    return true;
}

// Ships the game
bool build_dist()
{
#ifdef LF_HOT_RELOAD
    nob_log(NOB_ERROR, "We do not ship with hotreload enabled");
    return false;
#else
    // TODO: Implement
    return false;
#endif // LF_HOT_RELOAD
}