# Debian SDL2 backend

## Dependency

On Debian testing with GNOME:

\`\`\`sh
sudo apt install build-essential pkg-config libsdl2-dev
\`\`\`

The portable library and headless demo do not require SDL2. Only the interactive backend target links SDL2.

## Build and launch

\`\`\`sh
make desktop-build
./build/zdesktop-desktop
\`\`\`

Or use \`make desktop\` to build and launch.

SDL2 selects the available video driver. In a GNOME Wayland session it normally uses the Wayland driver when available, otherwise XWayland may be used. No direct X11 or Wayland calls are made by zdesktop.

## Ownership

The backend owns:
- SDL window, renderer, and streaming texture;
- RGBX framebuffer allocation and resize;
- event polling and conversion into zd_debian_event;
- conversion of the RGBX unused byte into opaque alpha for SDL's RGBA32 texture;
- presentation and cleanup.

The render callback receives a temporary zd_surface valid only for the duration of that callback. Do not retain its pixel pointer.

## Input mapping

- Key down/up events use USB HID Keyboard/Keypad Usage IDs, not SDL keycodes.
- Text input is a separate UTF-8 event, so keyboard layout and compose behavior are handled by the desktop environment.
- Pointer positions are surface coordinates.
- Mouse buttons, wheel, resize, and window focus are normalized to zdesktop event types.
- Returning non-zero from the event callback requests window shutdown.

## Initial demo

The demo contains two buttons:
- CLICK ME: increments the click counter.
- EXIT: closes the window.

Move the pointer to see hover painting, press a button to see pressed painting, and click to activate. Tab switches keyboard focus; Enter or Space activates the focused button; Escape closes the window. Text input and wheel activity are shown in the status line.
