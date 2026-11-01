#include "zdesktop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ZD_TEST_FONT_PATH
#define ZD_TEST_FONT_PATH "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc"
#endif

static unsigned int failures;
static uint8_t font_scratch[512U * 1024U];

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "FAIL %s:%d: %s\n", \
                      __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static void test_surface_format(void)
{
    uint8_t pixels[4] = {0U, 0U, 0U, 0U};
    zd_surface surface = {
        pixels, 1U, 1U, 4U, ZD_PIXEL_RGBX8888
    };

    zd_surface_clear(&surface, 0xFF123456U);
    CHECK(pixels[0] == 0x12U);
    CHECK(pixels[1] == 0x34U);
    CHECK(pixels[2] == 0x56U);
    CHECK(pixels[3] == 0U);

    surface.format = ZD_PIXEL_BGRX8888;
    zd_surface_clear(&surface, 0xFF123456U);
    CHECK(pixels[0] == 0x56U);
    CHECK(pixels[1] == 0x34U);
    CHECK(pixels[2] == 0x12U);
}

static void test_pitch_validation(void)
{
    uint8_t pixels[16] = {0U};
    zd_surface surface = {
        pixels, 2U, 2U, 4U, ZD_PIXEL_RGBX8888
    };

    CHECK(zd_surface_validate(&surface) == ZD_ERR_ARGUMENT);
    surface.pitch_bytes = 8U;
    CHECK(zd_surface_validate(&surface) == ZD_OK);
}

static void test_clipping(void)
{
    uint8_t pixels[4U * 4U * 4U] = {0U};
    zd_surface surface = {
        pixels, 4U, 4U, 4U * 4U, ZD_PIXEL_RGBX8888
    };
    zd_rect rect = {-1, -1, 2U, 2U};

    zd_surface_clear(&surface, 0xFF000000U);
    zd_fill_rect(&surface, rect, 0xFFFF0000U);

    CHECK(pixels[0] == 0xFFU);
    CHECK(pixels[1] == 0U);
    CHECK(pixels[2] == 0U);
    CHECK(pixels[4] == 0U);
    CHECK(pixels[(1U * 4U + 1U) * 4U] == 0U);
}

static void test_alpha_blending(void)
{
    uint8_t pixels[4] = {0U, 0U, 0U, 0U};
    zd_surface surface = {
        pixels, 1U, 1U, 4U, ZD_PIXEL_RGBX8888
    };
    zd_rect pixel = {0, 0, 1U, 1U};

    zd_surface_clear(&surface, 0xFF000000U);
    zd_fill_rect(&surface, pixel, 0x80FF0000U);
    CHECK(pixels[0] == 128U);
    CHECK(pixels[1] == 0U);
    CHECK(pixels[2] == 0U);

    zd_fill_rect(&surface, pixel, 0x800000FFU);
    CHECK(pixels[0] >= 63U && pixels[0] <= 65U);
    CHECK(pixels[1] == 0U);
    CHECK(pixels[2] >= 127U && pixels[2] <= 129U);
}

static void test_row_gap_distribution(void)
{
    uint8_t pixels[20U * 10U * 4U] = {0U};
    zd_surface surface = {
        pixels, 20U, 10U, 20U * 4U, ZD_PIXEL_RGBX8888
    };
    static const zd_ui_node first = {
        .kind = ZD_UI_BUTTON,
        .height = 10U,
        .background = 0xFFFF0000U
    };
    static const zd_ui_node second = {
        .kind = ZD_UI_BUTTON,
        .height = 10U,
        .background = 0xFF00FF00U
    };
    static const zd_ui_node *const children[] = {
        &first, &second
    };
    static const zd_ui_node root = {
        .kind = ZD_UI_ROW,
        .gap = 4U,
        .children = children,
        .child_count = 2U
    };
    zd_rect viewport = {0, 0, 20U, 10U};
    size_t second_pixel = (size_t)(5U * 20U + 17U) * 4U;

    CHECK(zd_ui_render(&surface, &root, viewport) == ZD_OK);
    CHECK(pixels[second_pixel] == 0U);
    CHECK(pixels[second_pixel + 1U] == 0xFFU);
    CHECK(pixels[second_pixel + 2U] == 0U);
}

