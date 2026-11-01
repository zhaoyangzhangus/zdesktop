#include "zdesktop.h"

#include <stdio.h>
#include <string.h>

static unsigned int failures;

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

static void test_declarative_ui(void)
{
    uint8_t pixels[64U * 32U * 4U] = {0U};
    zd_surface surface = {
        pixels, 64U, 32U, 64U * 4U, ZD_PIXEL_RGBX8888
    };
    static const zd_ui_node button = {
        .kind = ZD_UI_BUTTON,
        .id = "action",
        .width = 0U,
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
    test_declarative_ui();

    if (failures != 0U) {
        (void)fprintf(stderr, "%u test(s) failed\n", failures);
        return 1;
    }

    (void)puts("All zdesktop renderer tests passed.");
    return 0;
}
