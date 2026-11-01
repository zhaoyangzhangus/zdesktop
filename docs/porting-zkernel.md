# Porting to zkernel

The first development host is Debian, but the core must not grow dependencies on Debian facilities.

## Surface contract

A backend constructs a zd_surface using:
- a pointer to writable pixel memory;
- visible width and height;
- pitch in bytes (not width in pixels);
- one of the supported pixel byte orders.

The pitch may be larger than width * 4. Never assume tightly packed scan lines.

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
- timer/scheduler integration.

Do not add zkernel headers to the portable renderer. Keep OS-specific interfaces under a future platform/zkernel directory. The SDL2 header and implementation belong only to platform/debian.

## Debian validation

Run:
- make freestanding-check
- make test
- make demo
- make desktop-build (requires pkg-config and libsdl2-dev)

The interactive desktop demo is launched with ./build/zdesktop-desktop or make desktop. It validates the same UI tree, layout, and software renderer used by future non-Debian backends.
