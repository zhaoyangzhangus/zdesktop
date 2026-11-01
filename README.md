# zdesktop

A declarative C desktop UI framework for **zkernel**.

The project is developed and tested on Debian first, then will be ported to zkernel. Debian is a development and validation host, not a runtime dependency of the UI core.

## Design goals

- Pure C11 UI, layout, TrueType text rasterization, and software-rendering core.
- No libc, libm, SDL, GTK, Qt, Cairo, or other host graphics dependency in the core.
- Caller-owned memory and scratch arenas; the core performs no dynamic allocation.
- Declarative, statically describable control trees, with a path toward binary UI resources and runtime node pools.
- A small platform boundary: Debian/SDL2 now, UEFI GOP and zkernel backends later.
- The same UI description, layout, hit testing, font rasterization, and pixel renderer on every backend.

## Current status

The interactive Debian milestone provides:

- RGBX/BGRX software surfaces with explicit pitch.
- Clipped rectangle fills, alpha blending, and borders.
- TrueType glyf-outline .ttf and .ttc loading from caller-owned memory.
- UTF-8 decoding, glyph metrics, kerning, and antialiased grayscale glyph rasterization.
- Caller-provided font scratch memory; no malloc or libm in the renderer.
- Per-control font sizes, plus CJK-capable text when using a suitable font.
- Static declarative panel/row/column/text/button descriptors.
- Button hover, pressed, and keyboard-focus painting and hit testing.
- A Debian SDL2 window backend with keyboard, text-input, mouse, wheel, resize, and focus events.

The font parser intentionally supports TrueType glyf outlines. OpenType CFF .otf outlines and variable-font axis selection are not supported by this initial backend. Only load trusted font files; see docs/architecture.md for the security boundary.

## Build on Debian testing + GNOME

Install the dependencies and a CJK TrueType Collection font:

~~~sh
make debian-deps

# Equivalent manual installation:
sudo apt update
sudo apt install build-essential pkg-config libsdl2-dev fonts-wqy-microhei
~~~

The default font is:

~~~text
/usr/share/fonts/truetype/wqy/wqy-microhei.ttc
~~~

Build the portable core, tests, and PPM demo:

~~~sh
make
make test       # core tests; TTF integration tests skip if FONT is unavailable
make test-font  # require and run TTF/TTC integration tests
make demo
~~~

Build and run the interactive Debian window:

~~~sh
make desktop-build
./build/zdesktop-desktop
~~~

Or build and launch it in one step:

~~~sh
make desktop
~~~

Use a different font or TTC face index:

~~~sh
./build/zdesktop-desktop /path/to/font.ttf
./build/zdesktop-desktop /path/to/font.ttc 1
make desktop FONT=/path/to/font.ttf FONT_INDEX=0
~~~

The SDL2 backend uses the active GNOME display session. It can run on Wayland through SDL's Wayland backend or through XWayland, depending on the installed SDL2 drivers and session environment.

## Font API

The application owns the TTF bytes and scratch memory:

~~~c
static uint8_t font_scratch[512 * 1024];
zd_font font;

zd_font_init(&font, ttf_bytes, ttf_size, 0, 18,
             font_scratch, sizeof(font_scratch));

zd_ui_render_with_font(&surface, &root, viewport, &font);
~~~

A zd_ui_node.font_size of zero uses the font's configured pixel height. Text is UTF-8; glyph bitmaps are grayscale and composited through the same ARGB surface path as other primitives.

## Repository layout

~~~text
include/       Public portable and Debian backend APIs
src/           Platform-independent UI, TTF, and renderer core
third_party/   Vendored stb_truetype rasterizer
platform/      Platform-specific backends
  debian/      SDL2 display and input backend
examples/      Headless and interactive Debian examples
tests/         Host-side core and TTF tests
docs/          Architecture and porting notes
~~~

See docs/architecture.md, docs/debian-backend.md, and docs/porting-zkernel.md.
