#include "zdesktop.h"

#include <stdio.h>
#include <stdlib.h>

#define SCREEN_WIDTH 960U
#define SCREEN_HEIGHT 540U

static const zd_ui_node title = {
    .kind = ZD_UI_TEXT,
    .id = "title",
    .text = "ZDESKTOP",
    .height = 30U,
    .foreground = 0xFFF8FAFCU
};

static const zd_ui_node subtitle = {
    .kind = ZD_UI_TEXT,
    .id = "subtitle",
    .text = "DECLARATIVE UI FOR ZKERNEL",
    .height = 22U,
    .foreground = 0FFB8C4D6U
};

static const zd_ui_node button = {
    .kind = ZD_UI_BUTTON,
    .id = "launch",
    .text = "START",
    .height = 48U,
    .background = 0xFF2563EBU,
    .foreground = 0xFFFFFFFFU,
    .padding = 16U
};

static const zd_ui_node *const card_children[] = {
    &title,
    &subtitle,
    &button
};

static const zd_ui_node card = {
    .kind = ZD_UI_PANEL,
    .id = "welcome-card",
    .height = 230U,
    .background = 0xFF1E293BU,
    .border_color = 0xFF334155U,
    .border_width = 1U,
    .padding = 24U,
    .gap = 14U,
    .children = card_children,
    .child_count = 3U
};

static const zd_ui_node footer = {
    .kind = ZD_UI_TEXT,
    .id = "footer",
    .text = "DEBIAN HOST / SOFTWARE RENDERER",
    .height = 16U,
    .foreground = 0xFF94A3B8U
};

static const zd_ui_node *const root_children[] = {
    &card,
    &footer
};

static const zd_ui_node root = {
    .kind = ZD_UI_COLUMN,
    .id = "desktop-root",
    .background = 0xFF0F172AU,
    .padding = 48U,
    .gap = 24U,
    .children = root_children,
    .child_count = 2U
};

static int write_ppm(const char *path, const zd_surface *surface)
{
    FILE *file;
    uint32_t y;

    file = fopen(path, "wb");
    if (file == NULL) {
        (void)fprintf(stderr, "cannot open output: %s\n", path);
        return 0;
    }

    if (fprintf(file, "P6\n%u %u\n255\n",
                surface->width, surface->height) < 0) {
        (void)fclose(file);
        return 0;
    }

    for (y = 0U; y < surface->height; ++y) {
        uint32_t x;
        for (x = 0U; x < surface->width; ++x) {
            const uint8_t *pixel = surface->pixels +
                (size_t)y * surface->pitch_bytes + (size_t)x * 4U;
            uint8_t rgb[3];

            if (surface->format == ZD_PIXEL_BGRX8888) {
                rgb[0] = pixel[2];
                rgb[1] = pixel[1];
                rgb[2] = pixel[0];
            } else {
                rgb[0] = pixel[0];
                rgb[1] = pixel[1];
                rgb[2] = pixel[2];
            }

            if (fwrite(rgb, 1U, sizeof(rgb), file) != sizeof(rgb)) {
                (void)fclose(file);
                return 0;
            }
        }
    }

    return (fclose(file) == 0);
}

int main(int argc, char **argv)
{
    const char *output = (argc > 1) ? argv[1] : "zdesktop-demo.ppm";
    const uint32_t pitch = SCREEN_WIDTH * 4U;
    size_t bytes = (size_t)pitch * SCREEN_HEIGHT;
    uint8_t *memory = (uint8_t *)malloc(bytes);
    zd_surface surface;
    zd_rect viewport;
    int result;

    if (memory == NULL) {
        (void)fputs("out of memory\n", stderr);
        return 1;
    }

    surface.pixels = memory;
    surface.width = SCREEN_WIDTH;
    surface.height = SCREEN_HEIGHT;
    surface.pitch_bytes = pitch;
    surface.format = ZD_PIXEL_BGRX8888;

    zd_surface_clear(&surface, 0xFF0F172AU);
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = SCREEN_WIDTH;
    viewport.height = SCREEN_HEIGHT;

    if (zd_ui_render(&surface, &root, viewport) != ZD_OK) {
        free(memory);
        (void)fputs("UI render failed\n", stderr);
        return 1;
    }

    result = write_ppm(output, &surface);
    free(memory);

    if (!result) {
        (void)fprintf(stderr, "failed to write %s\n", output);
        return 1;
    }

    (void)printf("Rendered %ux%u image to %s\n",
                 SCREEN_WIDTH, SCREEN_HEIGHT, output);
    return 0;
}
