# zdesktop

A declarative C desktop UI framework for **zkernel**.

The project is developed and tested on Debian first, then will be ported to zkernel. Debian is a development and validation host, not a runtime dependency of the UI core.

## Design goals

- Pure C11 UI, layout, and software-rendering core.
- No libc, SDL, GTK, Qt, Cairo, or other host graphics dependency in the renderer.
- Caller-provided memory; no allocator is required by the core.
- Declarative, statically describable control trees, with a path toward binary UI resources and runtime node pools.
- A small platform boundary: Debian/SDL2 now, UEFI GOP and zkernel backends later.
- The same UI description, layout, hit testing, and rasterization code on every backend.

## Current status

The first interactive Debian milestone provides:

- RGBX/BGRX software surfaces with explicit pitch.
- Clipped rectangle fills, alpha blending, borders, and a built-in 5x7 ASCII font.
- Static declarative panel/row/column/text/button descriptors.
- Button hover, pressed, and keyboard-focus painting.
- Button hit testing that shares the renderer's layout calculation.
- A Debian SDL2 window backend with keyboard, text-input, mouse, wheel, resize, and focus events.
- A Debian interactive demo with mouse activation, Tab focus navigation, Enter/Space activation, and Escape to close.
- A headless PPM demo and core unit tests.

This is an initial window and control milestone, not yet a complete desktop/window manager or full CSS implementation.

## Build on Debian testing + GNOME

Install the SDL2 development dependency:

```sh
sudo apt update
sudo apt install build-essential pkg-config libsdl2-dev
```

Build the portable core, tests, and PPM demo:

```sh
make
make test
make demo
```

Build and run the interactive Debian window:

```sh
make desktop-build
./build/zdesktop-desktop
```

Or build and launch it in one step:

```sh
make desktop
```

The SDL2 backend uses the active GNOME display session. It can run on Wayland through SDL's Wayland backend or through XWayland, depending on the installed SDL2 drivers and session environment.

## Repository layout

```text
include/       Public portable and Debian backend APIs
src/           Platform-independent UI and renderer core
platform/      Platform-specific backends
  debian/      SDL2 display and input backend
examples/      Headless and interactive Debian examples
tests/         Host-side core tests
docs/          Architecture and porting notes
```

## Porting direction

The core consumes a caller-owned `zd_surface`; it does not discover displays or allocate memory. The Debian backend owns SDL2 window creation, framebuffer allocation, event translation, and presentation.

A future UEFI GOP adapter will translate GOP mode information into a zdesktop surface (including the real scan-line pitch and pixel format). A future zkernel adapter will provide memory, display, input, and scheduling services through explicit platform interfaces.

See [docs/architecture.md](docs/architecture.md), [docs/debian-backend.md](docs/debian-backend.md), and [docs/porting-zkernel.md](docs/porting-zkernel.md).
