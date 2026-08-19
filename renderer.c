#define _POSIX_C_SOURCE 200809L

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2platform.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <png.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-egl.h>

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#define MAX_OUTPUTS 16
#define DEFAULT_SNAPSHOT_RELATIVE ".cache/orbit-wallpaper-engine"
#define DEFAULT_BACKGROUND_RELATIVE ".cache/orbit-wallpaper-engine/hyprlock-background.conf"
#define DEFAULT_CONTROL_RELATIVE ".cache/orbit-wallpaper-engine/control"
#define DEFAULT_SHADER_FILENAME "wave.frag"
#define SHADER_DIRECTORY "shaders"
#define DEFAULT_GPU_PRESSURE_ENTER 75.0f
#define DEFAULT_GPU_PRESSURE_EXIT 45.0f
#define CPU_PRESSURE_ENTER 0.90
#define CPU_PRESSURE_EXIT 0.65
#define DEFAULT_TARGET_FPS 60.0f
#define DEFAULT_RESOURCE_GOVERNOR true
#define DEFAULT_RENDER_SCALE 1.0f
#define DEFAULT_SHADER_SPEED 1.0f
#define PRESSURE_CONFIRM_SECONDS 3.0
#define RECOVERY_CONFIRM_SECONDS 10.0
#define DEFAULT_INTRO_DURATION_SECONDS 4.5f
#define DEFAULT_EXIT_DURATION_SECONDS 1.0f
#define DEFAULT_INTRO_PEAK_SPEED 34.0f
#define DEFAULT_INTRO_PEAK_START 0.05f
#define DEFAULT_INTRO_PEAK_END 0.08f
#define DEFAULT_INTRO_REVEAL_END 0.22f
#define DEFAULT_INTRO_DECAY 10.0f
#define DEFAULT_AUTO_PALETTE_STRENGTH 0.72f
#define DEFAULT_PEAK_BRIGHTNESS 1.0f

struct color { float r, g, b; };

struct output {
    struct app *app;
    struct wl_output *wl_output;
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer;
    struct wl_egl_window *egl_window;
    EGLSurface egl_surface;
    GLuint render_fbo;
    GLuint render_texture;
    int32_t render_width, render_height;
    int32_t x, y;
    int32_t mode_width, mode_height;
    int32_t width, height;
    uint32_t configure_serial;
    bool configured;
    bool closed;
    char name[128];
};

struct app {
    struct wl_display *display;
    struct wl_compositor *compositor;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct output outputs[MAX_OUTPUTS];
    size_t output_count;
    int min_x, min_y, max_x, max_y;
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLConfig egl_config;
    GLuint program;
    GLuint blit_program;
    GLint blit_texture;
    GLint resolution, resolution_alias, i_resolution;
    GLint origin, canvas;
    GLint time_uniform, time_alias, u_time_alias, i_time;
    GLint i_time_delta, i_frame, i_frame_rate, i_mouse;
    GLint brightness, visibility;
    GLint primary, secondary, surface, error;
    GLint auto_palette_strength;
    GLint renderer_brightness;
    struct color colors[4];
    struct color target_colors[4];
    time_t palette_mtime;
    bool follow_system_palette;
    bool snapshot_dirty;
    bool capture_snapshots;
    bool debug_frames;
    bool frozen;
    bool resource_governor;
    float target_fps;
    float render_scale;
    float shader_speed;
    float gpu_pressure_enter;
    float gpu_pressure_exit;
    double pressure_since;
    double recovery_since;
    char gpu_busy_path[128];
    bool gpu_path_checked;
    char palette_path[PATH_MAX];
    char snapshot_dir[PATH_MAX];
    char background_path[PATH_MAX];
    char control_path[PATH_MAX];
    int control_fd;
    float intro_duration;
    float exit_duration;
    float intro_peak_speed;
    float intro_peak_start;
    float intro_peak_end;
    float intro_reveal_end;
    float intro_decay;
    float auto_palette_strength_value;
    float peak_brightness;
    float shader_time_delta;
    int shader_frame;
};

enum animation_mode { ANIMATION_NORMAL, ANIMATION_INTRO, ANIMATION_EXIT, ANIMATION_HIDDEN };

static volatile sig_atomic_t running = 1;

static void stop_handler(int signal_number) {
    (void)signal_number;
    running = 0;
}

static double monotonic_seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1e9;
}

static const char *environment_value(const char *name, const char *legacy_name) {
    const char *value = getenv(name);
    if (value && *value) return value;
    if (legacy_name && *legacy_name) {
        value = getenv(legacy_name);
        if (value && *value) return value;
    }
    return NULL;
}

static float environment_float_compat(const char *name, const char *legacy_name,
                                      float fallback, float minimum, float maximum) {
    const char *value = environment_value(name, legacy_name);
    if (!value) return fallback;
    char *end = NULL;
    float parsed = strtof(value, &end);
    if (end == value || *end != '\0' || parsed < minimum || parsed > maximum) {
        fprintf(stderr, "invalid %s; using %.3f\n", name, fallback);
        return fallback;
    }
    return parsed;
}

static bool environment_bool_compat(const char *name, const char *legacy_name,
                                    bool fallback) {
    const char *value = environment_value(name, legacy_name);
    if (!value) return fallback;
    if (!strcasecmp(value, "1") || !strcasecmp(value, "true") ||
        !strcasecmp(value, "yes") || !strcasecmp(value, "on")) return true;
    if (!strcasecmp(value, "0") || !strcasecmp(value, "false") ||
        !strcasecmp(value, "no") || !strcasecmp(value, "off")) return false;
    fprintf(stderr, "invalid %s; using %s\n", name, fallback ? "true" : "false");
    return fallback;
}

static void sleep_seconds(double seconds) {
    if (seconds <= 0.0) return;

    struct timespec pause = {
        .tv_sec = (time_t)seconds,
        .tv_nsec = (long)((seconds - (double)(time_t)seconds) * 1e9),
    };

    while (nanosleep(&pause, &pause) != 0 && errno == EINTR && running) {
    }
}

static bool read_gpu_busy(struct app *app, double *busy) {
    if (!app->gpu_path_checked) {
        app->gpu_path_checked = true;
        for (int card = 0; card < 10; card++) {
            char path[sizeof(app->gpu_busy_path)];
            snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/gpu_busy_percent", card);
            if (access(path, R_OK) == 0) {
                snprintf(app->gpu_busy_path, sizeof(app->gpu_busy_path), "%s", path);
                break;
            }
        }
    }
    if (!app->gpu_busy_path[0]) return false;

    FILE *file = fopen(app->gpu_busy_path, "r");
    if (!file) return false;
    int value = 0;
    bool result = fscanf(file, "%d", &value) == 1;
    fclose(file);
    if (result) *busy = value;
    return result;
}

static bool read_cpu_load(double *load) {
    FILE *file = fopen("/proc/loadavg", "r");
    if (!file) return false;
    bool result = fscanf(file, "%lf", load) == 1;
    fclose(file);
    return result;
}

static bool resource_pressure(struct app *app, double *gpu_busy, double *cpu_load) {
    bool gpu_available = read_gpu_busy(app, gpu_busy);
    bool cpu_available = read_cpu_load(cpu_load);
    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count < 1) cpu_count = 1;

    bool gpu_pressure = gpu_available && *gpu_busy >= app->gpu_pressure_enter;
    bool cpu_pressure = cpu_available && *cpu_load >= CPU_PRESSURE_ENTER * cpu_count;
    return gpu_pressure || cpu_pressure;
}

