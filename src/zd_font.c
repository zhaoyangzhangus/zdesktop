#include "zd_internal.h"

#include <limits.h>

/*
 * stb_truetype is vendored and configured to use the font's caller-owned
 * scratch arena. No malloc/free, libc string routine, or libm symbol is used.
 * The cubic/CFF helpers are deliberately unavailable: this API accepts only
 * TrueType glyf outlines, which use quadratic curves.
 */
static void *zd_font_scratch_alloc(size_t size, void *userdata);
static void zd_font_scratch_free(void *memory, void *userdata);
static void *zd_stbtt_memcpy(void *destination, const void *source, size_t size);
static void *zd_stbtt_memset(void *destination, int value, size_t size);
static size_t zd_stbtt_strlen(const char *text);
static float zd_stbtt_sqrt(float value);
static float zd_stbtt_fabs(float value);
static float zd_stbtt_fmod(float value, float divisor);
static int zd_stbtt_ifloor(float value);
static int zd_stbtt_iceil(float value);
static float zd_stbtt_unsupported_binary_math(float a, float b);
static float zd_stbtt_unsupported_unary_math(float value);

#define STBTT_malloc(size, userdata) \
    zd_font_scratch_alloc((size_t)(size), (userdata))
#define STBTT_free(memory, userdata) \
    zd_font_scratch_free((memory), (userdata))
#define STBTT_memcpy zd_stbtt_memcpy
#define STBTT_memset zd_stbtt_memset
#define STBTT_strlen zd_stbtt_strlen
#define STBTT_assert(condition) ((void)(condition))
#define STBTT_sqrt(value) zd_stbtt_sqrt((float)(value))
#define STBTT_fabs(value) zd_stbtt_fabs((float)(value))
#define STBTT_fmod(value, divisor) \
    zd_stbtt_fmod((float)(value), (float)(divisor))
#define STBTT_ifloor(value) zd_stbtt_ifloor((float)(value))
#define STBTT_iceil(value) zd_stbtt_iceil((float)(value))
#define STBTT_pow(a, b) \
    zd_stbtt_unsupported_binary_math((float)(a), (float)(b))
#define STBTT_cos(value) zd_stbtt_unsupported_unary_math((float)(value))
#define STBTT_acos(value) zd_stbtt_unsupported_unary_math((float)(value))
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

static uint16_t zd_read_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t zd_read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) |
           (uint32_t)p[3];
}

static int zd_tag_equal(const uint8_t *p, const char tag[4])
{
    return p[0] == (uint8_t)tag[0] &&
           p[1] == (uint8_t)tag[1] &&
           p[2] == (uint8_t)tag[2] &&
           p[3] == (uint8_t)tag[3];
}

typedef struct {
    uint32_t offset;
    uint32_t length;
} zd_ttf_table;

static int zd_ttf_find_table(const uint8_t *data, size_t size,
                             uint32_t face_offset, const char tag[4],
                             zd_ttf_table *table)
{
    uint16_t count;
    uint32_t i;
    uint64_t directory_end;

    if ((uint64_t)face_offset + 12U > size) return 0;
    count = zd_read_be16(data + face_offset + 4U);
    directory_end = (uint64_t)face_offset + 12U +
                    (uint64_t)count * 16U;
    if (directory_end > size) return 0;

    for (i = 0U; i < count; ++i) {
        const uint8_t *record = data + face_offset + 12U + (size_t)i * 16U;
        uint32_t offset;
        uint32_t length;

        if (!zd_tag_equal(record, tag)) continue;
        offset = zd_read_be32(record + 8U);
        length = zd_read_be32(record + 12U);
        if ((uint64_t)offset + length > size) return 0;
        table->offset = offset;
        table->length = length;
        return 1;
    }
    return 0;
}

