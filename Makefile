CC ?= cc
AR ?= ar
PKG_CONFIG ?= pkg-config

CPPFLAGS ?= -Iinclude
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic
CORE_CFLAGS = $(CFLAGS) -ffreestanding -fno-builtin

SDL2_CFLAGS = $(shell $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
SDL2_LIBS = $(shell $(PKG_CONFIG) --libs sdl2 2>/dev/null)

BUILD_DIR = build
CORE_SOURCES = src/zd_surface.c src/zd_draw.c src/zd_font.c src/zd_ui.c
CORE_OBJECTS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(CORE_SOURCES))
LIBRARY = $(BUILD_DIR)/libzdesktop.a
DEMO = $(BUILD_DIR)/zdesktop-demo
DESKTOP = $(BUILD_DIR)/zdesktop-desktop
TEST = $(BUILD_DIR)/test-render

.PHONY: all clean test demo desktop desktop-build freestanding-check

all: $(LIBRARY) $(DEMO)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: src/%.c include/zdesktop.h src/zd_internal.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CORE_CFLAGS) -c $< -o $@

$(LIBRARY): $(CORE_OBJECTS)
	$(AR) rcs $@ $^

$(DEMO): examples/debian_demo.c $(LIBRARY) include/zdesktop.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIBRARY) -o $@

$(TEST): tests/test_render.c $(LIBRARY) include/zdesktop.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(LIBRARY) -o $@

$(DESKTOP): examples/debian_desktop.c platform/debian/zd_debian.c \
            include/zdesktop_debian.h $(LIBRARY) | $(BUILD_DIR)
	@$(PKG_CONFIG) --atleast-version=2.0.5 sdl2 || { \
		echo "SDL2 development files are required. Install libsdl2-dev and pkg-config."; \
		exit 1; \
	}
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL2_CFLAGS) \
		platform/debian/zd_debian.c examples/debian_desktop.c \
		$(LIBRARY) $(SDL2_LIBS) -o $@

freestanding-check: $(CORE_OBJECTS)
	@echo "Core objects compiled with -ffreestanding -fno-builtin"

test: $(TEST)
	./$(TEST)

demo: $(DEMO)
	./$(DEMO) $(BUILD_DIR)/zdesktop-demo.ppm
	@echo "Wrote $(BUILD_DIR)/zdesktop-demo.ppm"

desktop-build: $(DESKTOP)

desktop: $(DESKTOP)
	./$(DESKTOP)

clean:
	rm -rf $(BUILD_DIR)