static bool resources_recovered(struct app *app, double *gpu_busy, double *cpu_load) {
    bool gpu_available = read_gpu_busy(app, gpu_busy);
    bool cpu_available = read_cpu_load(cpu_load);
    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count < 1) cpu_count = 1;

    bool gpu_recovered = !gpu_available || *gpu_busy <= app->gpu_pressure_exit;
    bool cpu_recovered = !cpu_available || *cpu_load <= CPU_PRESSURE_EXIT * cpu_count;
    return gpu_recovered && cpu_recovered;
}

static struct color hex_color(const char *value) {
    unsigned int rgb = 0;
    const char *start = strchr(value, '#');
    if (!start) {
        start = strchr(value, '(');
        if (start) start++;
    } else {
        start++;
    }
    if (!start || sscanf(start, "%6x", &rgb) != 1) {
        return (struct color){0.2f, 0.4f, 0.7f};
    }
    return (struct color){
        ((rgb >> 16) & 0xff) / 255.0f,
        ((rgb >> 8) & 0xff) / 255.0f,
        (rgb & 0xff) / 255.0f,
    };
}

static struct color environment_color_compat(const char *name, const char *legacy_name,
                                             const char *fallback) {
    const char *value = environment_value(name, legacy_name);
    return hex_color(value && *value ? value : fallback);
}


static struct color wallpaper_base_color(struct color surface) {
    float luminance = surface.r * 0.2126f + surface.g * 0.7152f + surface.b * 0.0722f;
    struct color rich = {
        luminance + (surface.r - luminance) * 1.18f,
        luminance + (surface.g - luminance) * 1.18f,
        luminance + (surface.b - luminance) * 1.18f,
    };
    return (struct color){
        fmaxf(rich.r * 0.2f, 0.003f),
        fmaxf(rich.g * 0.2f, 0.003f),
        fmaxf(rich.b * 0.2f, 0.003f),
    };
}

static bool write_background_color(struct app *app) {
    if (!app->background_path[0]) return true;
    struct color color = wallpaper_base_color(app->target_colors[2]);
    char temporary_path[PATH_MAX];
    snprintf(temporary_path, sizeof(temporary_path), "%s.tmp.XXXXXX", app->background_path);
    int descriptor = mkstemp(temporary_path);
    if (descriptor < 0) return false;
    FILE *file = fdopen(descriptor, "w");
    if (!file) {
        close(descriptor);
        unlink(temporary_path);
        return false;
    }
    fprintf(file, "background {\n    monitor =\n    color = rgb(%02x%02x%02x)\n}\n",
            (unsigned int)lroundf(color.r * 255.0f),
            (unsigned int)lroundf(color.g * 255.0f),
            (unsigned int)lroundf(color.b * 255.0f));
    if (fclose(file) != 0 || rename(temporary_path, app->background_path) != 0) {
        unlink(temporary_path);
        return false;
    }
    return true;
}

static bool open_control(struct app *app) {
    if (mkfifo(app->control_path, 0600) != 0 && errno != EEXIST) return false;
    app->control_fd = open(app->control_path, O_RDWR | O_NONBLOCK);
    return app->control_fd >= 0;
}

static int read_animation_request(struct app *app) {
    char commands[64];
    ssize_t length = read(app->control_fd, commands, sizeof(commands) - 1);
    if (length <= 0) return 0;
    commands[length] = '\0';
    if (strstr(commands, "intro")) return 1;
    if (strstr(commands, "exit")) return 2;
    if (strstr(commands, "palette")) return 3;
    return 0;
}

static bool read_palette(struct app *app) {
    if (!app->follow_system_palette) return false;
    struct stat file_stat;
    if (stat(app->palette_path, &file_stat) != 0 || file_stat.st_mtime == app->palette_mtime) {
        return false;
    }

    FILE *file = fopen(app->palette_path, "r");
    if (!file) return false;
    char content[16384];
    size_t length = fread(content, 1, sizeof(content) - 1, file);
    fclose(file);
    content[length] = '\0';

    const char *names[] = {"primary", "secondary", "surface", "error"};
    for (int i = 0; i < 4; i++) {
        char needle[64];
        snprintf(needle, sizeof(needle), "local %s", names[i]);
        char *match = strstr(content, needle);
        if (!match) continue;
        char *quote = strchr(match, '"');
        if (quote) app->target_colors[i] = hex_color(quote + 1);
    }
    app->palette_mtime = file_stat.st_mtime;
    write_background_color(app);
    app->snapshot_dirty = app->capture_snapshots;
    return true;
}

static bool xdg_home_path(char *buffer, size_t size, const char *environment_name,
                          const char *home_suffix) {
    if (!buffer || size < 2 || !environment_name || !home_suffix) return false;

    const char *xdg_home = getenv(environment_name);
    if (xdg_home && *xdg_home) {
        if (xdg_home[0] != '/') {
            fprintf(stderr, "%s must be an absolute path; ignoring it\n", environment_name);
        } else if (snprintf(buffer, size, "%s", xdg_home) < (int)size) {
            return true;
        } else {
            fprintf(stderr, "%s is too long; using HOME fallback\n", environment_name);
        }
    }

    const char *home = getenv("HOME");
    if (!home || !*home) return false;
    return snprintf(buffer, size, "%s/%s", home, home_suffix) < (int)size;
}

static bool shader_filename_valid(const char *name) {
    if (!name || !*name) return false;
    if (strchr(name, '/')) return false;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return false;
    return true;
}

static bool resolve_shader_path(int argc, char **argv, char *path, size_t size,
                                char *fallback, size_t fallback_size) {
    char config_home[PATH_MAX];
    char data_home[PATH_MAX];

    if (!xdg_home_path(config_home, sizeof(config_home),
                       "XDG_CONFIG_HOME", ".config")) {
        fprintf(stderr, "cannot determine XDG config directory\n");
        return false;
    }
    if (!xdg_home_path(data_home, sizeof(data_home),
                       "XDG_DATA_HOME", ".local/share")) {
        fprintf(stderr, "cannot determine XDG data directory\n");
        return false;
    }

    if (snprintf(fallback, fallback_size, "%s/%s/%s",
                 data_home, "orbit-wallpaper-engine", DEFAULT_SHADER_FILENAME)
        >= (int)fallback_size) {
        fprintf(stderr, "default shader path is too long\n");
        return false;
    }

    // An explicit command-line path remains the highest-priority debugging override.
    if (argc > 1 && argv[1] && *argv[1]) {
        if (snprintf(path, size, "%s", argv[1]) >= (int)size) {
            fprintf(stderr, "shader path is too long\n");
            return false;
        }
        return true;
    }

    const char *selected = environment_value("ORBIT_WALLPAPER_SHADER", "PS3_WAVE_SHADER");
    if (selected && *selected) {
        if (!shader_filename_valid(selected)) {
            fprintf(stderr,
                    "invalid ORBIT_WALLPAPER_SHADER '%s'; expected a filename inside "
                    "$XDG_CONFIG_HOME/orbit-wallpaper-engine/%s/; using %s\n",
                    selected, SHADER_DIRECTORY, DEFAULT_SHADER_FILENAME);
            snprintf(path, size, "%s", fallback);
            return true;
        }

        if (snprintf(path, size, "%s/%s/%s/%s",
                     config_home, "orbit-wallpaper-engine", SHADER_DIRECTORY, selected)
            >= (int)size) {
            fprintf(stderr,
                    "selected shader path is too long; using %s\n",
                    DEFAULT_SHADER_FILENAME);
            snprintf(path, size, "%s", fallback);
        }
        return true;
    }

    snprintf(path, size, "%s", fallback);
    return true;
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    if (fseek(file, 0, SEEK_END) != 0) { fclose(file); return NULL; }
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0) { fclose(file); return NULL; }
    char *data = calloc((size_t)length + 1, 1);
    if (data) fread(data, 1, (size_t)length, file);
    fclose(file);
    return data;
}

