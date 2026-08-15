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
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-egl.h>

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#define MAX_OUTPUTS 16
#define DEFAULT_PALETTE_RELATIVE ".config/hypr/noctalia.lua"
#define DEFAULT_SNAPSHOT_RELATIVE ".cache/ps3-wave-wallpaper"
#define DEFAULT_BACKGROUND_RELATIVE ".cache/ps3-wave-wallpaper/hyprlock-background.conf"
#define DEFAULT_CONTROL_RELATIVE ".cache/ps3-wave-wallpaper/control"
#define GPU_PRESSURE_ENTER 75.0
#define GPU_PRESSURE_EXIT 45.0
#define CPU_PRESSURE_ENTER 0.90
#define CPU_PRESSURE_EXIT 0.65
#define PRESSURE_CONFIRM_SECONDS 3.0
#define RECOVERY_CONFIRM_SECONDS 10.0
#define DEFAULT_INTRO_DURATION_SECONDS 4.5f
#define DEFAULT_EXIT_DURATION_SECONDS 1.0f
#define DEFAULT_INTRO_PEAK_SPEED 34.0f
#define DEFAULT_INTRO_PEAK_START 0.05f
#define DEFAULT_INTRO_PEAK_END 0.08f
#define DEFAULT_INTRO_REVEAL_END 0.22f
#define DEFAULT_INTRO_DECAY 10.0f

struct color { float r, g, b; };

struct output {
    struct app *app;
    struct wl_output *wl_output;
    struct wl_surface *surface;
    struct zwlr_layer_surface_v1 *layer;
    struct wl_egl_window *egl_window;
    EGLSurface egl_surface;
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
    GLint resolution, origin, canvas, time_uniform;
    GLint brightness, visibility;
    GLint primary, secondary, surface, error;
    struct color colors[4];
    struct color target_colors[4];
    time_t palette_mtime;
    bool snapshot_dirty;
    bool greeter_sync_pending;
    double next_greeter_sync;
    bool capture_snapshots;
    bool debug_frames;
    bool frozen;
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

static float environment_float(const char *name, float fallback, float minimum, float maximum) {
    const char *value = getenv(name);
    if (!value || !*value) return fallback;
    char *end = NULL;
    float parsed = strtof(value, &end);
    if (end == value || *end != '\0' || parsed < minimum || parsed > maximum) {
        fprintf(stderr, "invalid %s; using %.3f\n", name, fallback);
        return fallback;
    }
    return parsed;
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

    bool gpu_pressure = gpu_available && *gpu_busy >= GPU_PRESSURE_ENTER;
    bool cpu_pressure = cpu_available && *cpu_load >= CPU_PRESSURE_ENTER * cpu_count;
    return gpu_pressure || cpu_pressure;
}

static bool resources_recovered(struct app *app, double *gpu_busy, double *cpu_load) {
    bool gpu_available = read_gpu_busy(app, gpu_busy);
    bool cpu_available = read_cpu_load(cpu_load);
    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count < 1) cpu_count = 1;

    bool gpu_recovered = !gpu_available || *gpu_busy <= GPU_PRESSURE_EXIT;
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
    return 0;
}