static int zd_ttf_validate(const uint8_t *data, size_t size,
                           uint32_t face_index, uint32_t *face_offset_out)
{
    uint32_t face_offset = 0U;
    uint32_t signature;
    uint16_t table_count;
    uint16_t glyph_count;
    uint16_t horizontal_metrics;
    int16_t loca_format;
    zd_ttf_table head;
    zd_ttf_table hhea;
    zd_ttf_table maxp;
    zd_ttf_table hmtx;
    zd_ttf_table loca;
    zd_ttf_table glyf;
    zd_ttf_table cmap;
    uint64_t directory_end;
    uint64_t required_size;
    uint32_t i;
    int cmap_supported = 0;

    if (data == NULL || size < 12U || size > (size_t)INT_MAX) return 0;

    if (zd_tag_equal(data, "ttcf")) {
        uint32_t face_count;

        if (size < 12U) return 0;
        face_count = zd_read_be32(data + 8U);
        if (face_count == 0U ||
            (uint64_t)12U + (uint64_t)face_count * 4U > size ||
            face_index >= face_count) {
            return 0;
        }
        face_offset = zd_read_be32(data + 12U + (size_t)face_index * 4U);
    } else if (face_index != 0U) {
        return 0;
    }

    if ((uint64_t)face_offset + 12U > size) return 0;
    signature = zd_read_be32(data + face_offset);
    if (signature != 0x00010000U && signature != 0x74727565U) {
        return 0;
    }

    table_count = zd_read_be16(data + face_offset + 4U);
    directory_end = (uint64_t)face_offset + 12U +
                    (uint64_t)table_count * 16U;
    if (directory_end > size) return 0;

    for (i = 0U; i < table_count; ++i) {
        const uint8_t *record = data + face_offset + 12U + (size_t)i * 16U;
        uint32_t offset = zd_read_be32(record + 8U);
        uint32_t length = zd_read_be32(record + 12U);

        if ((uint64_t)offset + length > size) return 0;
    }

    if (!zd_ttf_find_table(data, size, face_offset, "head", &head) ||
        !zd_ttf_find_table(data, size, face_offset, "hhea", &hhea) ||
        !zd_ttf_find_table(data, size, face_offset, "maxp", &maxp) ||
        !zd_ttf_find_table(data, size, face_offset, "hmtx", &hmtx) ||
        !zd_ttf_find_table(data, size, face_offset, "loca", &loca) ||
        !zd_ttf_find_table(data, size, face_offset, "glyf", &glyf) ||
        !zd_ttf_find_table(data, size, face_offset, "cmap", &cmap)) {
        return 0;
    }

    if (head.length < 54U || hhea.length < 36U ||
        maxp.length < 6U || cmap.length < 4U || glyf.length == 0U) {
        return 0;
    }

    glyph_count = zd_read_be16(data + maxp.offset + 4U);
    horizontal_metrics = zd_read_be16(data + hhea.offset + 34U);
    loca_format = (int16_t)zd_read_be16(data + head.offset + 50U);
    if (glyph_count == 0U || horizontal_metrics == 0U ||
        horizontal_metrics > glyph_count ||
        (loca_format != 0 && loca_format != 1)) {
        return 0;
    }

    required_size = (uint64_t)horizontal_metrics * 4U +
                    (uint64_t)(glyph_count - horizontal_metrics) * 2U;
    if (required_size > hmtx.length) return 0;

    required_size = ((uint64_t)glyph_count + 1U) *
                    ((loca_format == 0) ? 2U : 4U);
    if (required_size > loca.length) return 0;

    {
        uint32_t previous = 0U;
        for (i = 0U; i <= glyph_count; ++i) {
            uint32_t current;

            if (loca_format == 0) {
                current = (uint32_t)zd_read_be16(
                    data + loca.offset + (size_t)i * 2U) * 2U;
            } else {
                current = zd_read_be32(
                    data + loca.offset + (size_t)i * 4U);
            }
            if (current < previous || current > glyf.length) return 0;
            previous = current;
        }
    }

    {
        uint16_t encoding_count = zd_read_be16(data + cmap.offset + 2U);
        if ((uint64_t)4U + (uint64_t)encoding_count * 8U > cmap.length) {
            return 0;
        }
        for (i = 0U; i < encoding_count; ++i) {
            const uint8_t *record = data + cmap.offset + 4U +
                                    (size_t)i * 8U;
            uint32_t sub_offset = zd_read_be32(record + 4U);
            uint16_t format;
            uint64_t sub_length;

            if ((uint64_t)sub_offset + 2U > cmap.length) continue;
            format = zd_read_be16(data + cmap.offset + sub_offset);
            if (format == 4U) {
                if ((uint64_t)sub_offset + 4U > cmap.length) continue;
                sub_length = zd_read_be16(
                    data + cmap.offset + sub_offset + 2U);
            } else if (format == 12U) {
                if ((uint64_t)sub_offset + 8U > cmap.length) continue;
                sub_length = zd_read_be32(
                    data + cmap.offset + sub_offset + 4U);
            } else {
                continue;
            }
            if (sub_length >= 16U &&
                (uint64_t)sub_offset + sub_length <= cmap.length) {
                cmap_supported = 1;
                break;
            }
        }
    }

    if (!cmap_supported) return 0;
    *face_offset_out = face_offset;
    return 1;
}

