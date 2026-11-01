# Third-party code

## stb_truetype

- Upstream: https://github.com/nothings/stb
- File: stb_truetype.h
- Version: v1.26
- Pinned upstream commit: 6e9f34d5429cf16790ec43c9bac3f1ee4ad1f760
- Upstream blob SHA-1: 90a5c2e2b3fe563c98585294ca7a949876aec94c
- License: dual-licensed MIT / public domain (Unlicense); the complete upstream license text is retained at the end of the vendored header.

zdesktop overrides stb_truetype's allocator, memory, and math hooks. Glyph rendering uses the caller-provided scratch arena and does not call malloc/free or libm. The current zdesktop wrapper accepts only TrueType glyf outlines; CFF outline support is intentionally rejected.
