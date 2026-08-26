#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <wayland-client.h>

struct capabilities {
    bool compositor;
    bool layer_shell;
    unsigned int outputs;
};

static void global(void *data, struct wl_registry *registry, uint32_t name,
                   const char *interface, uint32_t version) {
    (void)registry;
    (void)name;
    (void)version;
    struct capabilities *capabilities = data;
    if (strcmp(interface, "wl_compositor") == 0) {
        capabilities->compositor = true;
    } else if (strcmp(interface, "zwlr_layer_shell_v1") == 0) {
        capabilities->layer_shell = true;
    } else if (strcmp(interface, "wl_output") == 0) {
        capabilities->outputs++;
    }
}

static void global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = global,
    .global_remove = global_remove,
};

int main(void) {
    struct wl_display *display = wl_display_connect(NULL);
    if (!display) {
        puts("{\"state\":\"CANNOT_VERIFY\",\"reason\":\"could not connect to the Wayland display\"}");
        return 0;
    }

    struct capabilities capabilities = {0};
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, &capabilities);
    if (wl_display_roundtrip(display) < 0) {
        puts("{\"state\":\"CANNOT_VERIFY\",\"reason\":\"Wayland registry inspection failed\"}");
        wl_display_disconnect(display);
        return 0;
    }

    const bool compatible = capabilities.compositor && capabilities.layer_shell
        && capabilities.outputs > 0;
    printf("{\"state\":\"%s\",\"required\":{\"wl_compositor\":%s,"
           "\"zwlr_layer_shell_v1\":%s,\"wl_output\":%s},\"output_count\":%u",
           compatible ? "COMPATIBLE" : "UNSUPPORTED",
           capabilities.compositor ? "true" : "false",
           capabilities.layer_shell ? "true" : "false",
           capabilities.outputs > 0 ? "true" : "false",
           capabilities.outputs);
    if (!compatible) {
        fputs(",\"missing\":[", stdout);
        bool first = true;
        if (!capabilities.compositor) {
            fputs("\"wl_compositor\"", stdout);
            first = false;
        }
        if (!capabilities.layer_shell) {
            if (!first) fputc(',', stdout);
            fputs("\"zwlr_layer_shell_v1\"", stdout);
            first = false;
        }
        if (capabilities.outputs == 0) {
            if (!first) fputc(',', stdout);
            fputs("\"wl_output\"", stdout);
        }
        fputc(']', stdout);
    }
    puts("}");
    wl_display_disconnect(display);
    return 0;
}
