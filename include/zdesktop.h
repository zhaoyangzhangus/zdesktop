#ifndef ZDESKTOP_H
#define ZDESKTOP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZD_VERSION_MAJOR 0
#define ZD_VERSION_MINOR 1
#define ZD_VERSION_PATCH 0

typedef enum {
    ZD_OK = 0,
    ZD_ERR_ARGUMENT = -1,
    ZD_ERR_FORMAT = -2
} zd_result;

/*
 * Pixel bytes in memory:
 *   RGBX8888: R, G, B, unused
 *   BGRX8888: B, G, R, unused
 *
 * Public colors are numeric 0xAARRGGBB values, independent of host endian.
 */
typedef enum {
    ZD_PIXEL_RGBX8888 = 1,
    ZD_PIXEL_BGRX8888 = 2
} zd_pixel_format;

typedef struct {
    uint8_t *pixels;
    uint32_t width;
    uint32_t height;
    uint32_t pitch_bytes;
    zd_pixel_format format;
} zd_surface;

typedef struct {
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
} zd_rect;

zd_result zd_surface_validate(const zd_surface *surface);
void zd_surface_clear(zd_surface *surface, uint32_t argb);
void zd_fill_rect(zd_surface *surface, zd_rect rect, uint32_t argb);
void zd_stroke_rect(zd_surface *surface, zd_rect rect,
                    uint32_t thickness, uint32_t argb);

/* Built-in 5x7 ASCII font. Scale 0 is treated as 1. Returns drawn width. */
int32_t zd_draw_text(zd_surface *surface, int32_t x, int32_t y,
                     const char *text, uint32_t argb,
                     uint8_t scale, uint32_t max_width);

typedef enum {
    ZD_UI_PANEL = 1,
    ZD_UI_ROW = 2,
    ZD_UI_COLUMN = 3,
    ZD_UI_TEXT = 4,
    ZD_UI_BUTTON = 5
} zd_ui_kind;

/*
 * Immutable declarative description. Child descriptors and pointer arrays
 * are owned by the application and must remain alive while they are rendered.
 *
 * For ROW, width == 0 distributes remaining main-axis space among zero-width
 * children. For COLUMN/PANEL, height == 0 does the same for zero-height
 * children. A zero cross-axis size stretches to the available inner size.
 * PANEL uses vertical (column) flow.
 *
 * Colors are 0xAARRGGBB. A zero-alpha background is transparent.
 */
typedef struct zd_ui_node {
    zd_ui_kind kind;
    const char *id;
    const char *text;
    uint32_t width;
    uint32_t height;
    uint32_t background;
    uint32_t foreground;
    uint32_t border_color;
    uint16_t padding;
    uint16_t gap;
    uint16_t border_width;
    const struct zd_ui_node *const *children;
    uint16_t child_count;
} zd_ui_node;

/* Renders the immutable tree into viewport. No allocation is performed. */
zd_result zd_ui_render(zd_surface *surface,
                       const zd_ui_node *root,
                       zd_rect viewport);

#ifdef __cplusplus
}
#endif

#endif
