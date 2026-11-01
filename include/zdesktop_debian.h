#ifndef ZDESKTOP_DEBIAN_H
#define ZDESKTOP_DEBIAN_H

#include "zdesktop.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZD_DEBIAN_TEXT_MAX 32U

/* Keyboard key values are USB HID Keyboard/Keypad Usage IDs. */
#define ZD_KEY_ENTER 0x28U
#define ZD_KEY_ESCAPE 0x29U
#define ZD_KEY_BACKSPACE 0x2AU
#define ZD_KEY_TAB 0x2BU
#define ZD_KEY_SPACE 0x2CU
#define ZD_KEY_F1 0x3AU
#define ZD_KEY_HOME 0x4AU
#define ZD_KEY_PAGE_UP 0x4BU
#define ZD_KEY_DELETE 0x4CU
#define ZD_KEY_END 0x4DU
#define ZD_KEY_PAGE_DOWN 0x4EU
#define ZD_KEY_RIGHT 0x4FU
#define ZD_KEY_LEFT 0x50U
#define ZD_KEY_DOWN 0x51U
#define ZD_KEY_UP 0x52U

enum {
    ZD_MOD_SHIFT = 1U << 0,
    ZD_MOD_CTRL  = 1U << 1,
    ZD_MOD_ALT   = 1U << 2,
    ZD_MOD_GUI   = 1U << 3,
    ZD_MOD_CAPS  = 1U << 4,
    ZD_MOD_NUM   = 1U << 5
};

enum {
    ZD_MOUSE_LEFT   = 1U,
    ZD_MOUSE_MIDDLE = 2U,
    ZD_MOUSE_RIGHT  = 3U,
    ZD_MOUSE_X1     = 4U,
    ZD_MOUSE_X2     = 5U
};

enum {
    ZD_MOUSE_MASK_LEFT   = 1U << 0,
    ZD_MOUSE_MASK_MIDDLE = 1U << 1,
    ZD_MOUSE_MASK_RIGHT  = 1U << 2,
    ZD_MOUSE_MASK_X1     = 1U << 3,
    ZD_MOUSE_MASK_X2     = 1U << 4
};

typedef enum {
    ZD_DEBIAN_EVENT_QUIT = 1,
    ZD_DEBIAN_EVENT_KEY_DOWN,
    ZD_DEBIAN_EVENT_KEY_UP,
    ZD_DEBIAN_EVENT_TEXT_INPUT,
    ZD_DEBIAN_EVENT_POINTER_MOVE,
    ZD_DEBIAN_EVENT_POINTER_DOWN,
    ZD_DEBIAN_EVENT_POINTER_UP,
    ZD_DEBIAN_EVENT_POINTER_WHEEL,
    ZD_DEBIAN_EVENT_WINDOW_RESIZED,
    ZD_DEBIAN_EVENT_FOCUS_GAINED,
    ZD_DEBIAN_EVENT_FOCUS_LOST
} zd_debian_event_type;

typedef struct {
    zd_debian_event_type type;
    int32_t x;
    int32_t y;
    int32_t delta_x;
    int32_t delta_y;
    int32_t wheel_x;
    int32_t wheel_y;
    uint32_t buttons;
    uint32_t key;
    uint32_t modifiers;
    uint32_t width;
    uint32_t height;
    uint8_t button;
    uint8_t clicks;
    uint8_t repeat;
    char text[ZD_DEBIAN_TEXT_MAX];
} zd_debian_event;

typedef void (*zd_debian_render_callback)(zd_surface *surface,
                                         void *user_data);

/*
 * Return non-zero from the event callback to close the window.
 * A QUIT event closes the window regardless of the callback return value.
 */
typedef int (*zd_debian_event_callback)(const zd_debian_event *event,
                                        void *user_data);

typedef struct {
    const char *title;
    uint32_t width;
    uint32_t height;
    uint8_t resizable;
    uint8_t vsync;
    zd_debian_render_callback render;
    zd_debian_event_callback event;
    void *user_data;
} zd_debian_app;

/*
 * Runs the SDL2 window/event loop. The supplied surface is RGBX8888 and is
 * owned by the backend; callbacks must not retain its pixel pointer.
 */
zd_result zd_debian_run(const zd_debian_app *app);

/* Human-readable SDL/backend error from the most recent failed call. */
const char *zd_debian_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
