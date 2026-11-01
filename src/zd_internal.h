#ifndef ZDESKTOP_INTERNAL_H
#define ZDESKTOP_INTERNAL_H

#include "zdesktop.h"

/* surface must already be validated; coordinates must be in bounds. */
void zd_blend_coverage(zd_surface *surface, uint32_t x, uint32_t y,
                       uint32_t argb);

#endif
