#ifndef UTILS_H_
#define UTILS_H_

// Type shit (tyshi)
#define u8 uint8_t
#define u32 uint32_t
#define u64 uint64_t
#define f32 float
#define f64 double

// Logging shit
#ifdef LF_LOG
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>

// Global log file pointer - initialize to NULL
static FILE* g_log_file = NULL;

#ifdef LF_LOG_FILE
// Determine minimum log level for file output
#if defined(LF_LOG_FILE_LEVEL_INFO)
    #define LF_LOG_FILE_MIN_LEVEL 0
#elif defined(LF_LOG_FILE_LEVEL_WARN)
    #define LF_LOG_FILE_MIN_LEVEL 1
#elif defined(LF_LOG_FILE_LEVEL_ERROR)
    #define LF_LOG_FILE_MIN_LEVEL 2
#else
    #define LF_LOG_FILE_MIN_LEVEL 0  // Default to INFO if no level specified
#endif // LF_LOG_FILE_LEVEL_?

// Log level enum
typedef enum {
    LOG_LEVEL_INFO = 0,
    LOG_LEVEL_WARN = 1,
    LOG_LEVEL_ERROR = 2,
    LOG_LEVEL_CRITICAL = 3
} LogLevel;

// Initialize logging system with optional file output
static inline void log_init(const char* log_filename) {
    if (log_filename && strlen(log_filename) > 0) {
        g_log_file = fopen(log_filename, "a");
        if (!g_log_file) {
            fprintf(stderr, "Warning: Could not open log file '%s'\n", log_filename);
        }
    }
}

// Cleanup logging system
static inline void log_cleanup(void) {
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
}
#else
#define log_init(...) ((void)0)
#define log_cleanup(...) ((void)0)
#endif // LF_LOG_FILE

// Get current timestamp as string
static inline void get_timestamp(char* buffer, size_t buffer_size) {
    time_t raw_time;
    struct tm* time_info;
    
    time(&raw_time);
    time_info = localtime(&raw_time);
    
    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", time_info);
}

// Core logging function
static inline void log_write(const char* level, const char* file, int line,
                             FILE* console_stream, 
                             #ifdef LF_LOG_FILE
                             LogLevel log_level,
                             #endif
                             const char* format, ...) {
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    va_list args;
    va_start(args, format);
    
    // Write to console
    fprintf(console_stream, "[%s] [LF_%s]: ", timestamp, level);
    vfprintf(console_stream, format, args);
    fprintf(console_stream, " (%s:%d)\n", file, line);
    
    #ifdef LF_LOG_FILE
    // Write to file if available and log level is sufficient
    if (g_log_file && log_level >= LF_LOG_FILE_MIN_LEVEL) {
        va_start(args, format); // Reset va_list for file output
        fprintf(g_log_file, "[%s] [LF_%s]: ", timestamp, level);
        vfprintf(g_log_file, format, args);
        fprintf(g_log_file, " (%s:%d)\n", file, line);
        fflush(g_log_file); // Ensure immediate write
    }
    #endif // LF_LOG_FILE
    
    va_end(args);
}

#ifdef LF_LOG_FILE
#define log_info(...) \
    log_write("INFO", __FILE__, __LINE__, stdout, LOG_LEVEL_INFO, __VA_ARGS__)
#define log_warn(...) \
    log_write("WARN", __FILE__, __LINE__, stdout, LOG_LEVEL_WARN, __VA_ARGS__)
#define log_error(...) \
    log_write("ERROR", __FILE__, __LINE__, stderr, LOG_LEVEL_ERROR, __VA_ARGS__)
#define log_critical(...) \
    do { \
        log_write("CRITICAL", __FILE__, __LINE__, stderr, LOG_LEVEL_CRITICAL, __VA_ARGS__); \
        log_cleanup(); \
        exit(EXIT_FAILURE); \
    } while(0)
#else
#define log_info(...) \
    log_write("INFO", __FILE__, __LINE__, stdout, __VA_ARGS__)
#define log_warn(...) \
    log_write("WARN", __FILE__, __LINE__, stdout, __VA_ARGS__)
#define log_error(...) \
    log_write("ERROR", __FILE__, __LINE__, stderr, __VA_ARGS__)
#define log_critical(...) \
    do { \
        log_write("CRITICAL", __FILE__, __LINE__, stderr, __VA_ARGS__); \
        log_cleanup(); \
        exit(EXIT_FAILURE); \
    } while(0)
