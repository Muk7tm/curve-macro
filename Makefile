CC ?= gcc
PKG_CONFIG ?= pkg-config

TARGET := curve_macro
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
	install -Dm644 config/curve-macro.desktop $(DESTDIR)$(DATADIR)/applications/curve-macro.desktop

install-user: $(TARGET)
	install -Dm755 $(TARGET) $(USER_BINDIR)/$(TARGET)
	mkdir -p $(USER_APPDIR)
	rm -f $(USER_BINDIR)/camera_flip $(USER_APPDIR)/roblox-camera-flip.desktop
	sed "s|^Exec=.*|Exec=$(USER_BINDIR)/$(TARGET)|" config/curve-macro.desktop > $(USER_APPDIR)/curve-macro.desktop
	chmod 644 $(USER_APPDIR)/curve-macro.desktop
	if [ -d "$(USER_DESKTOP)" ]; then \
		rm -f $(USER_DESKTOP)/roblox-camera-flip.desktop; \
		sed "s|^Exec=.*|Exec=$(USER_BINDIR)/$(TARGET)|" config/curve-macro.desktop > $(USER_DESKTOP)/curve-macro.desktop; \
		chmod 755 $(USER_DESKTOP)/curve-macro.desktop; \
	fi

clean:
	rm -f $(TARGET) camera_flip $(OBJ)
