#include "zd_internal.h"

#include <limits.h>

#define ZD_UI_MAX_DEPTH 64U

typedef void (*zd_child_fn)(const zd_ui_node *child,
                            zd_rect child_bounds,
                            void *context);

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

/*
 * This is the single layout path used by both painting and hit testing.
 * Null child slots are ignored and do not consume a gap.
 */
static void zd_layout_children(const zd_ui_node *node, zd_rect bounds,
                               zd_child_fn callback, void *context)
{
    uint32_t i;
    uint32_t visible_count = 0U;
    uint32_t auto_count = 0U;
    uint32_t auto_left;
    uint32_t auto_space;
    uint32_t remaining_main;
    uint32_t main_total;
    uint32_t inner_width;
    uint32_t inner_height;
    uint32_t padding;
    uint32_t main_axis_row;
    uint64_t fixed_total = 0U;
    uint64_t gap_total;
    int32_t inner_x;
    int32_t inner_y;
    int64_t cursor;
    int64_t main_end;

    if (node == NULL || node->children == NULL ||
        node->child_count == 0U || callback == NULL) {
        return;
    }

    padding = node->padding;
    inner_width = (bounds.width > padding * 2U)
                ? bounds.width - padding * 2U : 0U;
    inner_height = (bounds.height > padding * 2U)
                 ? bounds.height - padding * 2U : 0U;
    inner_x = zd_i64_to_i32((int64_t)bounds.x + padding);
    inner_y = zd_i64_to_i32((int64_t)bounds.y + padding);
    main_axis_row = (node->kind == ZD_UI_ROW) ? 1U : 0U;
    main_total = main_axis_row ? inner_width : inner_height;
    cursor = main_axis_row ? inner_x : inner_y;
    main_end = cursor + main_total;

    for (i = 0U; i < node->child_count; ++i) {
        const zd_ui_node *child = node->children[i];
        uint32_t main_size;

        if (child == NULL) continue;
        ++visible_count;
        main_size = main_axis_row ? child->width : child->height;
        if (main_size == 0U) {
            ++auto_count;
        } else {
            fixed_total += main_size;
        }
    }

    if (visible_count == 0U) return;

    gap_total = (uint64_t)node->gap * (visible_count - 1U);
    if (gap_total >= main_total) {
        main_total = 0U;
    } else {
        main_total -= (uint32_t)gap_total;
    }

    auto_space = (fixed_total >= main_total)
               ? 0U : main_total - (uint32_t)fixed_total;
    auto_left = auto_count;
    remaining_main = main_total;

    visible_count = 0U;
    for (i = 0U; i < node->child_count; ++i) {
        const zd_ui_node *child = node->children[i];
        uint32_t main_size;
        uint32_t cross_size;
        zd_rect child_bounds;

        if (child == NULL) continue;

        main_size = main_axis_row ? child->width : child->height;
        if (main_size == 0U && auto_left != 0U) {
            main_size = auto_space / auto_left;
            auto_space -= main_size;
            --auto_left;
        }
        if (main_size > remaining_main) {
            main_size = remaining_main;
        }

        cross_size = main_axis_row ? child->height : child->width;
        if (cross_size == 0U) {
            cross_size = main_axis_row ? inner_height : inner_width;
        } else {
            cross_size = zd_min_u32(cross_size,
                main_axis_row ? inner_height : inner_width);
        }

        if (main_axis_row != 0U) {
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

        callback(child, child_bounds, context);

        cursor += main_size;
        remaining_main -= main_size;
        ++visible_count;
        if (visible_count < (uint32_t)node->child_count) {
            /* The next non-null child may be later; gaps are corrected below. */
            uint32_t next_index;
            int has_next = 0;

            for (next_index = i + 1U; next_index < node->child_count;
                 ++next_index) {
                if (node->children[next_index] != NULL) {
                    has_next = 1;
                    break;
                }
            }
            if (has_next) {
                int64_t gap_space = main_end - cursor;
                uint32_t actual_gap = (gap_space <= 0)
                    ? 0U
                    : ((gap_space < (int64_t)node->gap)
                       ? (uint32_t)gap_space : node->gap);
                cursor += actual_gap;
            }
        }
    }
}

typedef struct {
    zd_surface *surface;
    const zd_ui_state *state;
    uint32_t depth;
} zd_render_context;

static void zd_render_node(zd_surface *surface, const zd_ui_node *node,
                           zd_rect bounds, uint32_t depth,
                           const zd_ui_state *state);

static void zd_render_child(const zd_ui_node *child, zd_rect child_bounds,
                            void *opaque)
{
    zd_render_context *context = (zd_render_context *)opaque;

    zd_render_node(context->surface, child, child_bounds,
                   context->depth + 1U, context->state);
}

static void zd_render_node(zd_surface *surface, const zd_ui_node *node,
                           zd_rect bounds, uint32_t depth,
                           const zd_ui_state *state)
{
    uint32_t background;
    uint32_t border_color;
    uint32_t border_width;
    uint32_t padding;
    uint32_t inner_width;
    uint32_t inner_height;
    int32_t text_y;

    if (node == NULL || depth > ZD_UI_MAX_DEPTH ||
        bounds.width == 0U || bounds.height == 0U) {
        return;
    }

    background = node->background;
    if (state != NULL && state->pressed == node &&
        (node->pressed_background >> 24) != 0U) {
        background = node->pressed_background;
    } else if (state != NULL && state->hovered == node &&
               (node->hover_background >> 24) != 0U) {
        background = node->hover_background;
    }

    if ((background >> 24) != 0U) {
        zd_fill_rect(surface, bounds, background);
    }

    border_color = node->border_color;
    border_width = node->border_width;
    if (state != NULL && state->focused == node &&
        (node->focus_border_color >> 24) != 0U) {
        border_color = node->focus_border_color;
        if (border_width == 0U) border_width = 2U;
    }
    if (border_width != 0U && (border_color >> 24) != 0U) {
        zd_stroke_rect(surface, bounds, border_width, border_color);
    }

    if (node->kind == ZD_UI_TEXT || node->kind == ZD_UI_BUTTON) {
        uint32_t text_height = 7U;

        padding = node->padding;
        inner_width = (bounds.width > padding * 2U)
                    ? bounds.width - padding * 2U : 0U;
        inner_height = (bounds.height > padding * 2U)
                     ? bounds.height - padding * 2U : 0U;
        text_y = zd_i64_to_i32((int64_t)bounds.y + padding);
        if (inner_height > text_height) {
            text_y = zd_i64_to_i32((int64_t)text_y +
                                   (inner_height - text_height) / 2U);
        }

        if (node->text != NULL && inner_width != 0U &&
            (node->foreground >> 24) != 0U) {
            (void)zd_draw_text(surface,
                zd_i64_to_i32((int64_t)bounds.x + padding),
                text_y, node->text, node->foreground, 1U, inner_width);
        }
        return;
    }

    if (node->kind == ZD_UI_PANEL || node->kind == ZD_UI_ROW ||
        node->kind == ZD_UI_COLUMN) {
        zd_render_context context;

        context.surface = surface;
        context.state = state;
        context.depth = depth;
        zd_layout_children(node, bounds, zd_render_child, &context);
    }
}

zd_result zd_ui_render_with_state(zd_surface *surface,
                                  const zd_ui_node *root,
                                  zd_rect viewport,
                                  const zd_ui_state *state)
{
    zd_result surface_status = zd_surface_validate(surface);

    if (surface_status != ZD_OK) {
        return surface_status;
    }
    if (root == NULL || viewport.width == 0U || viewport.height == 0U) {
        return ZD_ERR_ARGUMENT;
    }

    zd_render_node(surface, root, viewport, 0U, state);
    return ZD_OK;
}

zd_result zd_ui_render(zd_surface *surface,
                       const zd_ui_node *root,
                       zd_rect viewport)
{
    return zd_ui_render_with_state(surface, root, viewport, NULL);
}

typedef struct {
    int32_t x;
    int32_t y;
    const zd_ui_node *hit;
    zd_rect bounds;
    uint32_t depth;
} zd_hit_context;

static void zd_hit_node(const zd_ui_node *node, zd_rect bounds,
                        zd_hit_context *context);

static void zd_hit_child(const zd_ui_node *child, zd_rect child_bounds,
                         void *opaque)
{
    zd_hit_context *context = (zd_hit_context *)opaque;

    zd_hit_node(child, child_bounds, context);
}

static void zd_hit_node(const zd_ui_node *node, zd_rect bounds,
                        zd_hit_context *context)
{
    if (node == NULL || context->depth > ZD_UI_MAX_DEPTH ||
        bounds.width == 0U || bounds.height == 0U) {
        return;
    }

    if (node->kind == ZD_UI_BUTTON) {
        int64_t right = (int64_t)bounds.x + bounds.width;
        int64_t bottom = (int64_t)bounds.y + bounds.height;

        if ((int64_t)context->x >= bounds.x &&
            (int64_t)context->x < right &&
            (int64_t)context->y >= bounds.y &&
            (int64_t)context->y < bottom) {
            /* Later siblings paint later, so the last hit is topmost. */
            context->hit = node;
            context->bounds = bounds;
        }
        return;
    }

    if (node->kind == ZD_UI_PANEL || node->kind == ZD_UI_ROW ||
        node->kind == ZD_UI_COLUMN) {
        uint32_t old_depth = context->depth;
        ++context->depth;
        zd_layout_children(node, bounds, zd_hit_child, context);
        context->depth = old_depth;
    }
}

const zd_ui_node *zd_ui_hit_test(const zd_ui_node *root,
                                 zd_rect viewport,
                                 int32_t x, int32_t y,
                                 zd_rect *out_bounds)
{
    zd_hit_context context;

    if (root == NULL || viewport.width == 0U || viewport.height == 0U) {
        return NULL;
    }

    context.x = x;
    context.y = y;
    context.hit = NULL;
    context.bounds.x = 0;
    context.bounds.y = 0;
    context.bounds.width = 0U;
    context.bounds.height = 0U;
    context.depth = 0U;

    zd_hit_node(root, viewport, &context);
    if (context.hit != NULL && out_bounds != NULL) {
        *out_bounds = context.bounds;
    }
    return context.hit;
}
