# Debian SDL2 backend and TTF fonts

## Dependencies

On Debian testing with GNOME:

~~~sh
sudo apt install build-essential pkg-config libsdl2-dev fonts-wqy-microhei
~~~

The default demo font is /usr/share/fonts/truetype/wqy/wqy-microhei.ttc, a two-face TrueType Collection. Face index zero is used by default. The portable core does not depend on fontconfig or FreeType; the example opens the file and passes its bytes to zdesktop.

## Build and launch

~~~sh
make desktop-build
./build/zdesktop-desktop
~~~

Or use make desktop to build and launch. Select another TTF/TTC file and optional face index with:

~~~sh
./build/zdesktop-desktop /path/to/font.ttf
./build/zdesktop-desktop /path/to/font.ttc 1
make desktop FONT=/path/to/font.ttf FONT_INDEX=0
~~~

SDL2 selects the available video driver. In a GNOME Wayland session it normally uses the Wayland driver when available, otherwise XWayland may be used. No direct X11 or Wayland calls are made by zdesktop.

## Font ownership

The application owns the font file bytes and the font scratch buffer. The font bytes must remain valid until the UI stops rendering. The scratch arena is reused glyph by glyph; 128 KiB is the accepted minimum, and 512 KiB is used by the example to accommodate complex CJK glyph outlines.

The core supports TrueType glyf outlines in .ttf files and .ttc collections. It rejects CFF-outline .otf files. Only trusted fonts should be loaded because stb_truetype does not range-check every internal glyph offset.

## Input mapping

- Key down/up events use USB HID Keyboard/Keypad Usage IDs, not SDL keycodes.
- Text input is a separate UTF-8 event, so keyboard layout and compose behavior are handled by the desktop environment.
- Pointer positions are surface coordinates.
- Mouse buttons, wheel, resize, and window focus are normalized to zdesktop event types.
- Returning non-zero from the event callback requests window shutdown.

## Initial demo

The demo contains two buttons:
- 点击测试: increments the click counter.
- 退出: closes the window.

Move the pointer to see hover painting, press a button to see pressed painting, and click to activate. Tab switches keyboard focus; Enter or Space activates the focused button; Escape closes the window. Text input and wheel activity are shown in the status line.
