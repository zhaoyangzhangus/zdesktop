# Porting to zkernel

The first development host is Debian, but the core must not grow dependencies on Debian facilities.

## Surface contract

A backend constructs a zd_surface using:
- a pointer to writable pixel memory;
- visible width and height;
- pitch in bytes (not width in pixels);
- one of the supported pixel byte orders.

The pitch may be larger than width * 4. Never assume tightly packed scan lines.

## Font contract

The font subsystem is platform-independent. A backend or application loads a TrueType .ttf/.ttc file into memory and passes the immutable byte buffer to zd_font_init. The font bytes must remain mapped and unchanged for the lifetime of the font.

The caller also supplies a scratch arena. The renderer performs no heap allocation; the arena is reused for each glyph. The minimum is ZD_FONT_SCRATCH_MIN_SIZE (128 KiB); allocate 256–512 KiB for complex CJK outlines. zkernel should allocate the arena from its normal kernel memory allocator and keep it alive while rendering.

The initial parser supports TrueType glyf outlines, cmap formats 4/12, and TrueType Collection face selection. It does not support CFF .otf outlines, variable-font axis selection, HarfBuzz shaping, bidi, or fallback-font chains. The vendored stb_truetype parser does not fully range-check hostile font internals; load only trusted fonts until a hardened parser is available.

The rasterizer is configured without libc, libm, malloc, or free. Its memory and math hooks are internal to src/zd_font.c. Do not move font file I/O into the portable core.

## Input contract

The Debian backend demonstrates the platform-independent event model: key transitions, UTF-8 text input, pointer motion/buttons, wheel, focus, and resize notifications. The zkernel adapter should translate its native input sources into equivalent events without importing SDL types into the core.

Keyboard events use USB HID Keyboard/Keypad Usage IDs. This provides a stable key identity independent of the active keyboard layout; text entry remains a separate UTF-8 event.

## UEFI GOP adapter plan

The GOP adapter should:
1. locate EFI_GRAPHICS_OUTPUT_PROTOCOL;
2. choose a usable mode and read its framebuffer base, resolution, pixel format, and PixelsPerScanLine;
3. map the GOP pixel format to RGBX/BGRX when directly compatible;
4. calculate pitch_bytes as PixelsPerScanLine * 4, with overflow checks;
5. construct a zd_surface over the framebuffer only when direct writes are valid;
6. otherwise render to a staging buffer and present with GOP Blt.

PixelRedGreenBlueReserved8BitPerColor and PixelBlueGreenRedReserved8BitPerColor need explicit byte-order mapping. PixelBitMask requires a mask-aware conversion path and is not supported by the initial core. PixelBltOnly has no directly writable linear framebuffer and must use a staging buffer plus GOP Blt.

The adapter, not the renderer, owns GOP protocol calls and firmware-specific structures.

## zkernel adapter plan

The zkernel backend will replace only platform services:
- physical/virtual memory mapping and framebuffer lifetime;
- page-aligned staging-buffer allocation;
- display mode and present operations;
- keyboard and pointer event sources;
- font file/resource loading and persistent font memory;
- font scratch arena allocation;
- timer/scheduler integration.

Do not add zkernel headers to the portable renderer. Keep OS-specific interfaces under a future platform/zkernel directory. The SDL2 header and implementation belong only to platform/debian.

## Debian validation

Install build dependencies and the CJK test font:

~~~sh
sudo apt install build-essential pkg-config libsdl2-dev fonts-wqy-microhei
~~~

Run:
- make freestanding-check
- make test
- make demo
- make desktop-build

The interactive desktop demo is launched with ./build/zdesktop-desktop or make desktop. It validates the same UI tree, layout, TrueType rasterizer, and software renderer used by future non-Debian backends.
