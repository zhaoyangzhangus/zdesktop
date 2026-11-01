CC ?= cc
AR ?= ar
PKG_CONFIG ?= pkg-config

CPPFLAGS ?= -Iinclude
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic
CORE_CFLAGS = $(CFLAGS) -ffreestanding -fno-builtin

SDL2_CFLAGS = $(shell $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
SDL2_LIBS = $(shell $(PKG_CONFIG) --libs sdl2 2>/dev/null)

BUILD_DIR = build
FONT ?= /usr/share/fonts/truetype/wqy/wqy-microhei.ttc
FONT_INDEX ?= 0
DEBIAN_PACKAGES = build-essential pkg-config libsdl2-dev fonts-wqy-microhei
CORE_SOURCES = src/zd_surface.c src/zd_draw.c src/zd_font.c src/zd_ui.c src/zd_window.c
CORE_OBJECTS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(CORE_SOURCES))
LIBRARY = $(BUILD_DIR)/libzdesktop.a
DEMO = $(BUILD_DIR)/zdesktop-demo
DESKTOP = $(BUILD_DIR)/zdesktop-desktop
TEST = $(BUILD_DIR)/test-render

.PHONY: all clean test test-font demo desktop desktop-build freestanding-check debian-deps

all: $(LIBRARY) $(DEMO)

debian-deps:
	sudo apt-get update
	sudo apt-get install -y $(DEBIAN_PACKAGES)

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/zd_font.o: src/zd_font.c include/zdesktop.h src/zd_internal.h \
                         third_party/stb_truetype.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CORE_CFLAGS) -Wno-pedantic -Wno-unused-function -Wno-unused-parameter \
		-c $< -o $@

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
	@command -v $(PKG_CONFIG) >/dev/null 2>&1 && \
		$(PKG_CONFIG) --atleast-version=2.0.5 sdl2 || { \
		echo "Missing SDL2 development dependencies."; \
		echo "On Debian, run: make debian-deps"; \
		exit 1; \
	}
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL2_CFLAGS) \
		platform/debian/zd_debian.c examples/debian_desktop.c \
		$(LIBRARY) $(SDL2_LIBS) -o $@

freestanding-check: $(CORE_OBJECTS)
	@echo "Core objects compiled with -ffreestanding -fno-builtin"

test: $(TEST)
	./$(TEST) "$(FONT)"

test-font: $(TEST)
	@test -r "$(FONT)" || { \
		echo "TTF test font not found: $(FONT)"; \
		echo "Install fonts-wqy-microhei with: make debian-deps"; \
		echo "Or set FONT=/path/to/font.ttf or FONT=/path/to/font.ttc"; \
		exit 1; \
	}
	./$(TEST) "$(FONT)"

demo: $(DEMO)
	./$(DEMO) $(BUILD_DIR)/zdesktop-demo.ppm "$(FONT)" $(FONT_INDEX)
	@echo "Wrote $(BUILD_DIR)/zdesktop-demo.ppm"

desktop-build: $(DESKTOP)

desktop: $(DESKTOP)
	./$(DESKTOP) "$(FONT)" $(FONT_INDEX)

clean:
	rm -rf $(BUILD_DIR)