static zd_result zd_font_info_init(zd_font *font, stbtt_fontinfo *info)
{
    if (font == NULL || info == NULL || font->data == NULL ||
        font->face_offset > (uint32_t)INT_MAX) {
        return ZD_ERR_ARGUMENT;
    }
    if (stbtt_InitFont(info, font->data, (int)font->face_offset) == 0 ||
        info->glyf == 0 || info->cff.size != 0) {
        return ZD_ERR_FORMAT;
    }
    info->userdata = font;
    return ZD_OK;
}

static int32_t zd_round_float(float value)
{
    if (value >= (float)INT32_MAX) return INT32_MAX;
    if (value <= (float)INT32_MIN) return INT32_MIN;
    return (value >= 0.0f)
         ? (int32_t)(value + 0.5f)
         : (int32_t)(value - 0.5f);
}

static uint32_t zd_utf8_next(const char **cursor)
{
    const uint8_t *p = (const uint8_t *)*cursor;
    uint32_t cp;
    uint32_t needed;
    uint32_t minimum;
    uint32_t i;

    if (p[0] < 0x80U) {
        *cursor += 1;
        return p[0];
    }
    if (p[0] >= 0xC2U && p[0] <= 0xDFU) {
        cp = p[0] & 0x1FU;
        needed = 1U;
        minimum = 0x80U;
    } else if (p[0] >= 0xE0U && p[0] <= 0xEFU) {
        cp = p[0] & 0x0FU;
        needed = 2U;
        minimum = 0x800U;
    } else if (p[0] >= 0xF0U && p[0] <= 0xF4U) {
        cp = p[0] & 0x07U;
        needed = 3U;
        minimum = 0x10000U;
    } else {
        *cursor += 1;
        return 0xFFFDU;
    }

    for (i = 1U; i <= needed; ++i) {
        if (p[i] == 0U || (p[i] & 0xC0U) != 0x80U) {
            *cursor += 1;
            return 0xFFFDU;
        }
        cp = (cp << 6) | (p[i] & 0x3FU);
    }

    if (cp < minimum || cp > 0x10FFFFU ||
        (cp >= 0xD800U && cp <= 0xDFFFU)) {
        *cursor += 1;
        return 0xFFFDU;
    }

    *cursor += needed + 1U;
    return cp;
}

static uint32_t zd_resolve_codepoint(const stbtt_fontinfo *info,
                                     uint32_t codepoint)
{
    if (stbtt_FindGlyphIndex(info, (int)codepoint) != 0 ||
        codepoint == 0U) {
        return codepoint;
    }
    if (stbtt_FindGlyphIndex(info, 0xFFFDU) != 0) return 0xFFFDU;
    if (stbtt_FindGlyphIndex(info, (int)'?') != 0) return (uint32_t)'?';
    return 0U;
}

static void *zd_font_scratch_alloc(size_t size, void *userdata)
{
    zd_font *font = (zd_font *)userdata;
    uintptr_t alignment_mask = (uintptr_t)sizeof(void *) - 1U;
    uintptr_t base;
    uintptr_t current;
    uintptr_t aligned;
    size_t start;

    if (font == NULL || font->scratch == NULL) return NULL;
    base = (uintptr_t)font->scratch;
    if (font->scratch_used > font->scratch_size ||
        base > (uintptr_t)-1 - font->scratch_used) {
        font->scratch_failed = 1U;
        return NULL;
    }
    current = base + font->scratch_used;
    if (current > (uintptr_t)-1 - alignment_mask) {
        font->scratch_failed = 1U;
        return NULL;
    }
    aligned = (current + alignment_mask) & ~alignment_mask;
    start = (size_t)(aligned - base);
    if (start > font->scratch_size ||
        size > font->scratch_size - start) {
        font->scratch_failed = 1U;
        return NULL;
    }
    font->scratch_used = start + size;
    return font->scratch + start;
}

