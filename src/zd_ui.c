#include "zd_internal.h"

#include <limits.h>

#define ZD_UI_MAX_DEPTH 64U

static int32_t zd_i64_to_i32(int64_t value)
{
    if (value < INT32_MIN) return INT32_MIN;
    if (value > INT32_MAX) return INT32_MAX;
    return (int32_t)value;
}

static uint32_t zd_min_u32(uint32_t a, uint32_t b)
{
    return (a < b) ? a : b;
}

static void zd_render_node(zd_surface *surface, const zd_ui_node *node,
                           zd_rect bounds, uint32_t depth)
{
    uint32_t padding;
    uint32_t inner_width;
    uint32_t inner_height;
    int32_t inner_x;
    int32_t inner_y;
    uint32_t i;
    int is_row;

    if (node == NULL || depth > ZD_UI_MAX_DEPTH ||
        bounds.width == 0U || bounds.height == 0U) {
        return;
    }

    if ((node->background >> 24) != 0U) {
        zd_fill_rect(surface, bounds, node->background);
    }

    if (node->border_width != 0U &&
        (node->border_color >> 24) != 0U) {
        zd_stroke_rect(surface, bounds, node->border_width,
                       node->border_color);
    }

    if (node->kind == ZD_UI_TEXT || node->kind == ZD_UI_BUTTON) {
        uint32_t text_height = 7U;
        uint32_t text_width;
        int32_t text_y;

        padding = node->padding;
        inner_width = (bounds.width > padding * 2U)
                    ? bounds.width - padding * 2U : 0U;
        inner_height = (bounds.height > padding * 2U)
                     ? bounds.height - padding * 2U : 0U;
        text_width = inner_width;
        text_y = zd_i64_to_i32((int64_t)bounds.y + padding);
        if (inner_height > text_height) {
            text_y = zd_i64_to_i32((int64_t)text_y +
                                   (inner_height - text_height) / 2U);
        }

        if (node->text != NULL && inner_width != 0U &&
            (node->foreground >> 24) != 0U) {
            (void)zd_draw_text(surface,
                zd_i64_to_i32((int64_t)bounds.x + padding),
                text_y, node->text, node->foreground, 1U, text_width);
        }
        return;
    }

    if (node->kind != ZD_UI_PANEL &&
        node->kind != ZD_UI_ROW &&
        node->kind != ZD_UI_COLUMN) {
        return;
    }

    if (node->children == NULL || node->child_count == 0U) {
        return;
    }

    padding = node->padding;
    inner_width = (bounds.width > padding * 2U)
                ? bounds.width - padding * 2U : 0U;
    inner_height = (bounds.height > padding * 2U)
                 ? bounds.height - padding * 2U : 0U;
    inner_x = zd_i64_to_i32((int64_t)bounds.x + padding);
    inner_y = zd_i64_to_i32((int64_t)bounds.y + padding);
    is_row = (node->kind == ZD_UI_ROW);

    {
        uint32_t main_total = is_row ? inner_width : inner_height;
        uint64_t fixed_total = 0U;
        uint64_t gap_total = (uint64_t)node->gap *
                             (node->child_count - 1U);
        uint32_t auto_count = 0U;
        uint32_t auto_space;
        uint32_t auto_left;
        uint32_t remaining_main;
        int64_t cursor = is_row ? inner_x : inner_y;

        for (i = 0U; i < node->child_count; ++i) {
            const zd_ui_node *child = node->children[i];
            uint32_t main_size;

            if (child == NULL) continue;
            main_size = is_row ? child->width : child->height;
            if (main_size == 0U) {
                ++auto_count;
            } else {
                fixed_total += main_size;
            }
        }

        if (gap_total >= main_total) {
            main_total = 0U;
        } else {
            main_total -= (uint32_t)gap_total;
        }

        if (fixed_total >= main_total) {
            auto_space = 0U;
        } else {
            auto_space = main_total - (uint32_t)fixed_total;
        }
        auto_left = auto_count;
        remaining_main = main_total;

        for (i = 0U; i < node->child_count; ++i) {
            const zd_ui_node *child = node->children[i];
            uint32_t main_size;
            uint32_t cross_size;
            zd_rect child_bounds;

            if (child == NULL) continue;

            main_size = is_row ? child->width : child->height;
            if (main_size == 0U) {
                if (auto_left != 0U) {
                    main_size = auto_space / auto_left;
                    auto_space -= main_size;
                    --auto_left;
                }
            }
            if (main_size > remaining_main) {
                main_size = remaining_main;
            }

            cross_size = is_row ? child->height : child->width;
            if (cross_size == 0U) {
                cross_size = is_row ? inner_height : inner_width;
            } else {
                cross_size = zd_min_u32(cross_size,
                    is_row ? inner_height : inner_width);
            }

            if (is_row) {
                child_bounds.x = zd_i64_to_i32(cursor);
                child_bounds.y = inner_y;
                child_bounds.width = main_size;
                child_bounds.height = cross_size;
            } else {
                child_bounds.x = inner_x;
                child_bounds.y = zd_i64_to_i32(cursor);
                child_bounds.width = cross_size;
                child_bounds.height = main_size;
            }

            zd_render_node(surface, child, child_bounds, depth + 1U);

            cursor += main_size;
            remaining_main -= main_size;
            if (i + 1U < node->child_count) {
                uint32_t actual_gap = zd_min_u32(node->gap, remaining_main);
                cursor += actual_gap;
            }
        }
    }
}

zd_result zd_ui_render(zd_surface *surface,
                       const zd_ui_node *root,
                       zd_rect viewport)
{
    zd_result surface_status = zd_surface_validate(surface);

    if (surface_status != ZD_OK) {
        return surface_status;
    }
    if (root == NULL || viewport.width == 0U || viewport.height == 0U) {
        return ZD_ERR_ARGUMENT;
    }

    zd_render_node(surface, root, viewport, 0U);
    return ZD_OK;
}
