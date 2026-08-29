CC ?= gcc
PKG_CONFIG ?= pkg-config

TARGET := camera_flip
SRC := src/main.c src/gui.c src/input.c src/camera.c src/config.c src/performance.c
OBJ := $(SRC:.c=.o)

CFLAGS ?= -O3 -march=native -Wall -Wextra
CPPFLAGS += $(shell $(PKG_CONFIG) --cflags gtk+-3.0 x11 xtst)
LDLIBS += $(shell $(PKG_CONFIG) --libs gtk+-3.0 x11 xtst)

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share
USER_PREFIX ?= $(HOME)/.local
USER_BINDIR ?= $(USER_PREFIX)/bin
USER_APPDIR ?= $(USER_PREFIX)/share/applications
USER_DESKTOP ?= $(HOME)/Desktop

.PHONY: all clean install install-user

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)
	install -Dm644 config/roblox-camera-flip.desktop $(DESTDIR)$(DATADIR)/applications/roblox-camera-flip.desktop

install-user: $(TARGET)
	install -Dm755 $(TARGET) $(USER_BINDIR)/$(TARGET)
	mkdir -p $(USER_APPDIR)
	sed "s|^Exec=.*|Exec=$(USER_BINDIR)/$(TARGET)|" config/roblox-camera-flip.desktop > $(USER_APPDIR)/roblox-camera-flip.desktop
	chmod 644 $(USER_APPDIR)/roblox-camera-flip.desktop
	if [ -d "$(USER_DESKTOP)" ]; then \
		sed "s|^Exec=.*|Exec=$(USER_BINDIR)/$(TARGET)|" config/roblox-camera-flip.desktop > $(USER_DESKTOP)/roblox-camera-flip.desktop; \
		chmod 755 $(USER_DESKTOP)/roblox-camera-flip.desktop; \
	fi

clean:
	rm -f $(TARGET) $(OBJ)
