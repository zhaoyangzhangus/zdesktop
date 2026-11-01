# zdesktop

A declarative C desktop UI framework for **zkernel**.

The project is developed and tested on Debian first, then will be ported to zkernel. Debian is a development and validation host, not a runtime dependency of the UI core.

## Design goals

- Pure C11 UI, layout, and software-rendering core.
- No libc, SDL, GTK, Qt, Cairo, or other host graphics dependency in the renderer.
- Caller-provided memory; no allocator is required by the core.
- Declarative, statically describable control trees, with a path toward binary UI resources and runtime node pools.
- A small platform boundary: Debian test backend now, UEFI GOP and zkernel backends later.
- The same UI description, layout, and rasterization code on every backend.

## Current status

The first bootstrap milestone provides:

- RGBX/BGRX software surfaces with explicit pitch.
- Clipped rectangle fills, alpha blending, and a built-in 5x7 ASCII font.
- Static declarative panel/row/column/text/button descriptors.
- A Debian headless demo that writes a PPM image.
- Unit tests for clipping, pixel formats, alpha blending, and UI rendering.

This is an early foundation, not yet a complete desktop or GOP implementation.

## Build on Debian

Requirements: GCC or Clang, GNU Make, and a C11-capable toolchain.

```sh
make
make test
make demo
```

The demo writes `build/zdesktop-demo.ppm`. Open it with any image viewer that supports PPM, or convert it with a tool such as ImageMagick if installed.

## Repository layout

```text
include/       Public C API
src/           Platform-independent UI and renderer core
examples/      Debian-hosted examples
tests/         Host-side tests
docs/          Architecture and porting notes
```

## Porting direction

The core consumes a caller-owned `zd_surface`; it does not discover displays or allocate memory. A future UEFI GOP adapter will translate GOP mode information into a zdesktop surface (including the real scan-line pitch and pixel format). A future zkernel adapter will provide memory, display, input, and scheduling services through explicit platform interfaces.

See [docs/architecture.md](docs/architecture.md) and [docs/porting-zkernel.md](docs/porting-zkernel.md).
