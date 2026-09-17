#define CONFIG_PATH "./build/config.h"

typedef struct {
    const char *macro;
    bool enabled_by_default;
} Target_Flag;

static Target_Flag target_flags[] = {
    {
        .macro = "LF_TARGET_LINUX",
        #if defined(linux) || defined(__linux) || defined(__linux__)
            .enabled_by_default = true,
        #else
            .enabled_by_default = false,
        #endif
    },
    {
        .macro = "LF_TARGET_WIN64_MINGW",
        #if (defined(WIN32) || defined(_WIN32)) && defined(__MINGW32__)
            .enabled_by_default = true,
        #else
            .enabled_by_default = false,
        #endif
    },
    {
        .macro = "LF_TARGET_WIN64_MSVC",
        #if (defined(WIN32) || defined(_WIN32)) && defined(_MSC_VER)
            .enabled_by_default = true,
        #else
            .enabled_by_default = false,
        #endif
    },
    {
        .macro = "LF_TARGET_EMSCRIPTEN",
        #if defined(__EMSCRIPTEN__)
            .enabled_by_default = true,
        #else
            .enabled_by_default = false,
        #endif
    },
    {
        .macro = "LF_TARGET_MACOS",
        #if defined(__APPLE__) || defined(__MACH__)
            .enabled_by_default = true,
        #else
            .enabled_by_default = false,
        #endif
    },
};

static Target_Flag win64_render_target_flags[] = {
    {
        .macro = "LF_WIN64_RENDER_BACKEND_D3D11",
        #if !(defined(WIN32) || defined(_WIN32))
            .enabled_by_default = false,
        #else
            .enabled_by_default = true,
        #endif
    },
    {
        .macro = "LF_WIN64_RENDER_BACKEND_OPENGL",
        #if !(defined(WIN32) || defined(_WIN32))
            .enabled_by_default = false,
        #else
            .enabled_by_default = false,
        #endif
    },
};

typedef struct Feature_Flag Feature_Flag;

struct Feature_Flag {
    const char *name;
    const char *macro;
    const char *description;
    bool enabled_by_default;
    Feature_Flag *subfeatures;
};

static Feature_Flag feature_flags[] = {
    {
        .macro = "LF_HOTRELOAD",
        .name = "hotreload",
        .description = "Moves everything in src/plug.c to a separate \"DLL\" so it can be hotreloaded.",
    },
    {
        .macro = "LF_DEBUG",
        .name = "debug",
        .description = "Adds debug symbols to the build for easier debugging.",
        .enabled_by_default = true,
    },
    {
        .macro = "LF_LOG",
        .name = "logging",
        .description = "Ability to log messages to the standard output and to a file.",
        .enabled_by_default = true,
        .subfeatures = (Feature_Flag[]){
            {
                .macro = "LF_LOG_FILE",
                .name = "log_file",
                .description = "Enables logging to a file.",
                .enabled_by_default = true,
            },
            {
                .macro = "LF_LOG_FILE_LEVEL_INFO",
                .name = "log_file_level_info",
                .description = "The min log level to write to the file",
                .enabled_by_default = false,
            },
            {
                .macro = "LF_LOG_FILE_LEVEL_WARN",
                .name = "log_file_level_warn",
                .description = "The min log level to write to the file",
                .enabled_by_default = true,
            },
            {
                .macro = "LF_LOG_FILE_LEVEL_ERROR",
                .name = "log_file_level_error",
                .description = "The min log level to write to the file",
                .enabled_by_default = false,
            },
            {0} // Null terminator
        },
    },
    /*
    {
        .macro = "LF_EXAMPLE_FEATURE",
        .name = "example_feature",
        .description = "Example feature",
        .enabled_by_default = true,
        .subfeatures = (Feature_Flag[]) {
            {
                .macro = "LF_EXAMPLE_SUBFEATURE",
                .name = "example_subfeature",
                .description = "Example subfeature",
                .enabled_by_default = true,
            },
            {0} // Null terminator
        }
    },
    */
};

#define genf(out, ...) \
    do { \
        fprintf((out), __VA_ARGS__); \
        fprintf((out), "\n"); \
    } while(0)

