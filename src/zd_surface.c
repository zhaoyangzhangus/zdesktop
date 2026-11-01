#include "zdesktop.h"

zd_result zd_surface_validate(const zd_surface *surface)
{
    uint64_t minimum_pitch;

    if (surface == NULL || surface->pixels == NULL ||
        surface->width == 0U || surface->height == 0U) {
        return ZD_ERR_ARGUMENT;
    }

    if (surface->format != ZD_PIXEL_RGBX8888 &&
        surface->format != ZD_PIXEL_BGRX8888) {
        return ZD_ERR_FORMAT;
    }

    minimum_pitch = (uint64_t)surface->width * 4U;
    if ((uint64_t)surface->pitch_bytes < minimum_pitch ||
        (uint64_t)surface->pitch_bytes * surface->height >
            (uint64_t)((size_t)-1)) {
        return ZD_ERR_ARGUMENT;
    }

    return ZD_OK;
}

void zd_surface_clear(zd_surface *surface, uint32_t argb)
{
    zd_rect full;

    if (zd_surface_validate(surface) != ZD_OK) {
        return;
    }

    full.x = 0;
    full.y = 0;
    full.width = surface->width;
    full.height = surface->height;
    zd_fill_rect(surface, full, argb);
}
