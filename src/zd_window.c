#include "zdesktop.h"

#include <limits.h>

#define ZD_WINDOW_DEFAULT_TITLE_HEIGHT 34U
#define ZD_WINDOW_DEFAULT_BORDER 1U
#define ZD_WINDOW_DEFAULT_CONTROL_WIDTH 42U
#define ZD_WINDOW_DRAG_VISIBLE 64U

static int32_t zd_window_i64_to_i32(int64_t value)
{
    if (value < INT32_MIN) return INT32_MIN;
    if (value > INT32_MAX) return INT32_MAX;
    return (int32_t)value;
}

static uint32_t zd_window_title_height(const zd_ui_window_desc *desc)
{
    return (desc->title_bar_height != 0U)
         ? desc->title_bar_height : ZD_WINDOW_DEFAULT_TITLE_HEIGHT;
}

static uint32_t zd_window_border_width(const zd_ui_window_desc *desc)
{
    return (desc->border_width != 0U)
         ? desc->border_width : ZD_WINDOW_DEFAULT_BORDER;
}

static uint32_t zd_window_control_width(const zd_ui_window_desc *desc)
{
    return (desc->control_width != 0U)
         ? desc->control_width : ZD_WINDOW_DEFAULT_CONTROL_WIDTH;
}

static int zd_window_in_title_bar(const zd_ui_window_desc *desc,
                                  const zd_ui_window_state *state,
                                  int32_t x, int32_t y)
{
    uint32_t border = zd_window_border_width(desc);
    uint32_t title_height = zd_window_title_height(desc);
    int64_t top = (int64_t)state->bounds.y + border;
    int64_t bottom = top + title_height;
    int64_t left = (int64_t)state->bounds.x + border;
    int64_t right = (int64_t)state->bounds.x +
                    state->bounds.width - border;

    return (int64_t)x >= left && (int64_t)x < right &&
           (int64_t)y >= top && (int64_t)y < bottom;
}

static zd_ui_window_control zd_window_control_at(
    const zd_ui_window_desc *desc,
    const zd_ui_window_state *state,
    int32_t x, int32_t y)
{
    uint32_t border = zd_window_border_width(desc);
    uint32_t title_height = zd_window_title_height(desc);
    uint32_t control_width = zd_window_control_width(desc);
    int64_t right = (int64_t)state->bounds.x +
                    state->bounds.width - border;
    int64_t top = (int64_t)state->bounds.y + border;
    int64_t bottom = top + title_height;
    int64_t close_left = right - control_width;
    int64_t maximize_left = close_left - control_width;
    int64_t minimize_left = maximize_left - control_width;

    if ((int64_t)y < top || (int64_t)y >= bottom) {
        return ZD_UI_WINDOW_CONTROL_NONE;
    }
    if ((int64_t)x >= close_left && (int64_t)x < right) {
        return ZD_UI_WINDOW_CONTROL_CLOSE;
    }
    if ((int64_t)x >= maximize_left && (int64_t)x < close_left) {
        return ZD_UI_WINDOW_CONTROL_MAXIMIZE;
    }
    if ((int64_t)x >= minimize_left && (int64_t)x < maximize_left) {
        return ZD_UI_WINDOW_CONTROL_MINIMIZE;
    }
    return ZD_UI_WINDOW_CONTROL_NONE;
}

zd_result zd_ui_window_state_init(zd_ui_window_state *state,
                                  zd_rect initial_bounds)
{
    if (state == NULL || initial_bounds.width == 0U ||
        initial_bounds.height == 0U) {
        return ZD_ERR_ARGUMENT;
    }

    *state = (zd_ui_window_state){0};
    state->bounds = initial_bounds;
    state->visible = 1U;
    state->active = 1U;
    return ZD_OK;
}

