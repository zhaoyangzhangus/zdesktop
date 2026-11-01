# Architecture

## Boundary

The repository is split into a platform-independent core and platform adapters.

The core owns:
- surface validation and pixel operations;
- clipping, alpha compositing, primitive drawing, and text rasterization;
- immutable declarative UI descriptions;
- layout traversal, button hit testing, and control painting;
- optional hover, pressed, and focus visual state.

A platform adapter owns:
- display discovery and mode selection;
- framebuffer mapping or allocation;
- input acquisition and event translation;
- resource allocation;
- presentation and synchronization.

The renderer receives a caller-owned surface. It does not open devices, allocate memory, call an operating-system API, or assume a particular framebuffer address.

## Core constraints

- C11, with a small public header.
- No libc calls in src/.
- No dynamic allocation in the core.
- No SDL, GTK, Qt, Cairo, X11, Wayland, or other host graphics dependency.
- Pixel colors are passed as numeric 0xAARRGGBB values.
- Surface memory layout and pitch are explicit.
- The core is compiled with -ffreestanding -fno-builtin by the Makefile.

Standard integer and size type headers are used for declarations. This does not imply a runtime dependency on libc.

## Declarative controls

The first milestone uses immutable zd_ui_node descriptors and application-owned child-pointer arrays. This allows a tree to be declared in static C data and rendered without a node allocator.

The initial renderer implements PANEL, ROW, COLUMN, TEXT, and BUTTON. The layout traversal is shared by painting and button hit testing so that pointer targets use the same calculated bounds as the renderer.

A separate zd_ui_state references hovered, pressed, and focused immutable nodes. It does not own nodes or allocate memory. Optional hover and pressed backgrounds and focus borders are specified in the node descriptor.

The intended evolution is:
1. keep the descriptor format independent from runtime state;
2. add a runtime node pool for mutable controls;
3. place variable properties in compact property storage;
4. compile shared style sheets into a versioned binary format;
5. make the same tree semantics available to a binary resource loader.

The current layout is a small vertical/horizontal flow foundation, not a complete CSS implementation. Parent clipping, scroll containers, flex grow/shrink, grid, and text shaping remain future work.

## Debian backend

The SDL2 backend lives under platform/debian and is not linked into the portable library. It owns the window, an RGBX software surface, a streaming SDL texture, and the event loop.

Keyboard keys are normalized to USB HID Keyboard/Keypad Usage IDs. Text input is delivered separately as UTF-8. Pointer coordinates are window-surface pixels; buttons, wheel, resize, and focus changes use zdesktop-defined event types.

The backend converts RGBX pixels to an opaque RGBA byte stream for SDL presentation. SDL2 is therefore a Debian host dependency only.

## Pixels and alpha

The public color representation is 0xAARRGGBB. Supported output surface byte orders are RGBX and BGRX. The reserved byte is written as zero by the core.

Alpha compositing is performed against the existing opaque destination pixel. The first font is a built-in 5x7 ASCII bitmap font; non-ASCII glyphs are not yet implemented.

## Roadmap

- [x] C11 public surface and UI descriptor API
- [x] Freestanding software rasterizer foundation
- [x] Debian headless PPM demo
- [x] Initial renderer tests
- [x] Debian SDL2 window backend
- [x] Keyboard and mouse event translation
- [x] Button hover/pressed/focus painting and hit testing
- [ ] Parent clipping and dirty-rectangle tracking
- [ ] Text input controls, focus manager, and accessibility semantics
- [ ] Runtime node pool and mutable properties
- [ ] Style cascade and binary style compiler
- [ ] UEFI GOP adapter
- [ ] zkernel platform adapter