static bool source_has_uniform(const char *source, const char *name) {
    const char *match = source;
    size_t name_length = strlen(name);
    while ((match = strstr(match, name)) != NULL) {
        bool left_ok = match == source ||
            !((match[-1] >= 'a' && match[-1] <= 'z') ||
              (match[-1] >= 'A' && match[-1] <= 'Z') ||
              (match[-1] >= '0' && match[-1] <= '9') || match[-1] == '_');
        char right = match[name_length];
        bool right_ok = !((right >= 'a' && right <= 'z') ||
                          (right >= 'A' && right <= 'Z') ||
                          (right >= '0' && right <= '9') || right == '_');
        if (left_ok && right_ok) {
            const char *line = match;
            while (line > source && line[-1] != '\n') line--;
            const char *uniform = strstr(line, "uniform");
            if (uniform && uniform < match) return true;
        }
        match += name_length;
    }
    return false;
}

static bool source_uses_identifier(const char *source, const char *name) {
    const char *match = source;
    size_t name_length = strlen(name);
    while ((match = strstr(match, name)) != NULL) {
        bool left_ok = match == source ||
            !((match[-1] >= 'a' && match[-1] <= 'z') ||
              (match[-1] >= 'A' && match[-1] <= 'Z') ||
              (match[-1] >= '0' && match[-1] <= '9') || match[-1] == '_');
        char right = match[name_length];
        bool right_ok = !((right >= 'a' && right <= 'z') ||
                          (right >= 'A' && right <= 'Z') ||
                          (right >= '0' && right <= '9') || right == '_');
        if (left_ok && right_ok) return true;
        match += name_length;
    }
    return false;
}

static bool source_has_main(const char *source) {
    const char *p = source;
    while ((p = strstr(p, "main")) != NULL) {
        const char *before = p;
        while (before > source && (before[-1] == ' ' || before[-1] == '\t' || before[-1] == '\r' || before[-1] == '\n')) before--;
        if ((size_t)(before - source) >= 4 && strncmp(before - 4, "void", 4) == 0) {
            const char *after = p + 4;
            while (*after == ' ' || *after == '\t' || *after == '\r' || *after == '\n') after++;
            if (*after == '(') return true;
        }
        p += 4;
    }
    return false;
}

static char *prepare_fragment_shader(const char *source) {
    bool has_version = strncmp(source, "#version", 8) == 0;
    const char *body = source;
    char version_line[128] = {0};

    if (has_version) {
        const char *newline = strchr(source, '\n');
        size_t length = newline ? (size_t)(newline - source + 1) : strlen(source);
        if (length >= sizeof(version_line)) length = sizeof(version_line) - 1;
        memcpy(version_line, source, length);
        version_line[length] = '\0';
        body = newline ? newline + 1 : source + strlen(source);
    }

    bool has_main = source_has_main(source);
    bool has_main_image = strstr(source, "mainImage") != NULL;

    bool need_i_time = source_uses_identifier(source, "iTime") &&
                       !source_has_uniform(source, "iTime");
    bool need_i_resolution = source_uses_identifier(source, "iResolution") &&
                             !source_has_uniform(source, "iResolution");
    bool need_i_time_delta = source_uses_identifier(source, "iTimeDelta") &&
                             !source_has_uniform(source, "iTimeDelta");
    bool need_i_frame = source_uses_identifier(source, "iFrame") &&
                        !source_has_uniform(source, "iFrame");
    bool need_i_frame_rate = source_uses_identifier(source, "iFrameRate") &&
                             !source_has_uniform(source, "iFrameRate");
    bool need_i_mouse = source_uses_identifier(source, "iMouse") &&
                        !source_has_uniform(source, "iMouse");

    // If a shader already declares any of Orbit's palette uniforms, assume it
    // intentionally handles the system palette itself (as the original wave
    // shader does). Otherwise inject a generic final-colour palette grade.
    bool palette_aware = source_has_uniform(source, "u_primary") ||
                         source_has_uniform(source, "u_secondary") ||
                         source_has_uniform(source, "u_surface") ||
                         source_has_uniform(source, "u_error");
    bool auto_palette = !palette_aware;

    // The compatibility layer injects float/vector uniforms before the user's
    // shader body. GLES therefore needs a default float precision established
    // here even when the original shader declares its own precision later.
    const char *precision = "precision highp float;\n";
    const char *i_time_decl = need_i_time ? "uniform float iTime;\n" : "";
    const char *i_resolution_decl = need_i_resolution ? "uniform vec3 iResolution;\n" : "";
    const char *i_time_delta_decl = need_i_time_delta ? "uniform float iTimeDelta;\n" : "";
    const char *i_frame_decl = need_i_frame ? "uniform int iFrame;\n" : "";
    const char *i_frame_rate_decl = need_i_frame_rate ? "uniform float iFrameRate;\n" : "";
    const char *i_mouse_decl = need_i_mouse ? "uniform vec4 iMouse;\n" : "";
    const char *brightness_support = "uniform float u_orbit_renderer_brightness;\n";

    // GLSL ES 1.00 lacks several functions commonly used by desktop/Shadertoy
    // shaders. Inject narrowly-scoped compatibility polyfills only when used.
    bool need_tanh = source_uses_identifier(source, "tanh");
    const char *math_compat = need_tanh ?
        "float orbit_tanh_scalar(float x) {\n"
        "    x = clamp(x, -20.0, 20.0);\n"
        "    float e = exp(2.0 * x);\n"
        "    return (e - 1.0) / (e + 1.0);\n"
        "}\n"
        "float tanh(float x) { return orbit_tanh_scalar(x); }\n"
        "vec2 tanh(vec2 v) { return vec2(orbit_tanh_scalar(v.x), orbit_tanh_scalar(v.y)); }\n"
        "vec3 tanh(vec3 v) { return vec3(orbit_tanh_scalar(v.x), orbit_tanh_scalar(v.y), orbit_tanh_scalar(v.z)); }\n"
        "vec4 tanh(vec4 v) { return vec4(orbit_tanh_scalar(v.x), orbit_tanh_scalar(v.y), orbit_tanh_scalar(v.z), orbit_tanh_scalar(v.w)); }\n"
        : "";

    const char *palette_support = auto_palette ?
        "uniform vec3 u_primary;\n"
        "uniform vec3 u_secondary;\n"
        "uniform vec3 u_surface;\n"
        "uniform vec3 u_error;\n"
        "uniform float u_orbit_palette_strength;\n"
        "vec3 orbit_apply_palette(vec3 source_color) {\n"
        "    float luma = dot(source_color, vec3(0.2126, 0.7152, 0.0722));\n"
        "    float hi = max(source_color.r, max(source_color.g, source_color.b));\n"
        "    float lo = min(source_color.r, min(source_color.g, source_color.b));\n"
        "    float chroma = max(hi - lo, 0.0);\n"
        "    vec3 dark_color = max(u_surface * 0.24, vec3(0.003));\n"
        "    vec3 mid_color = mix(u_secondary, u_primary, 0.38);\n"
        "    vec3 high_color = mix(u_primary, vec3(1.0), 0.32);\n"
        "    vec3 mapped = mix(dark_color, mid_color, smoothstep(0.02, 0.58, luma));\n"
        "    mapped = mix(mapped, high_color, smoothstep(0.48, 1.0, luma));\n"
        "    vec3 accent = mix(u_primary, u_error, 0.34);\n"
        "    mapped = mix(mapped, accent, smoothstep(0.42, 1.0, chroma) * 0.16);\n"
        "    float mapped_luma = dot(mapped, vec3(0.2126, 0.7152, 0.0722));\n"
        "    mapped *= (luma + 0.035) / (mapped_luma + 0.035);\n"
        "    return mix(source_color, mapped, u_orbit_palette_strength);\n"
        "}\n" : "";

    const char *rename_main = has_main ? "#define main orbit_user_main\n" : "";
    const char *restore_main = has_main ? "\n#undef main\n" : "";

    const char *wrapper = "";
    if (has_main && auto_palette) {
        wrapper =
            "void main() {\n"
            "    orbit_user_main();\n"
            "    gl_FragColor.rgb = orbit_apply_palette(gl_FragColor.rgb);\n"
            "    gl_FragColor.rgb *= u_orbit_renderer_brightness;\n"
            "}\n";
    } else if (has_main) {
        wrapper =
            "void main() {\n"
            "    orbit_user_main();\n"
            "    gl_FragColor.rgb *= u_orbit_renderer_brightness;\n"
            "}\n";
    } else if (has_main_image && auto_palette) {
        wrapper =
            "\nvoid main() {\n"
            "    vec4 orbit_color = vec4(0.0);\n"
            "    mainImage(orbit_color, gl_FragCoord.xy);\n"
            "    orbit_color.rgb = orbit_apply_palette(orbit_color.rgb);\n"
            "    orbit_color.rgb *= u_orbit_renderer_brightness;\n"
            "    gl_FragColor = orbit_color;\n"
            "}\n";
    } else if (has_main_image) {
        wrapper =
            "\nvoid main() {\n"
            "    vec4 orbit_color = vec4(0.0);\n"
            "    mainImage(orbit_color, gl_FragCoord.xy);\n"
            "    orbit_color.rgb *= u_orbit_renderer_brightness;\n"
            "    gl_FragColor = orbit_color;\n"
            "}\n";
    }

    size_t total = strlen(version_line) + strlen(precision) +
        strlen(i_time_decl) + strlen(i_resolution_decl) +
        strlen(i_time_delta_decl) + strlen(i_frame_decl) +
        strlen(i_frame_rate_decl) + strlen(i_mouse_decl) +
        strlen(brightness_support) + strlen(math_compat) + strlen(palette_support) +
        strlen(rename_main) + strlen(body) +
        strlen(restore_main) + strlen(wrapper) + 1;
    char *prepared = malloc(total);
    if (!prepared) return NULL;

    snprintf(prepared, total, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
             version_line, precision,
             i_time_decl, i_resolution_decl, i_time_delta_decl,
             i_frame_decl, i_frame_rate_decl, i_mouse_decl,
             brightness_support, math_compat, palette_support, rename_main, body,
             restore_main, wrapper);
    return prepared;
}

static GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        GLint log_length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        char *log = calloc((size_t)(log_length > 1 ? log_length : 1), 1);
        if (log) glGetShaderInfoLog(shader, log_length, NULL, log);
        fprintf(stderr, "shader compile failed: %s\n", log ? log : "unknown error");
        free(log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint create_program(const char *fragment_source) {
    static const char *vertex_source =
        "attribute vec2 position;"
        "void main() { gl_Position = vec4(position, 0.0, 1.0); }";
    GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!vertex || !fragment) return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, 0, "position");
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        fprintf(stderr, "shader link failed: %s\n", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

static GLuint create_blit_program(void) {
    static const char *vertex_source =
        "attribute vec2 position;"
        "varying vec2 uv;"
        "void main() {"
        "  gl_Position = vec4(position, 0.0, 1.0);"
        "  uv = position * 0.5 + 0.5;"
        "}";
    static const char *fragment_source =
        "precision mediump float;"
        "varying vec2 uv;"
        "uniform sampler2D source_texture;"
        "void main() {"
        "  gl_FragColor = texture2D(source_texture, uv);"
        "}";

    GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, 0, "position");
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        fprintf(stderr, "blit shader link failed: %s\\n", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}


static void output_geometry(void *data, struct wl_output *output, int32_t x, int32_t y,
                            int32_t physical_width, int32_t physical_height,
                            int32_t subpixel, const char *make, const char *model,
                            int32_t transform) {
    (void)output; (void)physical_width; (void)physical_height;
    (void)subpixel; (void)make; (void)model; (void)transform;
    struct output *item = data;
    item->x = x;
    item->y = y;
}

static void output_mode(void *data, struct wl_output *output, uint32_t flags,
                        int32_t width, int32_t height, int32_t refresh) {
    (void)output; (void)refresh;
    struct output *item = data;
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        item->mode_width = width;
        item->mode_height = height;
    }
}

static void output_done(void *data, struct wl_output *output) {
    (void)data; (void)output;
}

static void output_scale(void *data, struct wl_output *output, int32_t factor) {
    (void)data; (void)output; (void)factor;
}

static void output_name(void *data, struct wl_output *output, const char *name) {
    (void)output;
    struct output *item = data;
    snprintf(item->name, sizeof(item->name), "%s", name);
}

static void output_description(void *data, struct wl_output *output, const char *description) {
    (void)data; (void)output; (void)description;
}

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
    .name = output_name,
    .description = output_description,
};

static void layer_configure(void *data, struct zwlr_layer_surface_v1 *layer,
                            uint32_t serial, uint32_t width, uint32_t height) {
    struct output *item = data;
    item->configure_serial = serial;
    item->width = width ? (int)width : item->mode_width;
    item->height = height ? (int)height : item->mode_height;
    item->configured = true;
    zwlr_layer_surface_v1_ack_configure(layer, serial);
}

static void layer_closed(void *data, struct zwlr_layer_surface_v1 *layer) {
    (void)layer;
    ((struct output *)data)->closed = true;
    running = 0;
}

static const struct zwlr_layer_surface_v1_listener layer_listener = {
    .configure = layer_configure,
    .closed = layer_closed,
};

static void registry_global(void *data, struct wl_registry *registry, uint32_t name,
                            const char *interface, uint32_t version) {
    struct app *app = data;
    if (strcmp(interface, "wl_compositor") == 0) {
        app->compositor = wl_registry_bind(registry, name, &wl_compositor_interface,
                                           version < 4 ? version : 4);
    } else if (strcmp(interface, "zwlr_layer_shell_v1") == 0) {
        app->layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface,
                                             version < 4 ? version : 4);
    } else if (strcmp(interface, "wl_output") == 0 && app->output_count < MAX_OUTPUTS) {
        struct output *item = &app->outputs[app->output_count++];
        item->app = app;
        item->wl_output = wl_registry_bind(registry, name, &wl_output_interface,
                                           version < 4 ? version : 4);
        wl_output_add_listener(item->wl_output, &output_listener, item);
    }
}