void write_feature_flag(FILE *f, Feature_Flag *flag, int indent_level)
{
    // Generate indentation
    for (int i = 0; i < indent_level; ++i) {
        fprintf(f, "    ");
    }
    
    if (flag->enabled_by_default) {
        nob_log(NOB_INFO, "%*s%s: ENABLED", indent_level * 2, "", flag->name);
        fprintf(f, "#define %s", flag->macro);
    } else {
        nob_log(NOB_INFO, "%*s%s: DISABLED", indent_level * 2, "", flag->name);
        fprintf(f, "// #define %s", flag->macro);
    }
    
    if (flag->description) {
        fprintf(f, " // %s\n", flag->description);
    } else {
        fprintf(f, "\n");
    }
    
    // Recursively process subfeatures
    if (flag->subfeatures) {
        Feature_Flag *subflag = flag->subfeatures;
        while (subflag->macro) {
            write_feature_flag(f, subflag, indent_level + 1);
            subflag++;
        }
    }
}

bool generate_default_config(const char *file_path)
{
    nob_log(NOB_INFO, "Generating %s", file_path);
    FILE *f = fopen(file_path, "wb");
    if (f == NULL) {
        nob_log(NOB_ERROR, "Could not generate %s: %s", file_path, strerror(errno));
        return false;
    }

    // TODO: generate_default_config() should also log what platform it picked
    fprintf(f, "//// Build target. Pick only one!\n");
    for (size_t i = 0; i < NOB_ARRAY_LEN(target_flags); ++i) {
        if (target_flags[i].enabled_by_default) {
            fprintf(f, "#define %s\n", target_flags[i].macro);
        } else {
            fprintf(f, "// #define %s\n", target_flags[i].macro);
        }
    }

    fprintf(f, "\n");

    fprintf(f, "//// Windows render backend. Pick only one! (ONLY WORKS ON WINDOWS)\n");
    for (size_t i = 0; i < NOB_ARRAY_LEN(win64_render_target_flags); ++i) {
        if (win64_render_target_flags[i].enabled_by_default) {
            fprintf(f, "#define %s\n", win64_render_target_flags[i].macro);
        } else {
            fprintf(f, "// #define %s\n", win64_render_target_flags[i].macro);
        }
    }

    fprintf(f, "\n");

    for (size_t i = 0; i < NOB_ARRAY_LEN(feature_flags); ++i) {
        if (i > 0) fprintf(f, "\n"); // Add blank line between features
        fprintf(f, "//// %s\n", feature_flags[i].description);
        write_feature_flag(f, &feature_flags[i], 0);
    }

    fprintf(f, "\n");
    fprintf(f, "//// Window stuff\n");
    fprintf(f, "#define LF_WINDOW_TITLE  \"Hello log.frame\"\n");
    fprintf(f, "#define LF_WINDOW_WIDTH  800\n");
    fprintf(f, "#define LF_WINDOW_HEIGHT 600\n");
    fprintf(f, "#define LF_GAME_WIDTH   640 // The width the game renders at\n");
    fprintf(f, "#define LF_GAME_HEIGHT  360 // The height the game renders at\n");

    fclose(f);
    return true;
}

void generate_config_logger_for_flag(FILE *f, Feature_Flag *flag, int indent_level)
{
    for (int i = 0; i < indent_level; ++i) {
        fprintf(f, "    ");
    }
    genf(f, "#ifdef %s", flag->macro);
    
    for (int i = 0; i < indent_level; ++i) {
        fprintf(f, "    ");
    }
    genf(f, "    nob_log(level, \"%*s%s: ENABLED\");", indent_level * 2, "", flag->name);
    
    if (flag->subfeatures) {
        Feature_Flag *subflag = flag->subfeatures;
        while (subflag->macro) {
            generate_config_logger_for_flag(f, subflag, indent_level + 1);
            subflag++;
        }
    }
    
    for (int i = 0; i < indent_level; ++i) {
        fprintf(f, "    ");
    }
    genf(f, "#else");
    
    for (int i = 0; i < indent_level; ++i) {
        fprintf(f, "    ");
    }
    genf(f, "    nob_log(level, \"%*s%s: DISABLED\");", indent_level * 2, "", flag->name);
    
    for (int i = 0; i < indent_level; ++i) {
        fprintf(f, "    ");
    }
    genf(f, "#endif");
}

bool generate_config_logger(const char *config_logger_path)
{
    nob_log(NOB_INFO, "Generating %s", config_logger_path);
    FILE *f = fopen(config_logger_path, "wb");
    if (f == NULL) {
        nob_log(NOB_ERROR, "Could not generate %s: %s", config_logger_path, strerror(errno));
        return false;
    }

    genf(f, "void log_config(Nob_Log_Level level)");
    genf(f, "{");
    genf(f, "    nob_log(level, \"Target: %%s\", LF_TARGET_NAME);");
    
    for (size_t i = 0; i < NOB_ARRAY_LEN(feature_flags); ++i) {
        generate_config_logger_for_flag(f, &feature_flags[i], 1);
    }
    
    genf(f, "}");

    fclose(f);
    return true;
}