static void zd_font_scratch_free(void *memory, void *userdata)
{
    (void)memory;
    (void)userdata;
}

static void *zd_stbtt_memcpy(void *destination, const void *source,
                             size_t size)
{
    uint8_t *dst = (uint8_t *)destination;
    const uint8_t *src = (const uint8_t *)source;
    size_t i;

    for (i = 0U; i < size; ++i) dst[i] = src[i];
    return destination;
}

static void *zd_stbtt_memset(void *destination, int value, size_t size)
{
    uint8_t *dst = (uint8_t *)destination;
    size_t i;

    for (i = 0U; i < size; ++i) dst[i] = (uint8_t)value;
    return destination;
}

static size_t zd_stbtt_strlen(const char *text)
{
    size_t length = 0U;
    while (text[length] != '\0') ++length;
    return length;
}

static float zd_stbtt_sqrt(float value)
{
    float estimate;
    unsigned int i;

    if (value <= 0.0f) return 0.0f;
    estimate = (value >= 1.0f) ? value : 1.0f;
    for (i = 0U; i < 24U; ++i) {
        estimate = 0.5f * (estimate + value / estimate);
    }
    return estimate;
}

static float zd_stbtt_fabs(float value)
{
    return (value < 0.0f) ? -value : value;
}

static float zd_stbtt_fmod(float value, float divisor)
{
    int32_t quotient;

    if (divisor == 0.0f ||
        value > (float)INT32_MAX || value < (float)INT32_MIN) {
        return 0.0f;
    }
    quotient = (int32_t)(value / divisor);
    return value - (float)quotient * divisor;
}

static int zd_stbtt_ifloor(float value)
{
    int result = (int)value;
    if ((float)result > value) --result;
    return result;
}

static int zd_stbtt_iceil(float value)
{
    int result = (int)value;
    if ((float)result < value) ++result;
    return result;
}

static float zd_stbtt_unsupported_binary_math(float a, float b)
{
    (void)a;
    (void)b;
    return 0.0f;
}

static float zd_stbtt_unsupported_unary_math(float value)
{
    (void)value;
    return 0.0f;
}

zd_result zd_font_init(zd_font *font,
                       const void *font_data,
                       size_t font_data_size,
                       uint32_t face_index,
                       uint32_t pixel_height,
                       void *scratch,
                       size_t scratch_size)
{
    uint32_t face_offset;
    stbtt_fontinfo info;

    if (font == NULL || font_data == NULL || scratch == NULL ||
        scratch_size < ZD_FONT_SCRATCH_MIN_SIZE ||
        pixel_height == 0U || pixel_height > 512U ||
        font_data_size > (size_t)INT_MAX) {
        return ZD_ERR_ARGUMENT;
    }

    if (!zd_ttf_validate((const uint8_t *)font_data, font_data_size,
                         face_index, &face_offset)) {
        return ZD_ERR_FORMAT;
    }
    if (stbtt_InitFont(&info, (const unsigned char *)font_data,
                       (int)face_offset) == 0 ||
        info.glyf == 0 || info.cff.size != 0) {
        return ZD_ERR_FORMAT;
    }

    font->data = (const uint8_t *)font_data;
    font->data_size = font_data_size;
    font->face_index = face_index;
    font->face_offset = face_offset;
    font->pixel_height = 0U;
    font->scratch = (uint8_t *)scratch;
    font->scratch_size = scratch_size;
    font->scratch_used = 0U;
    font->scratch_failed = 0U;
    font->scale = 0.0f;
    font->ascent = 0;
    font->descent = 0;
    font->line_gap = 0;
    font->line_height = 0U;

    return zd_font_set_pixel_height(font, pixel_height);
}

