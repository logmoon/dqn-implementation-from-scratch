#define LF_TARGET_NAME "win64-mingw"

#ifdef LF_DEBUG
void add_debug_flags(Nob_Cmd *cmd) {
    nob_cmd_append(cmd, "-g");           // Generate debug symbols
    nob_cmd_append(cmd, "-O0");          // No optimization for easier debugging
    nob_cmd_append(cmd, "-Wall");        // All warnings
    nob_cmd_append(cmd, "-Wextra");      // Extra warnings
}
#else
void add_debug_flags(Nob_Cmd *cmd) {
    // No debug flags
}
#endif // LF_DEBUG

bool build_sokol(Nob_Cmd *cmd) {
#ifdef LF_HOTRELOAD
    // Dynamic
    if (nob_needs_rebuild1(BUILD_FOLDER "libsokol.dll", THIRDPARTY_FOLDER "sokol/sokol.c")) {
        nob_log(NOB_INFO, "Building libsokol.dll");
        
        // First compile sokol.c with SOKOL_DLL defined
        nob_cmd_append(cmd, "x86_64-w64-mingw32-gcc");
        add_debug_flags(cmd);
        nob_cmd_append(cmd, "-I.");
        nob_cmd_append(cmd, "-mwin32"); // Required for sokol_audio
#if defined(LF_WIN64_RENDER_BACKEND_D3D11)
        nob_cmd_append(cmd, "-DSOKOL_D3D11");
#elif defined(LF_WIN64_RENDER_BACKEND_OPENGL)
        nob_cmd_append(cmd, "-DSOKOL_GLCORE");
#else
        nob_log(NOB_ERROR, "Windows render backend not configured...");
        return false;
#endif // LF_WIN_64_RENDER_BACKEND
        nob_cmd_append(cmd, "-DSOKOL_DLL");  // Define SOKOL_DLL for export
        nob_cmd_append(cmd, "-DSOKOL_NO_ENTRY"); // No sokol_main
        nob_cmd_append(cmd, "-fPIC"); // Position independent code
        nob_cmd_append(cmd, "-shared");
        nob_cmd_append(cmd, "-o", BUILD_FOLDER "libsokol.dll");
        nob_cmd_append(cmd, THIRDPARTY_FOLDER "sokol/sokol.c");
        nob_cmd_append(cmd, "-luser32", "-lgdi32", "-lshell32");
        nob_cmd_append(cmd, "-lole32"); // Required for sokol_audio
        // Add render backend libraries
#if defined(LF_WIN64_RENDER_BACKEND_D3D11)
        nob_cmd_append(cmd, "-ld3d11", "-ldxgi");
#elif defined(LF_WIN64_RENDER_BACKEND_OPENGL)
        nob_cmd_append(cmd, "-lopengl32");
#else
        nob_log(NOB_ERROR, "Windows render backend not configured...");
        return false;
#endif
        if (!nob_cmd_run_sync_and_reset(cmd)) return false;
    }
#else
    // Static
    if (nob_needs_rebuild1(BUILD_FOLDER "sokol.o", THIRDPARTY_FOLDER "sokol/sokol.c")) {
        nob_log(NOB_INFO, "Building sokol.o");
        nob_cmd_append(cmd, "x86_64-w64-mingw32-gcc", "-c");
        add_debug_flags(cmd);
        nob_cmd_append(cmd, "-I.");
        nob_cmd_append(cmd, "-mwin32"); // Required for sokol_audio
#if defined(LF_WIN64_RENDER_BACKEND_D3D11)
        nob_cmd_append(cmd, "-DSOKOL_D3D11");
#elif defined(LF_WIN64_RENDER_BACKEND_OPENGL)
        nob_cmd_append(cmd, "-DSOKOL_GLCORE");
#else
        nob_log(NOB_ERROR, "Windows render backend not configured...");
        return false;
#endif // LF_WIN_64_RENDER_BACKEND
        nob_cmd_append(cmd, "-o", BUILD_FOLDER "sokol.o");
        nob_cmd_append(cmd, THIRDPARTY_FOLDER "sokol/sokol.c");
        if (!nob_cmd_run_sync_and_reset(cmd)) return false;
    }
#endif // LF_HOTRELOAD

    return true;
}

