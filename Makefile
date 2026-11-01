CC ?= cc
AR ?= ar

CPPFLAGS ?= -Iinclude
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic
CORE_CFLAGS = $(CFLAGS) -ffreestanding -fno-builtin

BUILD_DIR = build
CORE_SOURCES = src/zd_surface.c src/zd_draw.c src/zd_font.c src/zd_ui.c
CORE_OBJECTS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(CORE_SOURCES))
LIBRARY = $(BUILD_DIR)/libzdesktop.a
DEMO = $(BUILD_DIR)/zdesktop-demo
TEST = $(BUILD_DIR)/test-render

.PHONY: all clean test demo freestanding-check

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

freestanding-check: $(CORE_OBJECTS)
	@echo "Core objects compiled with -ffreestanding -fno-builtin"

test: $(TEST)
	./$(TEST)

demo: $(DEMO)
	./$(DEMO) $(BUILD_DIR)/zdesktop-demo.ppm
	@echo "Wrote $(BUILD_DIR)/zdesktop-demo.ppm"

clean:
	rm -rf $(BUILD_DIR)