zd_result zd_font_set_pixel_height(zd_font *font, uint32_t pixel_height)
{
    stbtt_fontinfo info;
    zd_result status;
    int ascent;
    int descent;
    int line_gap;
    float line_height;

    if (font == NULL || font->data == NULL ||
        pixel_height == 0U || pixel_height > 512U) {
        return ZD_ERR_ARGUMENT;
    }

    status = zd_font_info_init(font, &info);
    if (status != ZD_OK) return status;

    font->scale = stbtt_ScaleForPixelHeight(&info, (float)pixel_height);
    if (font->scale <= 0.0f) return ZD_ERR_FORMAT;

    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
    font->ascent = zd_round_float((float)ascent * font->scale);
    font->descent = zd_round_float((float)descent * font->scale);
    font->line_gap = zd_round_float((float)line_gap * font->scale);
    line_height = ((float)ascent - (float)descent +
                   (float)line_gap) * font->scale;
    if (line_height < 1.0f) line_height = 1.0f;

    font->pixel_height = pixel_height;
    font->line_height = (uint32_t)zd_round_float(line_height);
    if (font->line_height == 0U) font->line_height = 1U;
    return ZD_OK;
}

zd_result zd_font_get_metrics(const zd_font *font,
                              zd_font_metrics *out_metrics)
{
    if (font == NULL || out_metrics == NULL ||
        font->data == NULL || font->pixel_height == 0U) {
        return ZD_ERR_ARGUMENT;
    }

    out_metrics->pixel_height = font->pixel_height;
    out_metrics->ascent = font->ascent;
    out_metrics->descent = font->descent;
    out_metrics->line_gap = font->line_gap;
    out_metrics->line_height = font->line_height;
    return ZD_OK;
}

zd_result zd_measure_text(const zd_font *font, const char *utf8,
                          uint32_t max_width,
                          zd_text_metrics *out_metrics)
{
    stbtt_fontinfo info;
    const char *cursor;
    float pen_x = 0.0f;
    float maximum_width = 0.0f;
    uint32_t line_count = 1U;
    uint32_t previous = 0U;
    zd_result status;

    if (font == NULL || utf8 == NULL || out_metrics == NULL ||
        font->pixel_height == 0U) {
        return ZD_ERR_ARGUMENT;
    }
    status = zd_font_info_init((zd_font *)font, &info);
    if (status != ZD_OK) return status;

    cursor = utf8;
    while (*cursor != '\0') {
        uint32_t codepoint = zd_utf8_next(&cursor);
        uint32_t resolved;
        int advance;
        int bearing;
        float kern = 0.0f;
        float next_x;

        if (codepoint == '\r') continue;
        if (codepoint == '\n') {
            if (pen_x > maximum_width) maximum_width = pen_x;
            pen_x = 0.0f;
            previous = 0U;
            ++line_count;
            continue;
        }

        resolved = zd_resolve_codepoint(&info, codepoint);
        if (resolved == 0U) continue;
        if (previous != 0U) {
            kern = (float)stbtt_GetCodepointKernAdvance(
                &info, (int)previous, (int)resolved) * font->scale;
        }
        stbtt_GetCodepointHMetrics(
            &info, (int)resolved, &advance, &bearing);
        (void)bearing;
        next_x = pen_x + kern + (float)advance * font->scale;
        if (max_width != 0U && next_x > (float)max_width) break;
        pen_x = next_x;
        previous = resolved;
    }

    if (pen_x > maximum_width) maximum_width = pen_x;
    if (maximum_width > (float)INT32_MAX ||
        (uint64_t)line_count * font->line_height > UINT32_MAX) {
        return ZD_ERR_ARGUMENT;
    }

    out_metrics->width = zd_round_float(maximum_width);
    out_metrics->line_count = line_count;
    out_metrics->height = line_count * font->line_height;
    return ZD_OK;
}

