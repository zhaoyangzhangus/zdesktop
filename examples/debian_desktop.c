#include "zdesktop.h"
#include "zdesktop_debian.h"

#include <stdio.h>

#define WINDOW_WIDTH 1024U
#define WINDOW_HEIGHT 640U

typedef struct {
    zd_ui_state ui_state;
    char status_text[128];
    uint32_t click_count;
    uint32_t width;
    uint32_t height;
} demo_state;

static demo_state state;

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
    .text = "DEBIAN WINDOW / KEYBOARD / POINTER",
    .height = 18U,
    .foreground = 0xFFB8C4D6U
};

static const zd_ui_node launch_button = {
    .kind = ZD_UI_BUTTON,
    .id = "launch",
    .text = "CLICK ME",
    .width = 180U,
    .height = 48U,
    .background = 0xFF2563EBU,
    .foreground = 0xFFFFFFFFU,
    .hover_background = 0xFF3B82F6U,
    .pressed_background = 0xFF1D4ED8U,
    .focus_border_color = 0xFFFDE68AU,
    .padding = 12U
};

static const zd_ui_node exit_button = {
    .kind = ZD_UI_BUTTON,
    .id = "exit",
    .text = "EXIT",
    .width = 140U,
    .height = 48U,
    .background = 0xFF475569U,
    .foreground = 0xFFFFFFFFU,
    .hover_background = 0xFF64748BU,
    .pressed_background = 0xFF334155U,
    .focus_border_color = 0xFFFDE68AU,
    .padding = 12U
};

static const zd_ui_node *const action_children[] = {
    &launch_button,
    &exit_button
};

static const zd_ui_node action_row = {
    .kind = ZD_UI_ROW,
    .id = "actions",
    .height = 48U,
    .gap = 16U,
    .children = action_children,
    .child_count = 2U
};

static zd_ui_node status_node = {
    .kind = ZD_UI_TEXT,
    .id = "status",
    .text = state.status_text,
    .height = 18U,
    .foreground = 0xFF93C5FDU
};

static const zd_ui_node *const card_children[] = {
    &title,
    &subtitle,
    &action_row,
    &status_node
};

static const zd_ui_node card = {
    .kind = ZD_UI_PANEL,
    .id = "welcome-card",
    .height = 260U,
    .background = 0xFF1E293BU,
    .border_color = 0xFF334155U,
    .border_width = 1U,
    .padding = 28U,
    .gap = 18U,
    .children = card_children,
    .child_count = 4U
};

static const zd_ui_node footer = {
    .kind = ZD_UI_TEXT,
    .id = "footer",
    .text = "TAB TO CHANGE FOCUS  |  ENTER / SPACE TO ACTIVATE  |  ESC TO QUIT",
    .height = 18U,
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
    .gap = 28U,
    .children = root_children,
    .child_count = 2U
};

static zd_rect demo_viewport(void)
{
    zd_rect viewport;

    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (state.width != 0U) ? state.width : WINDOW_WIDTH;
    viewport.height = (state.height != 0U) ? state.height : WINDOW_HEIGHT;
    return viewport;
}

static void set_status(const char *message)
{
    (void)snprintf(state.status_text, sizeof(state.status_text), "%s",
                   message);
}

static int activate_button(const zd_ui_node *button)
{
    if (button == &exit_button) {
        return 1;
    }
    if (button == &launch_button) {
        char message[128];

        ++state.click_count;
        (void)snprintf(message, sizeof(message),
                       "Button activated. Click count: %u",
                       state.click_count);
        set_status(message);
    }
    return 0;
}

static void render(zd_surface *surface, void *user_data)
{
    zd_rect viewport;
    (void)user_data;

    state.width = surface->width;
    state.height = surface->height;
    viewport = demo_viewport();
    zd_surface_clear(surface, 0xFF0F172AU);
    (void)zd_ui_render_with_state(surface, &root, viewport,
                                  &state.ui_state);
}

static int handle_event(const zd_debian_event *event, void *user_data)
{
    zd_rect viewport = demo_viewport();
    const zd_ui_node *target;
    (void)user_data;

    switch (event->type) {
    case ZD_DEBIAN_EVENT_QUIT:
        return 1;

    case ZD_DEBIAN_EVENT_POINTER_MOVE:
        state.ui_state.hovered = zd_ui_hit_test(
            &root, viewport, event->x, event->y, NULL);
        break;

    case ZD_DEBIAN_EVENT_POINTER_DOWN:
        if (event->button == ZD_MOUSE_LEFT) {
            target = zd_ui_hit_test(
                &root, viewport, event->x, event->y, NULL);
            state.ui_state.pressed = target;
            if (target != NULL) state.ui_state.focused = target;
        }
        break;

    case ZD_DEBIAN_EVENT_POINTER_UP:
        if (event->button == ZD_MOUSE_LEFT) {
            target = zd_ui_hit_test(
                &root, viewport, event->x, event->y, NULL);
            if (target != NULL && target == state.ui_state.pressed) {
                if (activate_button(target) != 0) return 1;
            }
            state.ui_state.pressed = NULL;
            state.ui_state.hovered = target;
        }
        break;

    case ZD_DEBIAN_EVENT_KEY_DOWN:
        if (event->repeat != 0U) break;
        if (event->key == ZD_KEY_ESCAPE) return 1;
        if (event->key == ZD_KEY_TAB) {
            state.ui_state.focused =
                (state.ui_state.focused == &launch_button)
                ? &exit_button : &launch_button;
        } else if (event->key == ZD_KEY_ENTER ||
                   event->key == ZD_KEY_SPACE) {
            if (activate_button(state.ui_state.focused) != 0) return 1;
        }
        break;

    case ZD_DEBIAN_EVENT_TEXT_INPUT:
        (void)snprintf(state.status_text, sizeof(state.status_text),
                       "Text input: %.96s", event->text);
        break;

    case ZD_DEBIAN_EVENT_FOCUS_LOST:
        state.ui_state.pressed = NULL;
        break;

    case ZD_DEBIAN_EVENT_WINDOW_RESIZED:
        state.width = event->width;
        state.height = event->height;
        (void)snprintf(state.status_text, sizeof(state.status_text),
                       "Window resized: %u x %u",
                       event->width, event->height);
        break;

    case ZD_DEBIAN_EVENT_POINTER_WHEEL:
        (void)snprintf(state.status_text, sizeof(state.status_text),
                       "Mouse wheel: %d, %d",
                       event->wheel_x, event->wheel_y);
        break;

    default:
        break;
    }

    return 0;
}

int main(void)
{
    zd_debian_app app;

    (void)snprintf(state.status_text, sizeof(state.status_text),
                   "Ready. Move the pointer or press TAB.");
    state.ui_state.hovered = NULL;
    state.ui_state.pressed = NULL;
    state.ui_state.focused = &launch_button;
    state.click_count = 0U;
    state.width = WINDOW_WIDTH;
    state.height = WINDOW_HEIGHT;

    app.title = "zdesktop - Debian desktop backend";
    app.width = WINDOW_WIDTH;
    app.height = WINDOW_HEIGHT;
    app.resizable = 1U;
    app.vsync = 1U;
    app.render = render;
    app.event = handle_event;
    app.user_data = &state;

    if (zd_debian_run(&app) != ZD_OK) {
        (void)fprintf(stderr, "zdesktop: %s\n", zd_debian_last_error());
        return 1;
    }
    return 0;
}