// Reference: https://github.com/floooh/sokol-tools/blob/master/docs/sokol-shdc.md#standalone-usage
bool compile_shaders(Nob_Cmd* cmd) {
    nob_log(NOB_INFO, "Compiling main shader");
    nob_cmd_append(cmd, "./sokol-shdc");
    nob_cmd_append(cmd, "-i", SRC_FOLDER "main_shader.glsl");
    nob_cmd_append(cmd, "-o", SRC_FOLDER "main_shader.h");
#if defined(LF_WIN64_RENDER_BACKEND_D3D11)
    nob_cmd_append(cmd, "-l", "hlsl5");
#elif defined(LF_WIN64_RENDER_BACKEND_OPENGL)
    nob_cmd_append(cmd, "-l", "glsl430");
#else
    nob_log(NOB_ERROR, "Windows render backend not configured...");
    return false;
#endif
    if (!nob_cmd_run_sync_and_reset(cmd)) return false;

        nob_log(NOB_INFO, "Compiling lighting shader");
    nob_cmd_append(cmd, "./sokol-shdc");
    nob_cmd_append(cmd, "-i", SRC_FOLDER "lighting_shader.glsl");
    nob_cmd_append(cmd, "-o", SRC_FOLDER "lighting_shader.h");
#if defined(LF_WIN64_RENDER_BACKEND_D3D11)
    nob_cmd_append(cmd, "-l", "hlsl5");
#elif defined(LF_WIN64_RENDER_BACKEND_OPENGL)
    nob_cmd_append(cmd, "-l", "glsl430");
#else
    nob_log(NOB_ERROR, "Windows render backend not configured...");
    return false;
#endif
    if (!nob_cmd_run_sync_and_reset(cmd)) return false;

    return true;
}

bool build_libplug(Nob_Cmd* cmd) {
#ifdef LF_HOTRELOAD
    // Dynamic
    nob_log(NOB_INFO, "Building libplug.dll");
    nob_cmd_append(cmd, "x86_64-w64-mingw32-gcc");
    add_debug_flags(cmd);
    nob_cmd_append(cmd, "-I.");
    nob_cmd_append(cmd, "-I" THIRDPARTY_FOLDER "sokol");
    nob_cmd_append(cmd, "-mwin32"); // Required for sokol_audio
    nob_cmd_append(cmd, "-DSOKOL_DLL");  // Define SOKOL_DLL for import
    nob_cmd_append(cmd, "-fPIC"); // Position independent code for DLL
    nob_cmd_append(cmd, "-shared");
    nob_cmd_append(cmd, "-o", BUILD_FOLDER "libplug_temp.dll");
    nob_cmd_append(cmd, SRC_FOLDER "plug.c");
    nob_cmd_append(cmd, "-lole32");
    // Link against libsokol.dll
    nob_cmd_append(cmd, "-L"BUILD_FOLDER, "-l:libsokol.dll");
    nob_cmd_append(cmd, "-luser32", "-lgdi32", "-lshell32");
    // Add render backend libraries
#if defined(LF_WIN64_RENDER_BACKEND_D3D11)
    nob_cmd_append(cmd, "-ld3d11", "-ldxgi");
#elif defined(LF_WIN64_RENDER_BACKEND_OPENGL)
    nob_cmd_append(cmd, "-lopengl32");
#else
    nob_log(NOB_ERROR, "Windows render backend not configured...");
    return false;
#endif
    if (!nob_cmd_run_sync_and_reset(cmd)) return false;

    return true;
#else
    // Static: Do nothing
    return true;
#endif
}