static void test_hit_test_and_hover_paint(void)
{
    uint8_t pixels[40U * 20U * 4U] = {0U};
    zd_surface surface = {
        pixels, 40U, 20U, 40U * 4U, ZD_PIXEL_RGBX8888
    };
    static const zd_ui_node first = {
        .kind = ZD_UI_BUTTON,
        .id = "first",
        .width = 16U,
        .background = 0xFFFF0000U,
        .focus_border_color = 0xFFFFFFFFU
    };
    static const zd_ui_node second = {
        .kind = ZD_UI_BUTTON,
        .id = "second",
        .width = 16U,
        .background = 0xFF0000FFU,
        .hover_background = 0xFF00AA00U
    };
    static const zd_ui_node *const children[] = {
        &first, &second
    };
    static const zd_ui_node root = {
        .kind = ZD_UI_ROW,
        .gap = 4U,
        .children = children,
        .child_count = 2U
    };
    zd_rect viewport = {0, 0, 40U, 20U};
    zd_rect hit_bounds;
    zd_ui_state state = {NULL, NULL, &first};
    const zd_ui_node *hit;

    hit = zd_ui_hit_test(&root, viewport, 8, 10, &hit_bounds);
    CHECK(hit == &first);
    CHECK(hit_bounds.x == 0);
    CHECK(hit_bounds.width == 16U);

    hit = zd_ui_hit_test(&root, viewport, 24, 10, &hit_bounds);
    CHECK(hit == &second);
    CHECK(hit_bounds.x == 20);
    CHECK(hit_bounds.width == 16U);

    CHECK(zd_ui_hit_test(&root, viewport, 18, 10, NULL) == NULL);
    CHECK(zd_ui_hit_test(&root, viewport, 39, 19, NULL) == NULL);

    state.hovered = &second;
    CHECK(zd_ui_render_with_state(&surface, &root, viewport, &state) == ZD_OK);
    {
        size_t pixel = (size_t)(10U * 40U + 24U) * 4U;
        CHECK(pixels[pixel] == 0U);
        CHECK(pixels[pixel + 1U] == 0xAAU);
        CHECK(pixels[pixel + 2U] == 0U);
    }
}