#endif // LF_LOG_FILE
#else
#define log_init(...) ((void)0)
#define log_cleanup(...) ((void)0)
#define log_info(...) ((void)0)
#define log_warn(...) ((void)0)
#define log_error(...) ((void)0)
#define log_critical(...) ((void)0)
#endif // LF_LOG

// Constants
#define C_E        2.71828182845904523536   // e
#define C_LOG2E    1.44269504088896340736   // log2(e)
#define C_LOG10E   0.434294481903251827651  // log10(e)
#define C_LN2      0.693147180559945309417  // ln(2)
#define C_LN10     2.30258509299404568402   // ln(10)
#define C_PI       3.14159265358979323846   // pi
#define C_PI_2     1.57079632679489661923   // pi/2
#define C_PI_4     0.785398163397448309616  // pi/4
#define C_1_PI     0.318309886183790671538  // 1/pi
#define C_2_PI     0.636619772367581343076  // 2/pi
#define C_2_SQRTPI 1.12837916709551257390   // 2/sqrt(pi)
#define C_SQRT2    1.41421356237309504880   // sqrt(2)
#define C_SQRT1_2  0.707106781186547524401  // 1/sqrt(2)

// Easing
typedef enum {
    EASE_LINEAR,
    EASE_IN_QUAD,
    EASE_OUT_QUAD,
    EASE_IN_OUT_QUAD,
    EASE_IN_SINE,
    EASE_OUT_SINE,
    EASE_BOUNCE,
    EASE_ELASTIC
} ease_type_e;
float ease_function(ease_type_e type, float t) {
    switch (type) {
        case EASE_LINEAR: return t;
        case EASE_IN_QUAD: return t * t;
        case EASE_OUT_QUAD: return 1.0f - (1.0f - t) * (1.0f - t);
        case EASE_IN_OUT_QUAD: 
            return t < 0.5f ? 2.0f * t * t : 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
        case EASE_IN_SINE: return 1.0f - cosf(t * C_PI_2);
        case EASE_OUT_SINE: return sinf(t * C_PI_2);
        case EASE_BOUNCE: {
            if (t < 1.0f / 2.75f) return 7.5625f * t * t;
            else if (t < 2.0f / 2.75f) { t -= 1.5f / 2.75f; return 7.5625f * t * t + 0.75f; }
            else if (t < 2.5f / 2.75f) { t -= 2.25f / 2.75f; return 7.5625f * t * t + 0.9375f; }
            else { t -= 2.625f / 2.75f; return 7.5625f * t * t + 0.984375f; }
        }
        default: return t;
    }
}
static float smoothstep(float edge0, float edge1, float x) {
    float t = fmaxf(0.0f, fminf(1.0f, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0f - 2.0f * t);
}

// Random Shit (literally)
typedef struct {
    u32 seed;
} rng_state_t;

static int rng_init(rng_state_t* state, u32 seed) {
    state->seed = seed ? seed : 0x9E3779B9u;
}
static u32 rng_get(rng_state_t* state) {
    u32 x = state->seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state->seed = x;
    return x;
}
static float rng_float(rng_state_t* state, float min, float max) {
    float scale = (float)(rng_get(state) >> 8) * (1.0f / 16777216.0f); // Scale to [0, 1)
    return min + scale * (max - min);
}
static int rng_int(rng_state_t* state, int min, int max) {
    return min + (int)(rng_get(state) % (u32)(max - min + 1));
}
static float rng_angle(rng_state_t* state) {
    return rng_float(state, 0.0f, 2.0f * C_PI);
}

// Math Shit
typedef struct {
    float x, y;
} vec2_t;

typedef struct {
    float x, y, z;
} vec3_t;

typedef struct {
    float x, y, z, w;
} vec4_t;

typedef struct {
    int x, y;
} vec2i_t;

typedef struct {
    float m[16];
} mat4_t;

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static vec2_t vec2(float x, float y) {
    vec2_t v = {x, y};
    return v;
}

static vec2_t vec2_add(vec2_t a, vec2_t b) {
    return vec2(a.x + b.x, a.y + b.y);
}

static vec2_t vec2_subtract(vec2_t a, vec2_t b) {
    return vec2(a.x - b.x, a.y - b.y);
}

static vec2_t vec2_multiply(vec2_t a, vec2_t b) {
    return vec2(a.x * b.x, a.y * b.y);
}

static vec2_t vec2_scale(vec2_t v, float scale) {
    return vec2(v.x * scale, v.y * scale);
}

static float vec2_length(vec2_t v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

static vec2_t vec2_normalize(vec2_t v) {
    float length = vec2_length(v);
    if (length == 0) return vec2(0, 0);
    return vec2(v.x / length, v.y / length);
}

static vec2_t vec2_lerp(vec2_t a, vec2_t b, float t) {
    return vec2(
        lerp(a.x, b.x, t),
        lerp(a.y, b.y, t)
    );
}

static vec3_t vec3(float x, float y, float z) {
    vec3_t v = {x, y, z};
    return v;
}

static vec3_t vec3_lerp(vec3_t a, vec3_t b, float t) {
    return vec3(
        lerp(a.x, b.x, t),
        lerp(a.y, b.y, t),
        lerp(a.z, b.z, t)
    );
}

static vec4_t vec4(float x, float y, float z, float w) {
    vec4_t v = {x, y, z, w};
    return v;
}

static vec4_t vec4_subtract_scalar(vec4_t a, float s) {
    return vec4(a.x - s, a.y - s, a.z - s, a.w - s);
}

static mat4_t mat4_identity(void) {
    mat4_t m = {0};
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    return m;
}

static vec4_t vec4_lerp(vec4_t a, vec4_t b, float t) {
    return vec4(
        lerp(a.x, b.x, t),  // r
        lerp(a.y, b.y, t),  // g
        lerp(a.z, b.z, t),  // b
        lerp(a.w, b.w, t)   // a
    );
}

static mat4_t mat4_ortho(float l, float r, float b, float t, float n, float f) {
    mat4_t m = {0};
    m.m[0] = 2.0f / (r - l);
    m.m[5] = 2.0f / (t - b);
    m.m[10] = -2.0f / (f - n);
    m.m[12] = -(r + l) / (r - l);
    m.m[13] = -(t + b) / (t - b);
    m.m[14] = -(f + n) / (f - n);
    m.m[15] = 1.0f;
    return m;
}

static mat4_t mat4_translate(vec2_t pos) {
    mat4_t m = mat4_identity();
    m.m[12] = pos.x;
    m.m[13] = pos.y;
    return m;
}

static mat4_t mat4_scale(vec2_t scale) {
    mat4_t m = mat4_identity();
    m.m[0] = scale.x;
    m.m[5] = scale.y;
    return m;
}

static mat4_t mat4_rotate_z(float angle_rad) {
    mat4_t m = mat4_identity();
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);
    m.m[0] = c; m.m[1] = s;
    m.m[4] = -s; m.m[5] = c;
    return m;
}

static mat4_t mat4_multiply(mat4_t a, mat4_t b) {
    mat4_t result = {0};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                result.m[i * 4 + j] += a.m[i * 4 + k] * b.m[k * 4 + j];
            }
        }
    }
    return result;
}

