#include "zd_internal.h"

#include <limits.h>

static void zd_write_pixel(zd_surface *surface, uint32_t x, uint32_t y,
                           uint8_t red, uint8_t green, uint8_t blue)
{
    size_t offset = (size_t)y * (size_t)surface->pitch_bytes +
                    (size_t)x * 4U;
    uint8_t *pixel = surface->pixels + offset;

    if (surface->format == ZD_PIXEL_RGBX8888) {
        pixel[0] = red;
        pixel[1] = green;
        pixel[2] = blue;
    } else {
        pixel[0] = blue;
        pixel[1] = green;
        pixel[2] = red;
    }
    pixel[3] = 0U;
}

static void zd_blend_pixel(zd_surface *surface, uint32_t x, uint32_t y,
                           uint8_t red, uint8_t green, uint8_t blue,
                           uint8_t alpha)
{
    size_t offset = (size_t)y * (size_t)surface->pitch_bytes +
                    (size_t)x * 4U;
    const uint8_t *pixel = surface->pixels + offset;
    uint32_t dst_red;
    uint32_t dst_green;
    uint32_t dst_blue;
    uint32_t inverse = 255U - (uint32_t)alpha;
    uint32_t out_red;
    uint32_t out_green;
    uint32_t out_blue;

    if (surface->format == ZD_PIXEL_RGBX8888) {
        dst_red = pixel[0];
        dst_green = pixel[1];
        dst_blue = pixel[2];
    } else {
        dst_blue = pixel[0];
        dst_green = pixel[1];
        dst_red = pixel[2];
    }

    out_red = ((uint32_t)red * alpha + dst_red * inverse + 127U) / 255U;
    out_green = ((uint32_t)green * alpha + dst_green * inverse + 127U) / 255U;
    out_blue = ((uint32_t)blue * alpha + dst_blue * inverse + 127U) / 255U;

    zd_write_pixel(surface, x, y, (uint8_t)out_red,
                   (uint8_t)out_green, (uint8_t)out_blue);
}

void zd_fill_rect(zd_surface *surface, zd_rect rect, uint32_t argb)
{
    int64_t left;
    int64_t top;
    int64_t right;
    int64_t bottom;
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    uint32_t alpha;
    uint32_t y;

    if (zd_surface_validate(surface) != ZD_OK ||
        rect.width == 0U || rect.height == 0U) {
        return;
    }

    alpha = (argb >> 24) & 0xFFU;
    if (alpha == 0U) {
        return;
    }

    left = rect.x;
    top = rect.y;
    right = left + (int64_t)rect.width;
    bottom = top + (int64_t)rect.height;

    if (left < 0) left = 0;
    if (top < 0) top = 0;
    if (right > (int64_t)surface->width) right = surface->width;
    if (bottom > (int64_t)surface->height) bottom = surface->height;

    if (left >= right || top >= bottom) {
        return;
    }

    red = (argb >> 16) & 0xFFU;
    green = (argb >> 8) & 0xFFU;
    blue = argb & 0xFFU;

    for (y = (uint32_t)top; y < (uint32_t)bottom; ++y) {
        uint32_t x;
        for (x = (uint32_t)left; x < (uint32_t)right; ++x) {
            if (alpha == 255U) {
                zd_write_pixel(surface, x, y, (uint8_t)red,
                               (uint8_t)green, (uint8_t)blue);
            } else {
                zd_blend_pixel(surface, x, y, (uint8_t)red,
                               (uint8_t)green, (uint8_t)blue,
                               (uint8_t)alpha);
            }
        }
    }
}

void zd_stroke_rect(zd_surface *surface, zd_rect rect,
                    uint32_t thickness, uint32_t argb)
{
    int64_t right;
    int64_t bottom;
    zd_rect edge;

    if (zd_surface_validate(surface) != ZD_OK ||
        rect.width == 0U || rect.height == 0U || thickness == 0U) {
        return;
    }

    if (thickness > rect.width) thickness = rect.width;
    if (thickness > rect.height) thickness = rect.height;

    edge.x = rect.x;
    edge.y = rect.y;
    edge.width = rect.width;
    edge.height = thickness;
    zd_fill_rect(surface, edge, argb);

    bottom = (int64_t)rect.y + (int64_t)rect.height - thickness;
    if (bottom >= INT32_MIN && bottom <= INT32_MAX) {
        edge.y = (int32_t)bottom;
        zd_fill_rect(surface, edge, argb);
    }

    edge.x = rect.x;
    edge.y = rect.y;
    edge.width = thickness;
    edge.height = rect.height;
    zd_fill_rect(surface, edge, argb);

    right = (int64_t)rect.x + (int64_t)rect.width - thickness;
    if (right >= INT32_MIN && right <= INT32_MAX) {
        edge.x = (int32_t)right;
        zd_fill_rect(surface, edge, argb);
    }
}

int32_t zd_draw_text(zd_surface *surface, int32_t x, int32_t y,
                     const char *text, uint32_t argb,
                     uint8_t scale, uint32_t max_width)
{
    int64_t cursor_x = x;
    int64_t start_x = x;
    uint32_t advance;
    unsigned char character;

    if (zd_surface_validate(surface) != ZD_OK || text == NULL) {
        return 0;
    }

    if (scale == 0U) scale = 1U;
    advance = 6U * (uint32_t)scale;

    while (*text != '\0') {
        uint8_t row;
        character = (unsigned char)*text++;

        if (character == '\n') {
            break;
        }

        if (max_width != 0U &&
            cursor_x - start_x + 5U * (uint32_t)scale > max_width) {
            break;
        }

        for (row = 0U; row < 7U; ++row) {
            uint8_t bits = zd_font_row(character, row);
            uint8_t column;

            for (column = 0U; column < 5U; ++column) {
                uint8_t mask = (uint8_t)(1U << (4U - column));
                int64_t pixel_x = cursor_x +
                                  (int64_t)column * (int64_t)scale;
                int64_t pixel_y = (int64_t)y +
                                  (int64_t)row * (int64_t)scale;

                if ((bits & mask) != 0U &&
                    pixel_x >= INT32_MIN && pixel_x <= INT32_MAX &&
                    pixel_y >= INT32_MIN && pixel_y <= INT32_MAX) {
                    zd_rect glyph_pixel;
                    glyph_pixel.x = (int32_t)pixel_x;
                    glyph_pixel.y = (int32_t)pixel_y;
                    glyph_pixel.width = scale;
                    glyph_pixel.height = scale;
                    zd_fill_rect(surface, glyph_pixel, argb);
                }
            }
        }

        cursor_x += advance;
        if (cursor_x > INT32_MAX) {
            break;
        }
    }

    if (cursor_x - start_x > INT32_MAX) {
        return INT32_MAX;
    }
    return (int32_t)(cursor_x - start_x);
}
