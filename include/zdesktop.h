#ifndef ZDESKTOP_H
#define ZDESKTOP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZD_VERSION_MAJOR 0
#define ZD_VERSION_MINOR 3
#define ZD_VERSION_PATCH 0

#define ZD_FONT_SCRATCH_MIN_SIZE (128U * 1024U)

typedef enum {
    ZD_OK = 0,
    ZD_ERR_ARGUMENT = -1,
    ZD_ERR_FORMAT = -2,
    ZD_ERR_BACKEND = -3,
    ZD_ERR_SCRATCH = -4
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

/*
 * TrueType font backed by caller-owned data and scratch memory.
 *
 * Supported input: TrueType glyf-outline .ttf files and .ttc collections.
 * OpenType CFF (.otf) outlines are intentionally not supported.
 *
 * The font data must remain alive and unchanged while the font is in use.
 * scratch must be at least ZD_FONT_SCRATCH_MIN_SIZE; 256 KiB or more is
 * recommended for complex CJK glyphs. The renderer allocates nothing.
 *
 * stb_truetype does not fully validate hostile font internals. Only load
 * trusted font files. The sfnt directory and loca table are range-checked.
 */
typedef struct {
    const uint8_t *data;
    size_t data_size;
    uint32_t face_index;
    uint32_t face_offset;
    uint32_t pixel_height;
    uint8_t *scratch;
    size_t scratch_size;
    size_t scratch_used;
    uint8_t scratch_failed;
    float scale;
    int32_t ascent;
    int32_t descent;
    int32_t line_gap;
    uint32_t line_height;
} zd_font;

typedef struct {
    uint32_t pixel_height;
    int32_t ascent;
    int32_t descent;
    int32_t line_gap;
    uint32_t line_height;
} zd_font_metrics;

typedef struct {
    int32_t width;
    uint32_t height;
    uint32_t line_count;
} zd_text_metrics;

/* face_index selects a face in a TTC; use zero for a normal TTF file. */
zd_result zd_font_init(zd_font *font,
                       const void *font_data,
                       size_t font_data_size,
                       uint32_t face_index,
                       uint32_t pixel_height,
                       void *scratch,
                       size_t scratch_size);
zd_result zd_font_set_pixel_height(zd_font *font, uint32_t pixel_height);
zd_result zd_font_get_metrics(const zd_font *font,
                              zd_font_metrics *out_metrics);

/*
 * Draw UTF-8 text. baseline_y is the baseline of the first line.
 * max_width == 0 means no horizontal limit. Returns ZD_ERR_SCRATCH if the
 * caller-provided scratch arena cannot rasterize a glyph.
 */
zd_result zd_draw_text(zd_surface *surface, zd_font *font,
                       int32_t x, int32_t baseline_y,
                       const char *utf8, uint32_t argb,
                       uint32_t max_width, int32_t *out_width);
zd_result zd_measure_text(const zd_font *font, const char *utf8,
                          uint32_t max_width,
                          zd_text_metrics *out_metrics);

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
 * Colors are 0xAARRGGBB. A zero-alpha background is transparent. Hover,
 * pressed, and focus colors are optional; zero alpha falls back to the normal
 * background or border. font_size == 0 uses the supplied default font size.
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
    uint32_t hover_background;
    uint32_t pressed_background;
    uint32_t focus_border_color;
    uint16_t font_size;
    uint16_t padding;
    uint16_t gap;
    uint16_t border_width;
    const struct zd_ui_node *const *children;
    uint16_t child_count;
} zd_ui_node;

/* Runtime visual state references immutable nodes; it owns no node memory. */
typedef struct {
    const zd_ui_node *hovered;
    const zd_ui_node *pressed;
    const zd_ui_node *focused;
} zd_ui_state;

/* Renders layout and backgrounds. Text is omitted when font == NULL. */
zd_result zd_ui_render(zd_surface *surface,
                       const zd_ui_node *root,
                       zd_rect viewport);

/* Compatibility form with visual state but no font. */
zd_result zd_ui_render_with_state(zd_surface *surface,
                                  const zd_ui_node *root,
                                  zd_rect viewport,
                                  const zd_ui_state *state);

/* TTF-aware renderer; font scratch is reused and must not be shared concurrently. */
zd_result zd_ui_render_with_font(zd_surface *surface,
                                 const zd_ui_node *root,
                                 zd_rect viewport,
                                 zd_font *font);
zd_result zd_ui_render_with_state_and_font(zd_surface *surface,
                                           const zd_ui_node *root,
                                           zd_rect viewport,
                                           const zd_ui_state *state,
                                           zd_font *font);

/*
 * Returns the topmost BUTTON under the point, or NULL. Coordinates are in
 * surface/viewport pixels. out_bounds may be NULL.
 */
const zd_ui_node *zd_ui_hit_test(const zd_ui_node *root,
                                 zd_rect viewport,
                                 int32_t x, int32_t y,
                                 zd_rect *out_bounds);

#ifdef __cplusplus
}
#endif

#endif
