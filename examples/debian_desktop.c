#include "zdesktop.h"
#include "zdesktop_debian.h"

#include <stdio.h>
#include <stdlib.h>

#define WINDOW_WIDTH 1024U
#define WINDOW_HEIGHT 640U
#define FONT_SCRATCH_SIZE (512U * 1024U)
#define DEFAULT_FONT_PATH "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc"

typedef struct {
    zd_ui_state ui_state;
    char status_text[128];
    uint32_t click_count;
    uint32_t width;
    uint32_t height;
} demo_state;

static demo_state state;
static zd_font ui_font;
static uint8_t font_scratch[FONT_SCRATCH_SIZE];
static uint8_t *font_data;

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
    .text = "TrueType 字体渲染 / Debian 显示与输入",
    .height = 28U,
    .font_size = 20U,
    .foreground = 0xFFB8C4D6U
};

static const zd_ui_node launch_button = {
    .kind = ZD_UI_BUTTON,
    .id = "launch",
    .text = "点击测试",
    .width = 180U,
    .height = 52U,
    .font_size = 18U,
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
    .text = "退出",
    .width = 140U,
    .height = 52U,
    .font_size = 18U,
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
    .height = 52U,
    .gap = 16U,
    .children = action_children,
    .child_count = 2U
};

static zd_ui_node status_node = {
    .kind = ZD_UI_TEXT,
    .id = "status",
    .text = state.status_text,
    .height = 24U,
    .font_size = 16U,
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
    .height = 280U,
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
    .text = "Tab 切换焦点  |  Enter / Space 激活  |  Esc 退出",
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
    .gap = 28U,
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
    if (button == &exit_button) return 1;
    if (button == &launch_button) {
        char message[128];

        ++state.click_count;
        (void)snprintf(message, sizeof(message),
                       "按钮已激活，点击次数：%u", state.click_count);
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
    (void)zd_ui_render_with_state_and_font(
        surface, &root, viewport, &state.ui_state, &ui_font);
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
                       "文本输入：%.96s", event->text);
        break;

    case ZD_DEBIAN_EVENT_FOCUS_LOST:
        state.ui_state.pressed = NULL;
        state.ui_state.hovered = NULL;
        break;

    case ZD_DEBIAN_EVENT_WINDOW_RESIZED:
        state.width = event->width;
        state.height = event->height;
        (void)snprintf(state.status_text, sizeof(state.status_text),
                       "窗口尺寸：%u x %u",
                       event->width, event->height);
        break;

    case ZD_DEBIAN_EVENT_POINTER_WHEEL:
        (void)snprintf(state.status_text, sizeof(state.status_text),
                       "鼠标滚轮：%d, %d",
                       event->wheel_x, event->wheel_y);
        break;

    default:
        break;
    }

    return 0;
}

int main(int argc, char **argv)
{
    const char *font_path = (argc > 1) ? argv[1] : DEFAULT_FONT_PATH;
    uint32_t face_index = (argc > 2)
                        ? (uint32_t)strtoul(argv[2], NULL, 10) : 0U;
    size_t font_size = 0U;
    zd_debian_app app;
    zd_result status;

    font_data = load_file(font_path, &font_size);
    if (font_data == NULL) {
        (void)fprintf(stderr, "cannot read TTF/TTC font: %s\n", font_path);
        (void)fputs("Install fonts-wqy-microhei or pass a font path.\n", stderr);
        return 1;
    }

    status = zd_font_init(&ui_font, font_data, font_size, face_index,
                          20U, font_scratch, sizeof(font_scratch));
    if (status != ZD_OK) {
        (void)fprintf(stderr, "invalid/unsupported TrueType font: %s\n",
                      font_path);
        free(font_data);
        return 1;
    }

    (void)snprintf(state.status_text, sizeof(state.status_text),
                   "就绪：移动鼠标或按 Tab。");
    state.ui_state.hovered = NULL;
    state.ui_state.pressed = NULL;
    state.ui_state.focused = &launch_button;
    state.click_count = 0U;
    state.width = WINDOW_WIDTH;
    state.height = WINDOW_HEIGHT;

    app.title = "zdesktop - TrueType desktop";
    app.width = WINDOW_WIDTH;
    app.height = WINDOW_HEIGHT;
    app.resizable = 1U;
    app.vsync = 1U;
    app.render = render;
    app.event = handle_event;
    app.user_data = &state;

    status = zd_debian_run(&app);
    free(font_data);
    font_data = NULL;

    if (status != ZD_OK) {
        (void)fprintf(stderr, "zdesktop: %s\n", zd_debian_last_error());
        return 1;
    }
    return 0;
}
