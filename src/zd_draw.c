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


void zd_blend_coverage(zd_surface *surface, uint32_t x, uint32_t y,
                       uint32_t argb)
{
    uint32_t alpha = (argb >> 24) & 0xFFU;
    uint8_t red = (uint8_t)((argb >> 16) & 0xFFU);
    uint8_t green = (uint8_t)((argb >> 8) & 0xFFU);
    uint8_t blue = (uint8_t)(argb & 0xFFU);

    if (alpha == 0U) return;
    if (alpha == 255U) {
        zd_write_pixel(surface, x, y, red, green, blue);
    } else {
        zd_blend_pixel(surface, x, y, red, green, blue, (uint8_t)alpha);
    }
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
    int64_t middle_y;
    zd_rect edge;
    uint32_t middle_height;

    if (zd_surface_validate(surface) != ZD_OK ||
        rect.width == 0U || rect.height == 0U || thickness == 0U) {
        return;
    }

    if (thickness > rect.width) thickness = rect.width;
    if (thickness > rect.height) thickness = rect.height;

    /*
     * If the border is at least half as thick as either dimension, the
     * border covers the complete rectangle. Draw once to avoid blending
     * overlapping translucent edges multiple times.
     */
    if ((uint64_t)thickness * 2U >= rect.width ||
        (uint64_t)thickness * 2U >= rect.height) {
        zd_fill_rect(surface, rect, argb);
        return;
    }

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

    middle_y = (int64_t)rect.y + thickness;
    middle_height = rect.height - thickness * 2U;
    if (middle_y >= INT32_MIN && middle_y <= INT32_MAX) {
        edge.x = rect.x;
        edge.y = (int32_t)middle_y;
        edge.width = thickness;
        edge.height = middle_height;
        zd_fill_rect(surface, edge, argb);

        right = (int64_t)rect.x + (int64_t)rect.width - thickness;
        if (right >= INT32_MIN && right <= INT32_MAX) {
            edge.x = (int32_t)right;
            zd_fill_rect(surface, edge, argb);
        }
    }
}

