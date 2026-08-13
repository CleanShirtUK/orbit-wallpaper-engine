CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic
PKGS = wayland-client egl glesv2 libpng
CFLAGS += $(shell pkg-config --cflags $(PKGS))
LDLIBS += $(shell pkg-config --libs $(PKGS)) -lwayland-egl -lm

XDG_SHELL_XML = /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml

all: ps3-wave-wallpaper

wlr-layer-shell-unstable-v1-client-protocol.h: protocol/wlr-layer-shell-unstable-v1.xml
	wayland-scanner client-header $< $@

wlr-layer-shell-unstable-v1-protocol.c: protocol/wlr-layer-shell-unstable-v1.xml
	wayland-scanner private-code $< $@

xdg-shell-client-protocol.h: $(XDG_SHELL_XML)
	wayland-scanner client-header $< $@

xdg-shell-protocol.c: $(XDG_SHELL_XML)
	wayland-scanner private-code $< $@

ps3-wave-wallpaper: renderer.c wlr-layer-shell-unstable-v1-client-protocol.h wlr-layer-shell-unstable-v1-protocol.c xdg-shell-client-protocol.h xdg-shell-protocol.c
	$(CC) $(CFLAGS) -o $@ renderer.c wlr-layer-shell-unstable-v1-protocol.c xdg-shell-protocol.c $(LDLIBS)

clean:
	rm -f ps3-wave-wallpaper wlr-layer-shell-unstable-v1-client-protocol.h wlr-layer-shell-unstable-v1-protocol.c xdg-shell-client-protocol.h xdg-shell-protocol.c

.PHONY: all clean