zd_result zd_ui_window_get_client_rect(const zd_ui_window_desc *desc,
                                       const zd_ui_window_state *state,
                                       zd_rect *out_client)
{
    uint32_t border;
    uint32_t title_height;
    uint64_t horizontal_inset;
    uint64_t vertical_inset;

    if (desc == NULL || state == NULL || out_client == NULL ||
        state->bounds.width == 0U || state->bounds.height == 0U) {
        return ZD_ERR_ARGUMENT;
    }

    border = zd_window_border_width(desc);
    title_height = zd_window_title_height(desc);
    horizontal_inset = (uint64_t)border * 2U;
    vertical_inset = (uint64_t)border * 2U + title_height;

    out_client->x = zd_window_i64_to_i32(
        (int64_t)state->bounds.x + border);
    out_client->y = zd_window_i64_to_i32(
        (int64_t)state->bounds.y + border + title_height);
    out_client->width = (horizontal_inset >= state->bounds.width)
        ? 0U : state->bounds.width - (uint32_t)horizontal_inset;
    out_client->height = (state->minimized != 0U ||
                          vertical_inset >= state->bounds.height)
        ? 0U : state->bounds.height - (uint32_t)vertical_inset;
    return ZD_OK;
}

static void zd_window_draw_line(zd_surface *surface,
                                int32_t x0, int32_t y0,
                                int32_t x1, int32_t y1,
                                uint32_t color)
{
    int64_t dx = (x1 >= x0) ? (int64_t)x1 - x0 : (int64_t)x0 - x1;
    int64_t dy_abs = (y1 >= y0) ? (int64_t)y1 - y0 : (int64_t)y0 - y1;
    int64_t dy = -dy_abs;
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t sy = (y0 < y1) ? 1 : -1;
    int64_t error = dx + dy;

    for (;;) {
        zd_rect pixel = {x0, y0, 1U, 1U};
        int64_t twice;

        zd_fill_rect(surface, pixel, color);
        if (x0 == x1 && y0 == y1) break;
        twice = error * 2;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

static void zd_window_draw_control(zd_surface *surface,
                                   zd_rect button,
                                   zd_ui_window_control control,
                                   uint32_t color)
{
    int32_t center_x = zd_window_i64_to_i32(
        (int64_t)button.x + button.width / 2U);
    int32_t center_y = zd_window_i64_to_i32(
        (int64_t)button.y + button.height / 2U);

    if (control == ZD_UI_WINDOW_CONTROL_MINIMIZE) {
        zd_rect glyph = {center_x - 7, center_y + 5, 14U, 2U};
        zd_fill_rect(surface, glyph, color);
    } else if (control == ZD_UI_WINDOW_CONTROL_MAXIMIZE) {
        zd_rect glyph = {center_x - 6, center_y - 6, 12U, 12U};
        zd_stroke_rect(surface, glyph, 1U, color);
    } else if (control == ZD_UI_WINDOW_CONTROL_CLOSE) {
        zd_window_draw_line(surface, center_x - 5, center_y - 5,
                            center_x + 5, center_y + 5, color);
        zd_window_draw_line(surface, center_x + 5, center_y - 5,
                            center_x - 5, center_y + 5, color);
    }
}

zd_result zd_ui_window_render(zd_surface *surface,
                              const zd_ui_window_desc *desc,
                              const zd_ui_window_state *state,
                              const zd_ui_state *content_state,
                              zd_font *font)
{
    zd_rect shadow;
    zd_rect body;
    zd_rect title_bar;
    zd_rect client;
    uint32_t border;
    uint32_t title_height;
    uint32_t control_width;
    uint32_t title_color;
    uint32_t title_background;
    int64_t right;
    zd_result status;

    status = zd_surface_validate(surface);
    if (status != ZD_OK) return status;
    if (desc == NULL || state == NULL) return ZD_ERR_ARGUMENT;
    if (state->visible == 0U) return ZD_OK;
    if (state->bounds.width == 0U || state->bounds.height == 0U) {
        return ZD_ERR_ARGUMENT;
    }

    border = zd_window_border_width(desc);
    title_height = zd_window_title_height(desc);
    control_width = zd_window_control_width(desc);
    title_color = (desc->title_color != 0U)
                ? desc->title_color : 0xFFF8FAFCU;
    title_background = (state->active != 0U)
        ? ((desc->title_bar_background != 0U)
           ? desc->title_bar_background : 0xFF1E40AFU)
        : ((desc->inactive_title_bar_background != 0U)
           ? desc->inactive_title_bar_background : 0xFF334155U);

    shadow = state->bounds;
    shadow.x = zd_window_i64_to_i32((int64_t)shadow.x + 4);
    shadow.y = zd_window_i64_to_i32((int64_t)shadow.y + 5);
    zd_fill_rect(surface, shadow, 0x30000000U);

    body = state->bounds;
    zd_fill_rect(surface, body,
        (desc->background != 0U) ? desc->background : 0xFF0F172AU);

    title_bar.x = zd_window_i64_to_i32(
        (int64_t)state->bounds.x + border);
    title_bar.y = zd_window_i64_to_i32(
        (int64_t)state->bounds.y + border);
    title_bar.width = (state->bounds.width > border * 2U)
        ? state->bounds.width - border * 2U : 0U;
    title_bar.height = title_height;
    zd_fill_rect(surface, title_bar, title_background);

    zd_stroke_rect(surface, state->bounds, border,
        (desc->border_color != 0U)
        ? desc->border_color : 0xFF64748BU);

    right = (int64_t)state->bounds.x +
            state->bounds.width - border;
    {
        zd_ui_window_control control;
        for (control = ZD_UI_WINDOW_CONTROL_MINIMIZE;
             control <= ZD_UI_WINDOW_CONTROL_CLOSE;
             control = (zd_ui_window_control)((int)control + 1)) {
            zd_rect button;
            int64_t button_right = right -
                (int64_t)(ZD_UI_WINDOW_CONTROL_CLOSE - control) *
                control_width;
            uint32_t button_background = 0U;
            uint32_t glyph_color =
                (desc->control_color != 0U)
                ? desc->control_color : 0xFFE2E8F0U;

            button.x = zd_window_i64_to_i32(
                button_right - control_width);
            button.y = title_bar.y;
            button.width = control_width;
            button.height = title_height;

            if (state->pressed_control == control) {
                button_background =
                    (desc->control_pressed_background != 0U)
                    ? desc->control_pressed_background : 0xFF1D4ED8U;
            } else if (state->hovered_control == control) {
                button_background =
                    (desc->control_hover_background != 0U)
                    ? desc->control_hover_background : 0xFF475569U;
                if (control == ZD_UI_WINDOW_CONTROL_CLOSE &&
                    desc->control_hover_background == 0U) {
                    button_background = 0xFFDC2626U;
                }
            }
            if ((button_background >> 24) != 0U) {
                zd_fill_rect(surface, button, button_background);
            }
            zd_window_draw_control(surface, button, control, glyph_color);
        }
    }

    if (font != NULL && desc->title != NULL && title_height > 0U) {
        zd_font local_font;
        zd_font *draw_font = font;
        zd_font_metrics metrics;
        uint32_t title_size = desc->title_font_size;
        uint32_t text_width;
        int64_t baseline;
        int32_t drawn_width;

        if (title_size != 0U && title_size != font->pixel_height) {
            local_font = *font;
            status = zd_font_set_pixel_height(&local_font, title_size);
            if (status != ZD_OK) return status;
            draw_font = &local_font;
        }
        status = zd_font_get_metrics(draw_font, &metrics);
        if (status != ZD_OK) return status;

        text_width = (state->bounds.width >
                      border * 2U + control_width * 3U + 16U)
            ? state->bounds.width - border * 2U -
              control_width * 3U - 16U : 0U;
        baseline = (int64_t)title_bar.y +
                   (title_height > metrics.line_height
                    ? (title_height - metrics.line_height) / 2U : 0U) +
                   metrics.ascent;
        if (text_width != 0U) {
            status = zd_draw_text(surface, draw_font,
                zd_window_i64_to_i32((int64_t)title_bar.x + 12),
                zd_window_i64_to_i32(baseline),
                desc->title, title_color, text_width, &drawn_width);
            if (status != ZD_OK) return status;
        }
    }

    if (state->minimized != 0U || desc->content == NULL) {
        return ZD_OK;
    }
    status = zd_ui_window_get_client_rect(desc, state, &client);
    if (status != ZD_OK) return status;
    if (client.width == 0U || client.height == 0U) return ZD_OK;

    return zd_ui_render_with_state_and_font(
        surface, desc->content, client, content_state, font);
}

static int32_t zd_window_clamp_drag_axis(int64_t value,
                                         int64_t minimum,
                                         int64_t maximum)
{
    if (maximum < minimum) maximum = minimum;
    if (value < minimum) value = minimum;
    if (value > maximum) value = maximum;
    return zd_window_i64_to_i32(value);
}

static void zd_window_start_drag(const zd_ui_window_desc *desc,
                                 zd_ui_window_state *state,
                                 int32_t x, int32_t y)
{
    uint32_t title_height = zd_window_title_height(desc);
    uint32_t drag_visible = state->bounds.width < ZD_WINDOW_DRAG_VISIBLE
        ? state->bounds.width : ZD_WINDOW_DRAG_VISIBLE;
    int64_t offset_x = (int64_t)x - state->bounds.x;
    int64_t offset_y = (int64_t)y - state->bounds.y;

    if (offset_x > (int64_t)state->bounds.width - drag_visible) {
        offset_x = (int64_t)state->bounds.width - drag_visible;
    }
    if (offset_x < 0) offset_x = 0;
    if (offset_y >= title_height) offset_y = title_height - 1U;
    if (offset_y < 0) offset_y = 0;

    state->drag_offset_x = zd_window_i64_to_i32(offset_x);
    state->drag_offset_y = zd_window_i64_to_i32(offset_y);
    state->dragging = 1U;
}

zd_ui_window_action zd_ui_window_handle_pointer(
    const zd_ui_window_desc *desc,
    zd_ui_window_state *state,
    zd_rect work_area,
    zd_ui_pointer_event event,
    int32_t x, int32_t y,
    uint8_t primary_button)
{
    zd_ui_window_control control;

    if (desc == NULL || state == NULL || state->visible == 0U) {
        return ZD_UI_WINDOW_ACTION_NONE;
    }

    if (event == ZD_UI_POINTER_MOVE) {
        if (state->dragging != 0U) {
            int64_t min_x;
            int64_t max_x;
            int64_t min_y;
            int64_t max_y;
            uint32_t visible_width;
            uint32_t title_height;
            int32_t new_x;
            int32_t new_y;

            if (primary_button == 0U) {
                state->dragging = 0U;
                return ZD_UI_WINDOW_ACTION_NONE;
            }
            if (work_area.width == 0U || work_area.height == 0U) {
                return ZD_UI_WINDOW_ACTION_NONE;
            }

            visible_width = state->bounds.width < ZD_WINDOW_DRAG_VISIBLE
                ? state->bounds.width : ZD_WINDOW_DRAG_VISIBLE;
            title_height = zd_window_title_height(desc);
            min_x = (int64_t)work_area.x + visible_width -
                    state->bounds.width;
            max_x = (int64_t)work_area.x + work_area.width -
                    visible_width;
            min_y = work_area.y;
            max_y = (int64_t)work_area.y + work_area.height -
                    ((title_height < work_area.height)
                     ? title_height : work_area.height);
            new_x = zd_window_clamp_drag_axis(
                (int64_t)x - state->drag_offset_x, min_x, max_x);
            new_y = zd_window_clamp_drag_axis(
                (int64_t)y - state->drag_offset_y, min_y, max_y);
            if (new_x != state->bounds.x || new_y != state->bounds.y) {
                state->bounds.x = new_x;
                state->bounds.y = new_y;
                return ZD_UI_WINDOW_ACTION_MOVED;
            }
            return ZD_UI_WINDOW_ACTION_NONE;
        }
        state->hovered_control =
            zd_window_control_at(desc, state, x, y);
        return ZD_UI_WINDOW_ACTION_NONE;
    }

    if (event == ZD_UI_POINTER_DOWN) {
        if (primary_button == 0U) return ZD_UI_WINDOW_ACTION_NONE;
        state->active = 1U;
        control = zd_window_control_at(desc, state, x, y);
        if (control != ZD_UI_WINDOW_CONTROL_NONE) {
            state->pressed_control = control;
            return ZD_UI_WINDOW_ACTION_NONE;
        }
        if (zd_window_in_title_bar(desc, state, x, y)) {
            if (state->minimized != 0U) {
                state->bounds = state->pre_minimize_bounds;
                state->minimized = 0U;
                return ZD_UI_WINDOW_ACTION_RESTORED;
            }
            if (state->maximized != 0U && state->has_restore_bounds != 0U) {
                state->bounds = state->restore_bounds;
                state->maximized = 0U;
                state->has_restore_bounds = 0U;
            }
            zd_window_start_drag(desc, state, x, y);
        }
        return ZD_UI_WINDOW_ACTION_NONE;
    }

    if (event == ZD_UI_POINTER_UP) {
        if (primary_button == 0U) return ZD_UI_WINDOW_ACTION_NONE;
        state->dragging = 0U;
        control = state->pressed_control;
        state->pressed_control = ZD_UI_WINDOW_CONTROL_NONE;
        state->hovered_control =
            zd_window_control_at(desc, state, x, y);
        if (control == ZD_UI_WINDOW_CONTROL_NONE ||
            control != state->hovered_control) {
            return ZD_UI_WINDOW_ACTION_NONE;
        }

        if (control == ZD_UI_WINDOW_CONTROL_CLOSE) {
            state->visible = 0U;
            return ZD_UI_WINDOW_ACTION_CLOSED;
        }
        if (control == ZD_UI_WINDOW_CONTROL_MINIMIZE) {
            if (state->minimized != 0U) {
                state->bounds = state->pre_minimize_bounds;
                state->minimized = 0U;
                return ZD_UI_WINDOW_ACTION_RESTORED;
            }
            state->pre_minimize_bounds = state->bounds;
            state->minimized = 1U;
            state->bounds.height =
                zd_window_title_height(desc) +
                zd_window_border_width(desc) * 2U;
            return ZD_UI_WINDOW_ACTION_MINIMIZED;
        }
        if (control == ZD_UI_WINDOW_CONTROL_MAXIMIZE) {
            if (state->minimized != 0U) {
                state->bounds = state->pre_minimize_bounds;
                state->minimized = 0U;
                return ZD_UI_WINDOW_ACTION_RESTORED;
            }
            if (state->maximized != 0U &&
                state->has_restore_bounds != 0U) {
                state->bounds = state->restore_bounds;
                state->maximized = 0U;
                state->has_restore_bounds = 0U;
                return ZD_UI_WINDOW_ACTION_RESTORED;
            }
            if (work_area.width != 0U && work_area.height != 0U) {
                state->restore_bounds = state->bounds;
                state->has_restore_bounds = 1U;
                state->bounds = work_area;
                state->maximized = 1U;
                state->minimized = 0U;
                return ZD_UI_WINDOW_ACTION_MAXIMIZED;
            }
        }
    }
    return ZD_UI_WINDOW_ACTION_NONE;
}
