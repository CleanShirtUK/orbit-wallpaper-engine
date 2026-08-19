CC ?= cc

APP := ps3-wave-wallpaper
TARGET := $(APP)

SOURCES := \
	renderer.c \
	wlr-layer-shell-unstable-v1-protocol.c \
	xdg-shell-protocol.c

CFLAGS ?= -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -I/usr/include/libpng16 -DWITH_GZFILEOP
LDLIBS ?= -lwayland-client -lm -lEGL -lGLESv2 -lpng16 -lwayland-egl

HOME_DIR := $(HOME)
XDG_CONFIG_HOME ?= $(HOME_DIR)/.config
XDG_DATA_HOME ?= $(HOME_DIR)/.local/share
PREFIX ?= $(HOME_DIR)/.local

BINDIR ?= $(PREFIX)/bin
APP_CONFIG_DIR ?= $(XDG_CONFIG_HOME)/$(APP)
APP_SHADER_DIR ?= $(APP_CONFIG_DIR)/shaders
APP_DATA_DIR ?= $(XDG_DATA_HOME)/$(APP)
SYSTEMD_USER_DIR ?= $(XDG_CONFIG_HOME)/systemd/user

INSTALLED_BINARY := $(BINDIR)/$(APP)
INSTALLED_DEFAULT_SHADER := $(APP_DATA_DIR)/wave.frag
INSTALLED_CONFIG := $(APP_CONFIG_DIR)/config
INSTALLED_SERVICE := $(SYSTEMD_USER_DIR)/$(APP).service

DEFAULT_CONFIG_SRC := ps3-wave-wallpaper.conf
SERVICE_SRC := ps3-wave-wallpaper.service

.PHONY: all clean install install-config install-service daemon-reload enable restart reload intro deploy uninstall status paths

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	@printf '\n==> Installing $(APP)\n'
	install -d "$(BINDIR)"
	install -d "$(APP_DATA_DIR)"
	install -d "$(APP_CONFIG_DIR)"
	install -d "$(APP_SHADER_DIR)"
	install -d "$(SYSTEMD_USER_DIR)"
	install -m 0755 "$(TARGET)" "$(INSTALLED_BINARY)"
	install -m 0644 wave.frag "$(INSTALLED_DEFAULT_SHADER)"
	$(MAKE) --no-print-directory install-config
	$(MAKE) --no-print-directory install-service
	@printf '\nInstalled:\n'
	@printf '  binary:         %s\n' "$(INSTALLED_BINARY)"
	@printf '  default shader: %s\n' "$(INSTALLED_DEFAULT_SHADER)"
	@printf '  config:         %s\n' "$(INSTALLED_CONFIG)"
	@printf '  user shaders:   %s\n' "$(APP_SHADER_DIR)"
	@printf '  service:        %s\n' "$(INSTALLED_SERVICE)"
	@printf '\nRun `make deploy` to deploy and replay the intro animation.\n'

install-config:
	@if [ -e "$(INSTALLED_CONFIG)" ]; then \
		printf 'Keeping existing config: %s\n' "$(INSTALLED_CONFIG)"; \
	elif [ -f "$(DEFAULT_CONFIG_SRC)" ]; then \
		install -m 0644 "$(DEFAULT_CONFIG_SRC)" "$(INSTALLED_CONFIG)"; \
		printf 'Installed initial config: %s\n' "$(INSTALLED_CONFIG)"; \
	else \
		printf '%s\n' \
			'# ps3-wave-wallpaper user configuration' \
			'' \
			'PS3_WAVE_INTRO_DURATION=4.5' \
			'PS3_WAVE_EXIT_DURATION=1.0' \
			'PS3_WAVE_INTRO_PEAK_SPEED=34.0' \
			'PS3_WAVE_INTRO_PEAK_START=0.05' \
			'PS3_WAVE_INTRO_PEAK_END=0.08' \
			'PS3_WAVE_INTRO_REVEAL_END=0.22' \
			'PS3_WAVE_INTRO_DECAY=10.0' \
			'PS3_WAVE_PALETTE_STRENGTH=0.72' \
			'' \
			'# Filename from ~/.config/ps3-wave-wallpaper/shaders/' \
			'# PS3_WAVE_SHADER=example.frag' \
			> "$(INSTALLED_CONFIG)"; \
		printf 'Created initial config: %s\n' "$(INSTALLED_CONFIG)"; \
	fi

install-service:
	@if [ ! -f "$(SERVICE_SRC)" ]; then \
		printf 'ERROR: missing %s\n' "$(SERVICE_SRC)" >&2; \
		exit 1; \
	fi
	install -m 0644 "$(SERVICE_SRC)" "$(INSTALLED_SERVICE)"
	@printf 'Installed service: %s\n' "$(INSTALLED_SERVICE)"

daemon-reload:
	systemctl --user daemon-reload

enable:
	systemctl --user enable "$(APP).service"

restart:
	systemctl --user restart "$(APP).service"

# Replays the renderer's intro without restarting it.
reload intro:
	systemctl --user reload "$(APP).service"

# Deploy a new build, restart into the normal hidden state, then explicitly
# replay the intro. Login behaviour remains unchanged because only this target
# sends the post-restart reload.
deploy: install daemon-reload restart
	@printf '\n==> Triggering intro animation\n'
	@for i in 1 2 3 4 5 6 7 8 9 10; do \
		if systemctl --user --quiet is-active "$(APP).service"; then \
			break; \
		fi; \
		sleep 0.1; \
	done
	systemctl --user reload "$(APP).service"
	@printf '\n==> Deployment complete\n'
	@systemctl --user --no-pager --full status "$(APP).service" || true

status:
	@systemctl --user --no-pager --full status "$(APP).service" || true
	@printf '\nRecent log:\n'
	@journalctl --user -u "$(APP).service" -n 15 --no-pager || true

paths:
	@printf 'Source binary:    %s\n' "$(CURDIR)/$(TARGET)"
	@printf 'Installed binary: %s\n' "$(INSTALLED_BINARY)"
	@printf 'Default shader:   %s\n' "$(INSTALLED_DEFAULT_SHADER)"
	@printf 'Config:           %s\n' "$(INSTALLED_CONFIG)"
	@printf 'User shaders:     %s\n' "$(APP_SHADER_DIR)"
	@printf 'Systemd service:  %s\n' "$(INSTALLED_SERVICE)"

uninstall:
	@printf '\n==> Uninstalling $(APP)\n'
	-systemctl --user disable --now "$(APP).service"
	rm -f "$(INSTALLED_BINARY)"
	rm -f "$(INSTALLED_DEFAULT_SHADER)"
	rm -f "$(INSTALLED_SERVICE)"
	systemctl --user daemon-reload
	@printf '\nPreserved user data:\n'
	@printf '  %s\n' "$(INSTALLED_CONFIG)"
	@printf '  %s/\n' "$(APP_SHADER_DIR)"
	@printf '\nRemove ~/.config/ps3-wave-wallpaper manually if you want to purge user data.\n'