static uint8_t *load_test_font(size_t *out_size)
{
    FILE *file = fopen(ZD_TEST_FONT_PATH, "rb");
    long length;
    uint8_t *data;

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

static void test_ttf_rendering(void)
{
    size_t font_size = 0U;
    uint8_t *font_data = load_test_font(&font_size);
    uint8_t pixels[256U * 80U * 4U];
    uint8_t invalid_data[16] = {0U};
    zd_font font;
    zd_font_metrics font_metrics;
    zd_text_metrics text_metrics;
    zd_surface surface = {
        pixels, 256U, 80U, 256U * 4U, ZD_PIXEL_RGBX8888
    };
    int32_t drawn_width = 0;
    size_t changed_pixels = 0U;
    size_t i;

    CHECK(font_data != NULL);
    if (font_data == NULL) return;

    if (zd_font_init(&font, font_data, font_size, 0U, 20U,
                     font_scratch, sizeof(font_scratch)) != ZD_OK) {
        CHECK(0);
        free(font_data);
        return;
    }
    CHECK(zd_font_get_metrics(&font, &font_metrics) == ZD_OK);
    CHECK(zd_font_set_pixel_height(&font, 24U) == ZD_OK);
    CHECK(zd_font_get_metrics(&font, &font_metrics) == ZD_OK);
    CHECK(font_metrics.pixel_height == 24U);
    CHECK(zd_font_set_pixel_height(&font, 20U) == ZD_OK);
    CHECK(zd_font_get_metrics(&font, &font_metrics) == ZD_OK);
    CHECK(font_metrics.pixel_height == 20U);
    CHECK(font_metrics.ascent > 0);
    CHECK(font_metrics.line_height >= 20U);

    CHECK(zd_measure_text(&font, "TTF 中 文 A", 0U,
                          &text_metrics) == ZD_OK);
    CHECK(text_metrics.width > 0);
    CHECK(text_metrics.line_count == 1U);
    CHECK(text_metrics.height == font_metrics.line_height);

    CHECK(zd_measure_text(&font, "A\n中", 0U,
                          &text_metrics) == ZD_OK);
    CHECK(text_metrics.line_count == 2U);
    CHECK(text_metrics.height == font_metrics.line_height * 2U);

    zd_surface_clear(&surface, 0xFF000000U);
    CHECK(zd_draw_text(&surface, &font, 8, 30, "TTF 中 文 A",
                       0xFFFFFFFFU, 220U, &drawn_width) == ZD_OK);
    CHECK(drawn_width > 0);
    for (i = 0U; i < sizeof(pixels); i += 4U) {
        if (pixels[i] != 0U || pixels[i + 1U] != 0U ||
            pixels[i + 2U] != 0U) {
            ++changed_pixels;
        }
    }
    CHECK(changed_pixels > 20U);

    zd_surface_clear(&surface, 0xFF000000U);
    CHECK(zd_draw_text(&surface, &font, 10, 30, "MMMMMMMM",
                       0xFFFFFFFFU, 20U, &drawn_width) == ZD_OK);
    {
        int outside_changed = 0;
        for (i = 0U; i < 80U; ++i) {
            size_t x;
            for (x = 30U; x < 256U; ++x) {
                size_t p = (i * 256U + x) * 4U;
                if (pixels[p] != 0U || pixels[p + 1U] != 0U ||
                    pixels[p + 2U] != 0U) {
                    outside_changed = 1;
                }
            }
        }
        CHECK(outside_changed == 0);
    }

    CHECK(zd_draw_text(&surface, &font, 8, 30,
                       "\xF0\x28\x8C\x28", 0xFFFFFFFFU,
                       100U, &drawn_width) == ZD_OK);

    CHECK(zd_font_init(&font, invalid_data, sizeof(invalid_data),
                       0U, 18U, font_scratch,
                       sizeof(font_scratch)) == ZD_ERR_FORMAT);
    {
        zd_font second_face;
        CHECK(zd_font_init(&second_face, font_data, font_size, 1U, 18U,
                           font_scratch, sizeof(font_scratch)) == ZD_OK);
    }
    CHECK(zd_font_init(&font, font_data, font_size, 0U, 18U,
                       font_scratch, ZD_FONT_SCRATCH_MIN_SIZE - 1U)
          == ZD_ERR_ARGUMENT);

    free(font_data);
}

static void test_declarative_ui(void)
{
    uint8_t pixels[64U * 32U * 4U] = {0U};
    zd_surface surface = {
        pixels, 64U, 32U, 64U * 4U, ZD_PIXEL_RGBX8888
    };
    static const zd_ui_node button = {
        .kind = ZD_UI_BUTTON,
        .id = "action",
        .height = 12U,
        .background = 0xFF3366CCU
    };
    static const zd_ui_node *const children[] = {
        &button
    };
    static const zd_ui_node root = {
        .kind = ZD_UI_COLUMN,
        .id = "root",
        .background = 0xFF112233U,
        .padding = 2U,
        .children = children,
        .child_count = 1U
    };
    zd_rect viewport = {0, 0, 64U, 32U};
    size_t root_pixel = 0U;
    size_t button_pixel = (size_t)(5U * 64U + 40U) * 4U;

    CHECK(zd_ui_render(&surface, &root, viewport) == ZD_OK);
    CHECK(pixels[root_pixel] == 0x11U);
    CHECK(pixels[root_pixel + 1U] == 0x22U);
    CHECK(pixels[root_pixel + 2U] == 0x33U);
    CHECK(pixels[button_pixel] == 0x33U);
    CHECK(pixels[button_pixel + 1U] == 0x66U);
    CHECK(pixels[button_pixel + 2U] == 0xCCU);
}

int main(void)
{
    test_surface_format();
    test_pitch_validation();
    test_clipping();
    test_alpha_blending();
    test_row_gap_distribution();
    test_hit_test_and_hover_paint();
    test_ttf_rendering();
    test_declarative_ui();

    if (failures != 0U) {
        (void)fprintf(stderr, "%u test(s) failed\n", failures);
        return 1;
    }
    (void)puts("All zdesktop tests passed.");
    return 0;
}