zd_result zd_draw_text(zd_surface *surface, zd_font *font,
                       int32_t x, int32_t baseline_y,
                       const char *utf8, uint32_t argb,
                       uint32_t max_width, int32_t *out_width)
{
    stbtt_fontinfo info;
    const char *cursor;
    float pen_x = 0.0f;
    float maximum_width = 0.0f;
    uint32_t previous = 0U;
    int64_t baseline = baseline_y;
    uint8_t color_alpha = (uint8_t)(argb >> 24);
    uint32_t color_rgb = argb & 0x00FFFFFFU;
    zd_result status;
    zd_result surface_status = zd_surface_validate(surface);

    if (surface_status != ZD_OK) return surface_status;
    if (font == NULL || utf8 == NULL || out_width == NULL ||
        font->pixel_height == 0U) {
        return ZD_ERR_ARGUMENT;
    }
    *out_width = 0;
    status = zd_font_info_init(font, &info);
    if (status != ZD_OK) return status;

    cursor = utf8;
    while (*cursor != '\0') {
        uint32_t codepoint = zd_utf8_next(&cursor);
        uint32_t resolved;
        int advance;
        int bearing;
        int x0;
        int y0;
        int x1;
        int y1;
        int width;
        int height;
        float kern = 0.0f;
        float glyph_x;
        float next_x;
        uint64_t bitmap_bytes;

        if (codepoint == '\r') continue;
        if (codepoint == '\n') {
            if (pen_x > maximum_width) maximum_width = pen_x;
            pen_x = 0.0f;
            previous = 0U;
            baseline += font->line_height;
            continue;
        }

        resolved = zd_resolve_codepoint(&info, codepoint);
        if (resolved == 0U) continue;
        if (previous != 0U) {
            kern = (float)stbtt_GetCodepointKernAdvance(
                &info, (int)previous, (int)resolved) * font->scale;
        }
        stbtt_GetCodepointHMetrics(
            &info, (int)resolved, &advance, &bearing);
        (void)bearing;
        glyph_x = pen_x + kern;
        next_x = glyph_x + (float)advance * font->scale;
        if (max_width != 0U && next_x > (float)max_width) break;

        if (color_alpha != 0U) {
            stbtt_GetCodepointBitmapBox(
                &info, (int)resolved, font->scale, font->scale,
                &x0, &y0, &x1, &y1);
            width = x1 - x0;
            height = y1 - y0;

            if (width > 0 && height > 0) {
                uint32_t row;
                bitmap_bytes = (uint64_t)(uint32_t)width *
                               (uint64_t)(uint32_t)height;
                if (bitmap_bytes > font->scratch_size) {
                    return ZD_ERR_SCRATCH;
                }

                font->scratch_used = (size_t)bitmap_bytes;
                font->scratch_failed = 0U;
                (void)zd_stbtt_memset(font->scratch, 0,
                                      (size_t)bitmap_bytes);
                stbtt_MakeCodepointBitmap(
                    &info, font->scratch, width, height, width,
                    font->scale, font->scale, (int)resolved);

                if (font->scratch_failed != 0U) {
                    font->scratch_used = 0U;
                    return ZD_ERR_SCRATCH;
                }

                for (row = 0U; row < (uint32_t)height; ++row) {
                    uint32_t column;
                    int64_t pixel_y = baseline + y0 + row;

                    if (pixel_y < 0 ||
                        pixel_y >= (int64_t)surface->height) {
                        continue;
                    }
                    for (column = 0U; column < (uint32_t)width; ++column) {
                        uint8_t coverage =
                            font->scratch[(size_t)row * (size_t)width +
                                          column];
                        float px_float = (float)x + glyph_x +
                                         (float)x0 + (float)column;
                        int64_t pixel_x;
                        uint32_t alpha;

                        if (coverage == 0U) continue;
                        pixel_x = (int64_t)zd_round_float(px_float);
                        if (pixel_x < 0 ||
                            pixel_x >= (int64_t)surface->width ||
                            (max_width != 0U &&
                             (float)(pixel_x - x) >= (float)max_width)) {
                            continue;
                        }
                        alpha = ((uint32_t)color_alpha * coverage + 127U) /
                                255U;
                        if (alpha != 0U) {
                            zd_blend_coverage(surface, (uint32_t)pixel_x,
                                (uint32_t)pixel_y,
                                (alpha << 24) | color_rgb);
                        }
                    }
                }
                font->scratch_used = 0U;
            }
        }

        pen_x = next_x;
        previous = resolved;
    }

    if (pen_x > maximum_width) maximum_width = pen_x;
    if (maximum_width > (float)INT32_MAX) {
        *out_width = INT32_MAX;
    } else {
        *out_width = zd_round_float(maximum_width);
    }
    return ZD_OK;
}