bool build_log_frame(Nob_Cmd* cmd) {
#ifdef LF_HOTRELOAD
    if (nob_needs_rebuild1(BUILD_FOLDER "hotreload_windows.o", SRC_FOLDER "hotreload_windows.c")) {
        nob_log(NOB_INFO, "Rebuilding hotreload_windows.o");
        nob_cmd_append(cmd, "x86_64-w64-mingw32-gcc");
        nob_cmd_append(cmd, "-c");
        add_debug_flags(cmd);
        nob_cmd_append(cmd, "-I.");
        nob_cmd_append(cmd, "-mwin32"); // Required for sokol_audio
        nob_cmd_append(cmd, "-o", BUILD_FOLDER "hotreload_windows.o");
        nob_cmd_append(cmd, SRC_FOLDER "hotreload_windows.c");
        
        if (!nob_cmd_run_sync_and_reset(cmd)) return false;
    }
#endif

    // Compile main application
#ifdef LF_HOTRELOAD
    if (nob_needs_rebuild1(BUILD_FOLDER "log_frame_hr.o", SRC_FOLDER "log_frame.c")) {
        nob_log(NOB_INFO, "Rebuilding log_frame_hr.o");
#else
    if (nob_needs_rebuild1(BUILD_FOLDER "log_frame.o", SRC_FOLDER "log_frame.c")) {
        nob_log(NOB_INFO, "Rebuilding log_frame.o");
#endif // LF_HOTRELOAD
        nob_cmd_append(cmd, "x86_64-w64-mingw32-gcc");
        nob_cmd_append(cmd, "-c");
        add_debug_flags(cmd);
        nob_cmd_append(cmd, "-I.");
        nob_cmd_append(cmd, "-I" THIRDPARTY_FOLDER "sokol");
        nob_cmd_append(cmd, "-mwin32"); // Required for sokol_audio
#ifdef LF_HOTRELOAD
        nob_cmd_append(cmd, "-DSOKOL_DLL");
        nob_cmd_append(cmd, "-o", BUILD_FOLDER "log_frame_hr.o");
#else
        nob_cmd_append(cmd, "-o", BUILD_FOLDER "log_frame.o");
#endif // LF_HOTRELOAD
        nob_cmd_append(cmd, SRC_FOLDER "log_frame.c");
        
        if (!nob_cmd_run_sync_and_reset(cmd)) return false;
    }
    
#ifdef LF_HOTRELOAD
    const char *main_deps[] = {
        BUILD_FOLDER "log_frame_hr.o",
        BUILD_FOLDER "hotreload_windows.o",
    };
    
    if (nob_needs_rebuild(BUILD_FOLDER "log_frame.exe", main_deps, sizeof(main_deps)/sizeof(main_deps[0]))) {
        nob_log(NOB_INFO, "Linking log_frame.exe (hotreload)");
        nob_cmd_append(cmd, "x86_64-w64-mingw32-gcc");
#ifdef LF_DEBUG
        nob_cmd_append(cmd, "-g");  // Debug symbols in linker
#endif // LF_DEBUG
        nob_cmd_append(cmd, "-o", BUILD_FOLDER "log_frame.exe");
        
        // Add all object files
        for (size_t i = 0; i < sizeof(main_deps)/sizeof(main_deps[0]); i++) {
            nob_cmd_append(cmd, main_deps[i]);
        }
        nob_cmd_append(cmd, "-Wl,-rpath="BUILD_FOLDER, "-Wl,-rpath=./");
        nob_cmd_append(cmd, "-L"BUILD_FOLDER, "-l:libsokol.dll");
        
        nob_cmd_append(cmd, "-luser32", "-lgdi32", "-lshell32");
        nob_cmd_append(cmd, "-lole32"); // Required for sokol_audio
#if defined(LF_WIN64_RENDER_BACKEND_D3D11)
        nob_cmd_append(cmd, "-ld3d11", "-ldxgi");
#elif defined(LF_WIN64_RENDER_BACKEND_OPENGL)
        nob_cmd_append(cmd, "-lopengl32");
#else
        nob_log(NOB_ERROR, "Windows render backend not configured...");
        return false;
#endif
        
        if (!nob_cmd_run_sync_and_reset(cmd)) return false;
    }
#else
    // Static
    if (nob_needs_rebuild1(BUILD_FOLDER "plug.o", SRC_FOLDER "plug.c")) {
        nob_log(NOB_INFO, "Rebuilding plug.o (static)");
        nob_cmd_append(cmd, "x86_64-w64-mingw32-gcc");
        nob_cmd_append(cmd, "-c");
        add_debug_flags(cmd);
        nob_cmd_append(cmd, "-I.");
        nob_cmd_append(cmd, "-I" THIRDPARTY_FOLDER "sokol");
        nob_cmd_append(cmd, "-mwin32"); // Required for sokol_audio
        nob_cmd_append(cmd, "-o", BUILD_FOLDER "plug.o");
        nob_cmd_append(cmd, SRC_FOLDER "plug.c");
        
        if (!nob_cmd_run_sync_and_reset(cmd)) return false;
    }
    
    // Link everything statically
    nob_cmd_append(cmd, "x86_64-w64-mingw32-gcc");
#ifdef LF_DEBUG
    nob_cmd_append(cmd, "-g");  // Debug symbols in linker
#endif // LF_DEBUG
    nob_cmd_append(cmd, "-o", BUILD_FOLDER "log_frame.exe");
    nob_cmd_append(cmd, BUILD_FOLDER "log_frame.o");
    nob_cmd_append(cmd, BUILD_FOLDER "plug.o");
    nob_cmd_append(cmd, BUILD_FOLDER "sokol.o");
    nob_cmd_append(cmd, "-static");
    nob_cmd_append(cmd, "-luser32", "-lgdi32", "-lshell32");
    nob_cmd_append(cmd, "-lole32"); // Required for sokol_audio

#if defined(LF_WIN64_RENDER_BACKEND_D3D11)
    nob_cmd_append(cmd, "-ld3d11", "-ldxgi");
#elif defined(LF_WIN64_RENDER_BACKEND_OPENGL)
    nob_cmd_append(cmd, "-lopengl32");
#else
    nob_log(NOB_ERROR, "Windows render backend not configured...");
    return false;
#endif
    
    if (!nob_cmd_run_sync_and_reset(cmd)) return false;
#endif

    return true;
}

// Ships the game
bool build_dist()
{
#ifdef LF_HOTRELOAD
    nob_log(NOB_ERROR, "We do not ship with hotreload enabled");
    return false;
#else
    // TODO: Implement distribution packaging
    return false;
#endif // LF_HOTRELOAD
}