static void registry_remove(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data; (void)registry; (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_remove,
};

static bool init_egl(struct app *app) {
    app->egl_display = eglGetDisplay((EGLNativeDisplayType)app->display);
    if (app->egl_display == EGL_NO_DISPLAY || !eglInitialize(app->egl_display, NULL, NULL)) {
        fprintf(stderr, "could not initialize EGL\n");
        return false;
    }
    eglBindAPI(EGL_OPENGL_ES_API);
    const EGLint config_attributes[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE,
    };
    EGLint count = 0;
    if (!eglChooseConfig(app->egl_display, config_attributes, &app->egl_config, 1, &count) || !count) return false;
    const EGLint context_attributes[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    app->egl_context = eglCreateContext(app->egl_display, app->egl_config, EGL_NO_CONTEXT, context_attributes);
    if (app->egl_context == EGL_NO_CONTEXT) return false;
    eglSwapInterval(app->egl_display, 0);
    return true;
}

static bool create_output_surfaces(struct app *app) {
    for (size_t i = 0; i < app->output_count; i++) {
        struct output *item = &app->outputs[i];
        item->surface = wl_compositor_create_surface(app->compositor);
        item->layer = zwlr_layer_shell_v1_get_layer_surface(
            app->layer_shell, item->surface, item->wl_output,
            ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "orbit-wallpaper-engine");
        zwlr_layer_surface_v1_add_listener(item->layer, &layer_listener, item);
        zwlr_layer_surface_v1_set_anchor(item->layer,
            ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
        zwlr_layer_surface_v1_set_exclusive_zone(item->layer, -1);
        zwlr_layer_surface_v1_set_keyboard_interactivity(item->layer,
            ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
        struct wl_region *region = wl_compositor_create_region(app->compositor);
        wl_surface_set_input_region(item->surface, region);
        wl_region_destroy(region);
        wl_surface_commit(item->surface);
    }
    if (wl_display_roundtrip(app->display) < 0) return false;

    app->min_x = app->min_y = 0;
    app->max_x = app->max_y = 1;
    for (size_t i = 0; i < app->output_count; i++) {
        struct output *item = &app->outputs[i];
        if (!item->configured || !item->width || !item->height) return false;
        if (i == 0) {
            app->min_x = item->x; app->min_y = item->y;
            app->max_x = item->x + item->width; app->max_y = item->y + item->height;
        } else {
            if (item->x < app->min_x) app->min_x = item->x;
            if (item->y < app->min_y) app->min_y = item->y;
            if (item->x + item->width > app->max_x) app->max_x = item->x + item->width;
            if (item->y + item->height > app->max_y) app->max_y = item->y + item->height;
        }
    }

    for (size_t i = 0; i < app->output_count; i++) {
        struct output *item = &app->outputs[i];
        item->egl_window = wl_egl_window_create(item->surface, item->width, item->height);
        item->egl_surface = eglCreateWindowSurface(app->egl_display, app->egl_config,
                                                    (EGLNativeWindowType)item->egl_window, NULL);
        if (item->egl_surface == EGL_NO_SURFACE) return false;
    }
    return true;
}

static bool create_scaled_render_targets(struct app *app) {
    if (app->render_scale >= 0.999f) return true;

    static const GLenum texture_unit = GL_TEXTURE0;
    glActiveTexture(texture_unit);

    for (size_t i = 0; i < app->output_count; i++) {
        struct output *item = &app->outputs[i];

        item->render_width = (int32_t)((float)item->width * app->render_scale + 0.5f);
        item->render_height = (int32_t)((float)item->height * app->render_scale + 0.5f);
        if (item->render_width < 1) item->render_width = 1;
        if (item->render_height < 1) item->render_height = 1;

        glGenTextures(1, &item->render_texture);
        glBindTexture(GL_TEXTURE_2D, item->render_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     item->render_width, item->render_height,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        glGenFramebuffers(1, &item->render_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, item->render_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, item->render_texture, 0);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            fprintf(stderr,
                    "could not create scaled framebuffer for %s: 0x%x\\n",
                    item->name, status);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }

        fprintf(stderr,
                "render scale: %s %dx%d -> %dx%d (%.2fx)\\n",
                item->name,
                item->width, item->height,
                item->render_width, item->render_height,
                app->render_scale);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}


static bool write_png(struct output *item, const uint8_t *pixels) {
    char path[PATH_MAX];
    char temporary_path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s.png", item->app->snapshot_dir, item->name);
    snprintf(temporary_path, sizeof(temporary_path), "%s/.%s.png.tmp.XXXXXX",
             item->app->snapshot_dir, item->name);
    int descriptor = mkstemp(temporary_path);
    if (descriptor < 0) return false;
    FILE *file = fdopen(descriptor, "wb");
    if (!file) {
        close(descriptor);
        unlink(temporary_path);
        return false;
    }
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);
    if (!png || !info || setjmp(png_jmpbuf(png))) {
        if (png) png_destroy_write_struct(&png, &info);
        fclose(file);
        unlink(temporary_path);
        return false;
    }
    png_init_io(png, file);
    png_set_IHDR(png, info, item->width, item->height, 8, PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    size_t stride = (size_t)item->width * 4;
    for (int y = item->height - 1; y >= 0; y--) png_write_row(png, pixels + (size_t)y * stride);
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    if (fclose(file) != 0 || rename(temporary_path, path) != 0) {
        unlink(temporary_path);
        return false;
    }
    return true;
}

static bool save_snapshot(struct output *item) {
    size_t size = (size_t)item->width * (size_t)item->height * 4;
    uint8_t *pixels = malloc(size);
    if (!pixels) return false;
    glReadPixels(0, 0, item->width, item->height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    bool result = write_png(item, pixels);
    free(pixels);
    return result;
}

static void set_vec3(GLint location, struct color color) {
    glUniform3f(location, color.r, color.g, color.b);
}

static void render(struct app *app, float seconds, float brightness, float visibility,
                   bool capture_snapshot) {
    static const GLfloat triangle[] = {-1, -1, 3, -1, -1, 3};
    bool snapshots_saved = true;
    for (size_t i = 0; i < app->output_count; i++) {
        struct output *item = &app->outputs[i];
        if (item->closed) continue;
        eglMakeCurrent(app->egl_display, item->egl_surface, item->egl_surface, app->egl_context);

        const bool scaled = app->render_scale < 0.999f && item->render_fbo != 0;
        const int render_width = scaled ? item->render_width : item->width;
        const int render_height = scaled ? item->render_height : item->height;
        const float coordinate_scale = scaled ? app->render_scale : 1.0f;

        glBindFramebuffer(GL_FRAMEBUFFER, scaled ? item->render_fbo : 0);
        glUseProgram(app->program);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, triangle);
        glUniform1f(app->time_uniform, seconds);
        glUniform1f(app->time_alias, seconds);
        glUniform1f(app->u_time_alias, seconds);
        glUniform1f(app->i_time, seconds);
        glUniform1f(app->i_time_delta, app->shader_time_delta);
        glUniform1i(app->i_frame, app->shader_frame);
        glUniform1f(app->i_frame_rate, app->shader_time_delta > 0.000001f
                    ? 1.0f / app->shader_time_delta : 0.0f);
        glUniform4f(app->i_mouse, 0.0f, 0.0f, 0.0f, 0.0f);

        // Legacy shader-controlled fade uniforms remain supported, but are
        // neutralized here. The renderer now owns lifecycle fading so generic
        // fragment shaders fade correctly without knowing about Orbit.
        glUniform1f(app->brightness, 1.0f);
        glUniform1f(app->visibility, 1.0f);

        glUniform2f(app->canvas,
                    (float)(app->max_x - app->min_x) * coordinate_scale,
                    (float)(app->max_y - app->min_y) * coordinate_scale);
        glViewport(0, 0, render_width, render_height);
        glUniform2f(app->resolution, (float)render_width, (float)render_height);
        glUniform2f(app->resolution_alias, (float)render_width, (float)render_height);
        glUniform3f(app->i_resolution, (float)render_width, (float)render_height, 1.0f);
        glUniform2f(app->origin,
                    (float)(item->x - app->min_x) * coordinate_scale,
                    (float)(item->y - app->min_y) * coordinate_scale);
        set_vec3(app->primary, app->colors[0]);
        set_vec3(app->secondary, app->colors[1]);
        set_vec3(app->surface, app->colors[2]);
        set_vec3(app->error, app->colors[3]);
        glUniform1f(app->auto_palette_strength, app->auto_palette_strength_value);
        glUniform1f(app->renderer_brightness, app->peak_brightness);

        float fade = brightness * visibility;
        if (fade < 0.0f) fade = 0.0f;
        if (fade > 1.0f) fade = 1.0f;
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_BLEND);
        glBlendColor(0.0f, 0.0f, 0.0f, fade);
        glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDisable(GL_BLEND);

        if (scaled) {
            // Upscale the reduced-resolution shader result into the native
            // Wayland buffer with a cheap bilinear fullscreen pass.
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, item->width, item->height);
            glDisable(GL_BLEND);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glUseProgram(app->blit_program);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, triangle);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, item->render_texture);
            glUniform1i(app->blit_texture, 0);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        // Snapshots are captured from the final native-resolution buffer, not
        // the reduced offscreen target.
        if (capture_snapshot && !save_snapshot(item)) snapshots_saved = false;
        eglSwapBuffers(app->egl_display, item->egl_surface);
    }
    if (capture_snapshot) {
        app->snapshot_dirty = !snapshots_saved;
    }
    eglMakeCurrent(app->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

int main(int argc, char **argv) {
    char shader_path[PATH_MAX];
    char default_shader_path[PATH_MAX];
    if (!resolve_shader_path(argc, argv, shader_path, sizeof(shader_path),
                             default_shader_path, sizeof(default_shader_path))) {
        return 1;
    }
    const char *home = getenv("HOME");
    if (!home || !*home) {
        fprintf(stderr, "HOME is not set\n");
        return 1;
    }
    signal(SIGINT, stop_handler);
    signal(SIGTERM, stop_handler);
    struct app app = {0};
    app.colors[0] = app.target_colors[0] = environment_color_compat(
        "ORBIT_WALLPAPER_PRIMARY", "PS3_WAVE_PRIMARY", "#7AA2F7");
    app.colors[1] = app.target_colors[1] = environment_color_compat(
        "ORBIT_WALLPAPER_SECONDARY", "PS3_WAVE_SECONDARY", "#BB9AF7");
    app.colors[2] = app.target_colors[2] = environment_color_compat(
        "ORBIT_WALLPAPER_SURFACE", "PS3_WAVE_SURFACE", "#24283B");
    app.colors[3] = app.target_colors[3] = environment_color_compat(
        "ORBIT_WALLPAPER_ERROR", "PS3_WAVE_ERROR", "#F7768E");
    app.follow_system_palette = environment_bool_compat(
        "ORBIT_WALLPAPER_FOLLOW_SYSTEM_PALETTE",
        "PS3_WAVE_FOLLOW_SYSTEM_PALETTE", true);
    app.capture_snapshots = environment_value("ORBIT_WALLPAPER_DISABLE_SNAPSHOTS", "PS3_WAVE_DISABLE_SNAPSHOTS") == NULL;
    app.snapshot_dirty = app.capture_snapshots;
    app.debug_frames = environment_value("ORBIT_WALLPAPER_DEBUG_FRAMES", "PS3_WAVE_DEBUG_FRAMES") != NULL;
    app.intro_duration = environment_float_compat("ORBIT_WALLPAPER_INTRO_DURATION", "PS3_WAVE_INTRO_DURATION", DEFAULT_INTRO_DURATION_SECONDS, 0.1f, 60.0f);
    app.exit_duration = environment_float_compat("ORBIT_WALLPAPER_EXIT_DURATION", "PS3_WAVE_EXIT_DURATION", DEFAULT_EXIT_DURATION_SECONDS, 0.1f, 60.0f);
    app.intro_peak_speed = environment_float_compat("ORBIT_WALLPAPER_INTRO_PEAK_SPEED", "PS3_WAVE_INTRO_PEAK_SPEED", DEFAULT_INTRO_PEAK_SPEED, 0.01f, 1000.0f);
    app.intro_peak_start = environment_float_compat("ORBIT_WALLPAPER_INTRO_PEAK_START", "PS3_WAVE_INTRO_PEAK_START", DEFAULT_INTRO_PEAK_START, 0.0f, 0.99f);
    app.intro_peak_end = environment_float_compat("ORBIT_WALLPAPER_INTRO_PEAK_END", "PS3_WAVE_INTRO_PEAK_END", DEFAULT_INTRO_PEAK_END, 0.01f, 1.0f);
    app.intro_reveal_end = environment_float_compat("ORBIT_WALLPAPER_INTRO_REVEAL_END", "PS3_WAVE_INTRO_REVEAL_END", DEFAULT_INTRO_REVEAL_END, 0.01f, 1.0f);
    app.intro_decay = environment_float_compat("ORBIT_WALLPAPER_INTRO_DECAY", "PS3_WAVE_INTRO_DECAY", DEFAULT_INTRO_DECAY, 0.01f, 100.0f);
    app.auto_palette_strength_value = environment_float_compat("ORBIT_WALLPAPER_PALETTE_STRENGTH", "PS3_WAVE_PALETTE_STRENGTH", DEFAULT_AUTO_PALETTE_STRENGTH, 0.0f, 1.0f);
    app.peak_brightness = environment_float_compat("ORBIT_WALLPAPER_PEAK_BRIGHTNESS", "PS3_WAVE_PEAK_BRIGHTNESS", DEFAULT_PEAK_BRIGHTNESS, 0.0f, 4.0f);
    app.target_fps = environment_float_compat("ORBIT_WALLPAPER_TARGET_FPS", "PS3_WAVE_TARGET_FPS", DEFAULT_TARGET_FPS, 1.0f, 240.0f);
    app.render_scale = environment_float_compat("ORBIT_WALLPAPER_RENDER_SCALE", "PS3_WAVE_RENDER_SCALE", DEFAULT_RENDER_SCALE, 0.25f, 1.0f);
    app.shader_speed = environment_float_compat("ORBIT_WALLPAPER_SPEED", "PS3_WAVE_SPEED", DEFAULT_SHADER_SPEED, 0.0f, 4.0f);
    app.resource_governor = environment_bool_compat("ORBIT_WALLPAPER_RESOURCE_GOVERNOR", "PS3_WAVE_RESOURCE_GOVERNOR", DEFAULT_RESOURCE_GOVERNOR);
    app.gpu_pressure_enter = environment_float_compat("ORBIT_WALLPAPER_GPU_PRESSURE_ENTER", "PS3_WAVE_GPU_PRESSURE_ENTER", DEFAULT_GPU_PRESSURE_ENTER, 1.0f, 100.0f);
    app.gpu_pressure_exit = environment_float_compat("ORBIT_WALLPAPER_GPU_PRESSURE_EXIT", "PS3_WAVE_GPU_PRESSURE_EXIT", DEFAULT_GPU_PRESSURE_EXIT, 0.0f, 100.0f);
    if (app.gpu_pressure_exit > app.gpu_pressure_enter) {
        fprintf(stderr,
                "ORBIT_WALLPAPER_GPU_PRESSURE_EXIT must be <= ORBIT_WALLPAPER_GPU_PRESSURE_ENTER; "
                "using defaults %.0f/%.0f\n",
                DEFAULT_GPU_PRESSURE_ENTER, DEFAULT_GPU_PRESSURE_EXIT);
        app.gpu_pressure_enter = DEFAULT_GPU_PRESSURE_ENTER;
        app.gpu_pressure_exit = DEFAULT_GPU_PRESSURE_EXIT;
    }
    fprintf(stderr,
            "render tuning: target %.1f fps, scale %.2fx, speed %.2fx, resource governor %s, gpu thresholds %.0f%%/%.0f%%\n",
            app.target_fps,
            app.render_scale,
            app.shader_speed,
            app.resource_governor ? "enabled" : "disabled",
            app.gpu_pressure_enter,
            app.gpu_pressure_exit);
    if (app.intro_peak_end <= app.intro_peak_start) {
        fprintf(stderr, "intro peak end must be after peak start; using defaults\n");
        app.intro_peak_start = DEFAULT_INTRO_PEAK_START;
        app.intro_peak_end = DEFAULT_INTRO_PEAK_END;
    }
    const char *palette_path = environment_value(
        "ORBIT_WALLPAPER_PALETTE_FILE", "PS3_WAVE_PALETTE_FILE");
    if (palette_path && *palette_path) {
        snprintf(app.palette_path, sizeof(app.palette_path), "%s", palette_path);
    } else {
        app.palette_path[0] = '\0';
    }
    snprintf(app.snapshot_dir, sizeof(app.snapshot_dir), "%s/%s", home, DEFAULT_SNAPSHOT_RELATIVE);
    const char *background_path = environment_value("ORBIT_WALLPAPER_BACKGROUND_FILE", "PS3_WAVE_BACKGROUND_FILE");
    if (background_path && *background_path) {
        snprintf(app.background_path, sizeof(app.background_path), "%s", background_path);
    } else {
        snprintf(app.background_path, sizeof(app.background_path), "%s/%s", home, DEFAULT_BACKGROUND_RELATIVE);
    }
    const char *control_path = environment_value("ORBIT_WALLPAPER_CONTROL_FILE", "PS3_WAVE_CONTROL_FILE");
    if (control_path && *control_path) {
        snprintf(app.control_path, sizeof(app.control_path), "%s", control_path);
    } else {
        snprintf(app.control_path, sizeof(app.control_path), "%s/%s", home, DEFAULT_CONTROL_RELATIVE);
    }
    char *background_directory = strdup(app.background_path);
    if (background_directory) {
        char *separator = strrchr(background_directory, '/');
        if (separator) {
            *separator = '\0';
            mkdir(background_directory, 0755);
        }
        free(background_directory);
    }
    if (app.capture_snapshots && mkdir(app.snapshot_dir, 0755) != 0 && access(app.snapshot_dir, F_OK) != 0) {
        fprintf(stderr, "cannot create snapshot directory: %s\n", app.snapshot_dir);
        return 1;
    }
    if (!open_control(&app)) {
        fprintf(stderr, "cannot open animation control: %s\n", app.control_path);
        return 1;
    }

    char *fragment_source = read_file(shader_path);
    if (!fragment_source && strcmp(shader_path, default_shader_path) != 0) {
        fprintf(stderr, "cannot read selected shader: %s; falling back to %s\n",
                shader_path, default_shader_path);
        snprintf(shader_path, sizeof(shader_path), "%s", default_shader_path);
        fragment_source = read_file(shader_path);
    }
    if (!fragment_source) {
        fprintf(stderr, "cannot read shader: %s\n", shader_path);
        return 1;
    }
    fprintf(stderr, "using shader: %s\n", shader_path);
    app.display = wl_display_connect(NULL);
    if (!app.display) { fprintf(stderr, "cannot connect to Wayland\n"); free(fragment_source); return 1; }
    struct wl_registry *registry = wl_display_get_registry(app.display);
    wl_registry_add_listener(registry, &registry_listener, &app);
    if (wl_display_roundtrip(app.display) < 0 || !app.compositor || !app.layer_shell || !app.output_count) {
        fprintf(stderr, "required Wayland globals are unavailable\n"); return 1;
    }
    if (!init_egl(&app) || !create_output_surfaces(&app)) {
        fprintf(stderr, "could not initialize renderer surfaces\n"); return 1;
    }
    if (!eglMakeCurrent(app.egl_display, app.outputs[0].egl_surface,
                        app.outputs[0].egl_surface, app.egl_context)) {
        fprintf(stderr, "could not make the EGL context current\n"); return 1;
    }
    char *prepared_source = prepare_fragment_shader(fragment_source);
    free(fragment_source);
    if (!prepared_source) {
        fprintf(stderr, "could not prepare fragment shader\n");
        return 1;
    }
    app.program = create_program(prepared_source);
    free(prepared_source);
    if (!app.program) return 1;

    if (app.render_scale < 0.999f) {
        app.blit_program = create_blit_program();
        if (!app.blit_program) {
            fprintf(stderr, "could not create render-scale blit program\n");
            return 1;
        }
        app.blit_texture = glGetUniformLocation(app.blit_program, "source_texture");
        if (!create_scaled_render_targets(&app)) {
            fprintf(stderr, "could not create scaled render targets\n");
            return 1;
        }
    }

    app.resolution = glGetUniformLocation(app.program, "u_resolution");
    app.resolution_alias = glGetUniformLocation(app.program, "resolution");
    app.i_resolution = glGetUniformLocation(app.program, "iResolution");
    app.origin = glGetUniformLocation(app.program, "u_origin");
    app.canvas = glGetUniformLocation(app.program, "u_canvas");
    app.time_uniform = glGetUniformLocation(app.program, "u_time");
    app.time_alias = glGetUniformLocation(app.program, "time");
    app.u_time_alias = glGetUniformLocation(app.program, "uTime");
    app.i_time = glGetUniformLocation(app.program, "iTime");
    app.i_time_delta = glGetUniformLocation(app.program, "iTimeDelta");
    app.i_frame = glGetUniformLocation(app.program, "iFrame");
    app.i_frame_rate = glGetUniformLocation(app.program, "iFrameRate");
    app.i_mouse = glGetUniformLocation(app.program, "iMouse");
    app.brightness = glGetUniformLocation(app.program, "u_brightness");
    app.visibility = glGetUniformLocation(app.program, "u_visibility");
    app.primary = glGetUniformLocation(app.program, "u_primary");
    app.secondary = glGetUniformLocation(app.program, "u_secondary");
    app.surface = glGetUniformLocation(app.program, "u_surface");
    app.error = glGetUniformLocation(app.program, "u_error");
    app.auto_palette_strength = glGetUniformLocation(app.program, "u_orbit_palette_strength");
    app.renderer_brightness = glGetUniformLocation(app.program, "u_orbit_renderer_brightness");
    read_palette(&app);
    enum animation_mode animation;
    if (environment_value("ORBIT_WALLPAPER_START_HIDDEN", "PS3_WAVE_START_HIDDEN")) {
        animation = ANIMATION_HIDDEN;
    } else {
        animation = environment_value("ORBIT_WALLPAPER_SKIP_INTRO", "PS3_WAVE_SKIP_INTRO")
            ? ANIMATION_NORMAL : ANIMATION_INTRO;
    }
    double animation_started = monotonic_seconds();
    double motion_time = 0.0;
    double last_frame = animation_started;
    double last_palette = monotonic_seconds();
    float last_debug_snapshot = -1.0f;
    double last_pressure_sample = -1.0;
    double gpu_busy = 0.0;
    double cpu_load = 0.0;
    bool under_pressure = false;
    const double target_frame_seconds = 1.0 / (double)app.target_fps;
    while (running) {
        double frame_started = monotonic_seconds();
        wl_display_dispatch_pending(app.display);
        wl_display_flush(app.display);
        double now = monotonic_seconds();
        double frame_delta = now - last_frame;
        if (frame_delta < 0.0 || frame_delta > 0.25) frame_delta = 0.0;
        last_frame = now;

        int animation_request = read_animation_request(&app);
        if (animation_request == 1) {
            // Session transitions must remain visible even if the resource
            // governor froze the normal wallpaper during a game.
            app.frozen = false;
            app.pressure_since = 0.0;
            app.recovery_since = 0.0;
            animation = ANIMATION_INTRO;
            animation_started = now;
            motion_time = 0.0;
            animation_request = 0;
        } else if (animation_request == 2) {
            app.frozen = false;
            app.pressure_since = 0.0;
            app.recovery_since = 0.0;
            animation = ANIMATION_EXIT;
            animation_started = now;
            animation_request = 0;
        } else if (animation_request == 3) {
            app.palette_mtime = 0;
            read_palette(&app);
            last_palette = now;
            animation_request = 0;
        }

        float brightness_value = 1.0f;
        float visibility_value = 1.0f;
        float speed = 1.0f;
        double animation_progress = now - animation_started;
        if (animation == ANIMATION_INTRO) {
            float progress = (float)(animation_progress / app.intro_duration);
            if (progress >= 1.0f) {
                animation = ANIMATION_NORMAL;
                animation_started = now;
            } else {
                if (progress < app.intro_reveal_end) {
                    visibility_value = progress / app.intro_reveal_end;
                }
                // Shape the phase speed like the intro curve: nearly still,
                // sharply fast, briefly sustained, then back to baseline.
                float peak_speed = app.intro_peak_speed;
                if (progress < app.intro_peak_start) {
                    float phase = progress / app.intro_peak_start;
                    float eased = phase * phase * (3.0f - 2.0f * phase);
                    speed = 0.01f + (peak_speed - 0.01f) * eased;
                } else if (progress < app.intro_peak_end) {
                    speed = peak_speed;
                } else {
                    float phase = (progress - app.intro_peak_end) / (1.0f - app.intro_peak_end);
                    float decay = expf(-app.intro_decay * phase);
                    float end_decay = expf(-app.intro_decay);
                    speed = 1.0f + (peak_speed - 1.0f)
                        * (decay - end_decay) / (1.0f - end_decay);
                }
                brightness_value = visibility_value;
            }
        } else if (animation == ANIMATION_EXIT) {
            float progress = (float)(animation_progress / app.exit_duration);
            if (progress >= 1.0f) {
                animation = ANIMATION_HIDDEN;
                visibility_value = 0.0f;
            } else {
                visibility_value = 1.0f - progress;
                float eased = progress * progress * (3.0f - 2.0f * progress);
                speed = 1.0f - 0.85f * eased;
            }
        } else if (animation == ANIMATION_HIDDEN) {
            visibility_value = 0.0f;
        }

        // ORBIT_WALLPAPER_SPEED scales shader time independently of the intro/exit
        // envelope. 1.0 is authored speed, 0.5 is half speed, 2.0 is double.
        const float effective_speed = speed * app.shader_speed;
        app.shader_time_delta = (float)(frame_delta * effective_speed);
        motion_time += frame_delta * effective_speed;
        float elapsed = (float)motion_time;
        app.shader_frame++;
        // External integrations may request a palette refresh through the
        // control FIFO. Keep a slow fallback for direct file edits.
        if (now - last_palette >= 5.0) {
            read_palette(&app);
            last_palette = now;
        }
        for (int i = 0; i < 4; i++) {
            app.colors[i].r += (app.target_colors[i].r - app.colors[i].r) * 0.025f;
            app.colors[i].g += (app.target_colors[i].g - app.colors[i].g) * 0.025f;
            app.colors[i].b += (app.target_colors[i].b - app.colors[i].b) * 0.025f;
        }
        bool capture_snapshot = app.snapshot_dirty;
        if (app.capture_snapshots && app.debug_frames &&
            (last_debug_snapshot < 0.0f || elapsed - last_debug_snapshot >= 0.5f)) {
            capture_snapshot = true;
            last_debug_snapshot = elapsed;
        }

        if (app.resource_governor &&
            (last_pressure_sample < 0.0 || now - last_pressure_sample >= 0.5)) {
            last_pressure_sample = now;
            under_pressure = app.frozen
                ? !resources_recovered(&app, &gpu_busy, &cpu_load)
                : resource_pressure(&app, &gpu_busy, &cpu_load);

            if (!app.frozen) {
                if (under_pressure) {
                    if (app.pressure_since == 0.0) app.pressure_since = now;
                    if (now - app.pressure_since >= PRESSURE_CONFIRM_SECONDS) {
                        // Preserve the exact frame that was visible when the
                        // governor engaged, then stop all animation draws.
                        render(&app, elapsed, brightness_value, visibility_value, app.capture_snapshots);
                        app.frozen = true;
                        app.pressure_since = 0.0;
                        app.recovery_since = 0.0;
                        fprintf(stderr, "resource pressure detected (gpu %.0f%%, load %.2f); wallpaper frozen\n",
                                gpu_busy, cpu_load);
                    }
                } else {
                    app.pressure_since = 0.0;
                }
            } else if (!under_pressure) {
                if (app.recovery_since == 0.0) app.recovery_since = now;
                if (now - app.recovery_since >= RECOVERY_CONFIRM_SECONDS) {
                    app.frozen = false;
                    app.recovery_since = 0.0;
                    fprintf(stderr, "resources recovered (gpu %.0f%%, load %.2f); wallpaper resumed\n",
                            gpu_busy, cpu_load);
                }
            } else {
                app.recovery_since = 0.0;
            }
        } else if (!app.resource_governor && app.frozen) {
            app.frozen = false;
            app.pressure_since = 0.0;
            app.recovery_since = 0.0;
        }

        if (app.frozen) {
            // The committed Wayland buffer remains visible while no EGL work
            // is submitted. Keep dispatching compositor events cheaply.
            sleep_seconds(0.25);
            continue;
        }

        render(&app, elapsed, brightness_value, visibility_value, capture_snapshot);

        // Pace from the start of this iteration instead of sleeping a fixed
        // amount after rendering. Heavy shaders therefore do not pay their
        // render cost *plus* an unconditional 16 ms delay.
        double frame_work_seconds = monotonic_seconds() - frame_started;
        sleep_seconds(target_frame_seconds - frame_work_seconds);
    }
    close(app.control_fd);
    if (app.output_count && app.outputs[0].egl_surface != EGL_NO_SURFACE) {
        eglMakeCurrent(app.egl_display, app.outputs[0].egl_surface,
                       app.outputs[0].egl_surface, app.egl_context);
    }
    for (size_t i = 0; i < app.output_count; i++) {
        if (app.outputs[i].render_fbo) glDeleteFramebuffers(1, &app.outputs[i].render_fbo);
        if (app.outputs[i].render_texture) glDeleteTextures(1, &app.outputs[i].render_texture);
    }
    if (app.blit_program) glDeleteProgram(app.blit_program);
    if (app.program) glDeleteProgram(app.program);
    eglMakeCurrent(app.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    for (size_t i = 0; i < app.output_count; i++) {
        if (app.outputs[i].egl_surface != EGL_NO_SURFACE) eglDestroySurface(app.egl_display, app.outputs[i].egl_surface);
        if (app.outputs[i].egl_window) wl_egl_window_destroy(app.outputs[i].egl_window);
        if (app.outputs[i].layer) zwlr_layer_surface_v1_destroy(app.outputs[i].layer);
        if (app.outputs[i].surface) wl_surface_destroy(app.outputs[i].surface);
    }
    if (app.egl_context != EGL_NO_CONTEXT) eglDestroyContext(app.egl_display, app.egl_context);
    if (app.egl_display != EGL_NO_DISPLAY) eglTerminate(app.egl_display);
    wl_display_disconnect(app.display);
    return 0;
}