static vec4_t mat4_transform_vec4(mat4_t m, vec4_t v) {
    vec4_t result;
    result.x = m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12] * v.w;
    result.y = m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13] * v.w;
    result.z = m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14] * v.w;
    result.w = m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15] * v.w;
    return result;
}

static float sine_breathe(float p) {
    return (sinf((p - 0.25f) * 2.0f * (float)C_PI) / 2.0f) + 0.5f;
}

typedef enum {
    PIVOT_BOTTOM_LEFT,
    PIVOT_BOTTOM_CENTER,
    PIVOT_BOTTOM_RIGHT,
    PIVOT_CENTER_LEFT,
    PIVOT_CENTER_CENTER,
    PIVOT_CENTER_RIGHT,
    PIVOT_TOP_LEFT,
    PIVOT_TOP_CENTER,
    PIVOT_TOP_RIGHT,
} pivot_t;

static vec2_t scale_from_pivot(pivot_t pivot) {
    switch (pivot) {
        case PIVOT_BOTTOM_LEFT: return vec2(0.0f, 0.0f);
        case PIVOT_BOTTOM_CENTER: return vec2(0.5f, 0.0f);
        case PIVOT_BOTTOM_RIGHT: return vec2(1.0f, 0.0f);
        case PIVOT_CENTER_LEFT: return vec2(0.0f, 0.5f);
        case PIVOT_CENTER_CENTER: return vec2(0.5f, 0.5f);
        case PIVOT_CENTER_RIGHT: return vec2(1.0f, 0.5f);
        case PIVOT_TOP_LEFT: return vec2(0.0f, 1.0f);
        case PIVOT_TOP_CENTER: return vec2(0.5f, 1.0f);
        case PIVOT_TOP_RIGHT: return vec2(1.0f, 1.0f);
    }
    return vec2(0.0f, 0.0f);
}

#endif // UTILS_H_