static bool read_palette(struct app *app) {
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
            ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "ps3-wave-wallpaper");
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
        glUseProgram(app->program);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, triangle);
        glUniform1f(app->time_uniform, seconds);
        glUniform1f(app->brightness, brightness);
        glUniform1f(app->visibility, visibility);
        glUniform2f(app->canvas, (float)(app->max_x - app->min_x), (float)(app->max_y - app->min_y));
        glViewport(0, 0, item->width, item->height);
        glUniform2f(app->resolution, (float)item->width, (float)item->height);
        glUniform2f(app->origin, (float)(item->x - app->min_x), (float)(item->y - app->min_y));
        set_vec3(app->primary, app->colors[0]);
        set_vec3(app->secondary, app->colors[1]);
        set_vec3(app->surface, app->colors[2]);
        set_vec3(app->error, app->colors[3]);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        if (capture_snapshot && !save_snapshot(item)) snapshots_saved = false;
        eglSwapBuffers(app->egl_display, item->egl_surface);
    }
    if (capture_snapshot) {
        app->snapshot_dirty = !snapshots_saved;
        if (snapshots_saved) app->greeter_sync_pending = true;
    }
    double now = monotonic_seconds();
    if (app->greeter_sync_pending && now >= app->next_greeter_sync) {
        if (system("noctalia msg greeter-sync >/dev/null 2>&1") == 0) {
            app->greeter_sync_pending = false;
        } else {
            app->next_greeter_sync = now + 1.0;
        }
    }
    eglMakeCurrent(app->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

int main(int argc, char **argv) {
    const char *shader_path = argc > 1 ? argv[1] : "wave.frag";
    const char *home = getenv("HOME");
    if (!home || !*home) {
        fprintf(stderr, "HOME is not set\n");
        return 1;
    }
    signal(SIGINT, stop_handler);
    signal(SIGTERM, stop_handler);
    struct app app = {0};
    app.colors[0] = app.target_colors[0] = (struct color){0.45f, 0.8f, 0.93f};
    app.colors[1] = app.target_colors[1] = (struct color){0.1f, 0.65f, 0.85f};
    app.colors[2] = app.target_colors[2] = (struct color){0.1f, 0.12f, 0.15f};
    app.colors[3] = app.target_colors[3] = (struct color){0.95f, 0.25f, 0.35f};
    app.capture_snapshots = getenv("PS3_WAVE_DISABLE_SNAPSHOTS") == NULL;
    app.snapshot_dirty = app.capture_snapshots;
    app.debug_frames = getenv("PS3_WAVE_DEBUG_FRAMES") != NULL;
    app.intro_duration = environment_float("PS3_WAVE_INTRO_DURATION", DEFAULT_INTRO_DURATION_SECONDS, 0.1f, 60.0f);
    app.exit_duration = environment_float("PS3_WAVE_EXIT_DURATION", DEFAULT_EXIT_DURATION_SECONDS, 0.1f, 60.0f);
    app.intro_peak_speed = environment_float("PS3_WAVE_INTRO_PEAK_SPEED", DEFAULT_INTRO_PEAK_SPEED, 0.01f, 1000.0f);
    app.intro_peak_start = environment_float("PS3_WAVE_INTRO_PEAK_START", DEFAULT_INTRO_PEAK_START, 0.0f, 0.99f);
    app.intro_peak_end = environment_float("PS3_WAVE_INTRO_PEAK_END", DEFAULT_INTRO_PEAK_END, 0.01f, 1.0f);
    app.intro_reveal_end = environment_float("PS3_WAVE_INTRO_REVEAL_END", DEFAULT_INTRO_REVEAL_END, 0.01f, 1.0f);
    app.intro_decay = environment_float("PS3_WAVE_INTRO_DECAY", DEFAULT_INTRO_DECAY, 0.01f, 100.0f);
    if (app.intro_peak_end <= app.intro_peak_start) {
        fprintf(stderr, "intro peak end must be after peak start; using defaults\n");
        app.intro_peak_start = DEFAULT_INTRO_PEAK_START;
        app.intro_peak_end = DEFAULT_INTRO_PEAK_END;
    }
    snprintf(app.palette_path, sizeof(app.palette_path), "%s/%s", home, DEFAULT_PALETTE_RELATIVE);
    snprintf(app.snapshot_dir, sizeof(app.snapshot_dir), "%s/%s", home, DEFAULT_SNAPSHOT_RELATIVE);
    const char *background_path = getenv("PS3_WAVE_BACKGROUND_FILE");
    if (background_path && *background_path) {
        snprintf(app.background_path, sizeof(app.background_path), "%s", background_path);
    } else {
        snprintf(app.background_path, sizeof(app.background_path), "%s/%s", home, DEFAULT_BACKGROUND_RELATIVE);
    }
    const char *control_path = getenv("PS3_WAVE_CONTROL_FILE");
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
    if (!fragment_source) { fprintf(stderr, "cannot read shader: %s\n", shader_path); return 1; }
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
    app.program = create_program(fragment_source);
    free(fragment_source);
    if (!app.program) return 1;
    app.resolution = glGetUniformLocation(app.program, "u_resolution");
    app.origin = glGetUniformLocation(app.program, "u_origin");
    app.canvas = glGetUniformLocation(app.program, "u_canvas");
    app.time_uniform = glGetUniformLocation(app.program, "u_time");
    app.brightness = glGetUniformLocation(app.program, "u_brightness");
    app.visibility = glGetUniformLocation(app.program, "u_visibility");
    app.primary = glGetUniformLocation(app.program, "u_primary");
    app.secondary = glGetUniformLocation(app.program, "u_secondary");
    app.surface = glGetUniformLocation(app.program, "u_surface");
    app.error = glGetUniformLocation(app.program, "u_error");
    read_palette(&app);
    enum animation_mode animation;
    if (getenv("PS3_WAVE_START_HIDDEN")) {
        animation = ANIMATION_HIDDEN;
    } else {
        animation = getenv("PS3_WAVE_SKIP_INTRO")
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
    while (running) {
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

        motion_time += frame_delta * speed;
        float elapsed = (float)motion_time;
        if (now - last_palette >= 0.5) {
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

        if (last_pressure_sample < 0.0 || now - last_pressure_sample >= 0.5) {
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
        }

        if (app.frozen) {
            // The committed Wayland buffer remains visible while no EGL work
            // is submitted. Keep dispatching compositor events cheaply.
            struct timespec pause = {0, 250000000L};
            nanosleep(&pause, NULL);
            continue;
        }

        render(&app, elapsed, brightness_value, visibility_value, capture_snapshot);
        struct timespec pause = {0, 16000000L};
        nanosleep(&pause, NULL);
    }
    close(app.control_fd);
    for (size_t i = 0; i < app.output_count; i++) {
        if (app.outputs[i].egl_surface != EGL_NO_SURFACE) eglDestroySurface(app.egl_display, app.outputs[i].egl_surface);
        if (app.outputs[i].egl_window) wl_egl_window_destroy(app.outputs[i].egl_window);
        if (app.outputs[i].layer) zwlr_layer_surface_v1_destroy(app.outputs[i].layer);
        if (app.outputs[i].surface) wl_surface_destroy(app.outputs[i].surface);
    }
    if (app.program) glDeleteProgram(app.program);
    if (app.egl_context != EGL_NO_CONTEXT) eglDestroyContext(app.egl_display, app.egl_context);
    if (app.egl_display != EGL_NO_DISPLAY) eglTerminate(app.egl_display);
    wl_display_disconnect(app.display);
    return 0;
}
