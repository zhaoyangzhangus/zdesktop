#include "zdesktop.h"

#include <stdio.h>
#include <stdlib.h>

#define SCREEN_WIDTH 960U
#define SCREEN_HEIGHT 540U
#define FONT_SCRATCH_SIZE (512U * 1024U)
#define DEFAULT_FONT_PATH "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc"

static uint8_t font_scratch[FONT_SCRATCH_SIZE];

static const zd_ui_node title = {
    .kind = ZD_UI_TEXT,
    .id = "title",
    .text = "ZDESKTOP 桌面系统",
    .height = 38U,
    .font_size = 30U,
    .foreground = 0xFFF8FAFCU
};

static const zd_ui_node subtitle = {
    .kind = ZD_UI_TEXT,
    .id = "subtitle",
    .text = "TTF 字体渲染 / Declarative UI for zkernel",
    .height = 28U,
    .font_size = 20U,
    .foreground = 0xFFB8C4D6U
};

static const zd_ui_node button = {
    .kind = ZD_UI_BUTTON,
    .id = "launch",
    .text = "开始测试",
    .height = 48U,
    .font_size = 18U,
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
    .height = 250U,
    .background = 0xFF1E293BU,
    .border_color = 0xFF334155U,
    .border_width = 1U,
    .padding = 24U,
    .gap = 16U,
    .children = card_children,
    .child_count = 3U
};

static const zd_ui_node footer = {
    .kind = ZD_UI_TEXT,
    .id = "footer",
    .text = "DEBIAN HOST / TTF SOFTWARE RENDERER",
    .height = 22U,
    .font_size = 14U,
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

static uint8_t *load_file(const char *path, size_t *out_size)
{
    FILE *file;
    long length;
    uint8_t *data;

    file = fopen(path, "rb");
    if (file == NULL) return NULL;
    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)fclose(file);
        return NULL;
    }
    length = ftell(file);
    if (length <= 0L || fseek(file, 0L, SEEK_SET) != 0) {
        (void)fclose(file);
        return NULL;
    }
    data = (uint8_t *)malloc((size_t)length);
    if (data == NULL) {
        (void)fclose(file);
        return NULL;
    }
    if (fread(data, 1U, (size_t)length, file) != (size_t)length) {
        free(data);
        (void)fclose(file);
        return NULL;
    }
    (void)fclose(file);
    *out_size = (size_t)length;
    return data;
}

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
    const char *font_path = (argc > 2) ? argv[2] : DEFAULT_FONT_PATH;
    uint32_t face_index = (argc > 3)
                        ? (uint32_t)strtoul(argv[3], NULL, 10) : 0U;
    const uint32_t pitch = SCREEN_WIDTH * 4U;
    size_t bytes = (size_t)pitch * SCREEN_HEIGHT;
    size_t font_size = 0U;
    uint8_t *memory = (uint8_t *)malloc(bytes);
    uint8_t *font_data;
    zd_font font;
    zd_surface surface;
    zd_rect viewport;
    int result;

    if (memory == NULL) {
        (void)fputs("out of memory\n", stderr);
        return 1;
    }
    font_data = load_file(font_path, &font_size);
    if (font_data == NULL) {
        free(memory);
        (void)fprintf(stderr, "cannot read font: %s\n", font_path);
        return 1;
    }
    if (zd_font_init(&font, font_data, font_size, face_index, 20U,
                     font_scratch, sizeof(font_scratch)) != ZD_OK) {
        free(font_data);
        free(memory);
        (void)fprintf(stderr, "unsupported TTF/TTC font: %s\n", font_path);
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

    if (zd_ui_render_with_font(&surface, &root, viewport, &font) != ZD_OK) {
        free(font_data);
        free(memory);
        (void)fputs("UI render failed\n", stderr);
        return 1;
    }

    result = write_ppm(output, &surface);
    free(font_data);
    free(memory);

    if (!result) {
        (void)fprintf(stderr, "failed to write %s\n", output);
        return 1;
    }

    (void)printf("Rendered %ux%u TTF image to %s\n",
                 SCREEN_WIDTH, SCREEN_HEIGHT, output);
    return 0;
}
