# Architecture

## Boundary

The repository is split into a platform-independent core and platform adapters.

The core owns:
- surface validation and pixel operations;
- clipping, alpha compositing, primitive drawing, and text rasterization;
- TrueType font table validation, glyph metrics, UTF-8 decoding, and glyph rasterization;
- immutable declarative UI descriptions;
- layout traversal, button hit testing, and control painting;
- optional hover, pressed, and focus visual state.

A platform adapter owns:
- display discovery and mode selection;
- framebuffer mapping or allocation;
- input acquisition and event translation;
- loading font files into caller-owned memory;
- resource allocation;
- presentation and synchronization.

The renderer receives caller-owned surface, font data, and font scratch memory. It does not open devices, allocate memory, call an operating-system API, or assume a particular framebuffer address.

## Core constraints

- C11, with a small public header.
- No libc or libm calls in src/.
- No dynamic allocation in the core.
- No SDL, GTK, Qt, Cairo, X11, Wayland, FreeType, or other host graphics dependency.
- Pixel colors are passed as numeric 0xAARRGGBB values.
- Surface memory layout and pitch are explicit.
- The core is compiled with -ffreestanding -fno-builtin by the Makefile.
- stb_truetype is vendored and its allocator, memory, and math hooks are redirected to internal routines and caller-owned scratch memory.

## TrueType fonts

zd_font references immutable font bytes and a caller-provided scratch arena. The font bytes must outlive the font object. The scratch arena is reused for each glyph and is never allocated internally; 128 KiB is the minimum accepted size and 256–512 KiB is recommended for complex glyphs.

The first implementation supports TrueType glyf outlines in .ttf files and TrueType Collection .ttc faces. It validates the sfnt table directory, required tables, horizontal metrics, cmap format 4/12 records, and monotonic loca offsets before initializing the rasterizer. Glyphs are decoded from UTF-8, looked up in cmap, positioned with horizontal metrics and legacy kern/supported GPOS data, rasterized to grayscale coverage, and alpha-composited into RGBX/BGRX surfaces.

OpenType CFF outlines (.otf) are rejected. Variable-font axis selection, shaping engines (HarfBuzz), complex script shaping, ligatures, bidirectional layout, and fallback-font chains are not part of this initial milestone.

**Font trust boundary:** stb_truetype v1.26 explicitly does not guarantee safe parsing of hostile font data and does not range-check every internal glyph offset. zdesktop checks the outer sfnt directory and loca ranges, but this is not a complete sandbox. Only load trusted font files. A hardened parser or isolated font-loading process is required before accepting arbitrary user-supplied fonts in a privileged kernel environment.

The Debian example loads WenQuanYi Micro Hei from the fonts-wqy-microhei package at /usr/share/fonts/truetype/wqy/wqy-microhei.ttc. No font binary is bundled in the repository. The package provides a compact CJK TrueType Collection.

## Declarative controls

The first milestone uses immutable zd_ui_node descriptors and application-owned child-pointer arrays. This allows a tree to be declared in static C data and rendered without a node allocator.

The initial renderer implements PANEL, ROW, COLUMN, TEXT, and BUTTON. The layout traversal is shared by painting and button hit testing so that pointer targets use the same calculated bounds as the renderer.

A separate zd_ui_state references hovered, pressed, and focused immutable nodes. It does not own nodes or allocate memory. Optional hover and pressed backgrounds and focus borders are specified in the node descriptor. Each text node can select a pixel size through font_size.

The current layout is a small vertical/horizontal flow foundation, not a complete CSS implementation. Parent clipping, scroll containers, flex grow/shrink, grid, and text shaping remain future work.

## Debian backend

The SDL2 backend lives under platform/debian and is not linked into the portable library. It owns the window, an RGBX software surface, a streaming SDL texture, and the event loop.

Keyboard keys are normalized to USB HID Keyboard/Keypad Usage IDs. Text input is delivered separately as UTF-8. Pointer coordinates are window-surface pixels; buttons, wheel, resize, and focus changes use zdesktop-defined event types.

The backend converts RGBX pixels to an opaque RGBA byte stream for SDL presentation. SDL2 is therefore a Debian host dependency only.

## Pixels and alpha

The public color representation is 0xAARRGGBB. Supported output surface byte orders are RGBX and BGRX. The reserved byte is written as zero by the core.

TrueType glyph bitmaps are 8-bit grayscale coverage. Glyph coverage is multiplied by the requested text alpha and composited through the same destination-aware alpha blending path as rectangles.

## Roadmap

- [x] C11 public surface and UI descriptor API
- [x] Freestanding software rasterizer foundation
- [x] Debian headless PPM demo
- [x] Debian SDL2 window backend
- [x] Keyboard and mouse event translation
- [x] Button hover/pressed/focus painting and hit testing
- [x] TrueType glyf outline rendering and UTF-8 decoding
- [ ] Glyph cache and dirty-rectangle tracking
- [ ] Parent clipping and scroll containers
- [ ] Text shaping, fallback fonts, and bidirectional text
- [ ] Runtime node pool and mutable properties
- [ ] Style cascade and binary style compiler
- [ ] UEFI GOP adapter
- [ ] zkernel platform adapter
