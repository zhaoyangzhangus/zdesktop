#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "zdesktop_debian.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char zd_debian_error_buffer[256];

static zd_result zd_debian_fail(const char *operation)
{
    (void)snprintf(zd_debian_error_buffer, sizeof(zd_debian_error_buffer),
                   "%s: %s", operation, SDL_GetError());
    return ZD_ERR_BACKEND;
}

const char *zd_debian_last_error(void)
{
    return zd_debian_error_buffer;
}

static uint32_t zd_map_modifiers(SDL_Keymod modifiers)
{
    uint32_t result = 0U;

    if ((modifiers & KMOD_SHIFT) != 0) result |= ZD_MOD_SHIFT;
    if ((modifiers & KMOD_CTRL) != 0) result |= ZD_MOD_CTRL;
    if ((modifiers & KMOD_ALT) != 0) result |= ZD_MOD_ALT;
    if ((modifiers & KMOD_GUI) != 0) result |= ZD_MOD_GUI;
    if ((modifiers & KMOD_CAPS) != 0) result |= ZD_MOD_CAPS;
    if ((modifiers & KMOD_NUM) != 0) result |= ZD_MOD_NUM;
    return result;
}

static uint32_t zd_map_key(SDL_Scancode scancode)
{
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        return 0x04U + (uint32_t)(scancode - SDL_SCANCODE_A);
    }
    if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
        return 0x1EU + (uint32_t)(scancode - SDL_SCANCODE_1);
    }
    if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12) {
        return 0x3AU + (uint32_t)(scancode - SDL_SCANCODE_F1);
    }

    switch (scancode) {
    case SDL_SCANCODE_0: return 0x27U;
    case SDL_SCANCODE_RETURN: return 0x28U;
    case SDL_SCANCODE_ESCAPE: return 0x29U;
    case SDL_SCANCODE_BACKSPACE: return 0x2AU;
    case SDL_SCANCODE_TAB: return 0x2BU;
    case SDL_SCANCODE_SPACE: return 0x2CU;
    case SDL_SCANCODE_MINUS: return 0x2DU;
    case SDL_SCANCODE_EQUALS: return 0x2EU;
    case SDL_SCANCODE_LEFTBRACKET: return 0x2FU;
    case SDL_SCANCODE_RIGHTBRACKET: return 0x30U;
    case SDL_SCANCODE_BACKSLASH: return 0x31U;
    case SDL_SCANCODE_SEMICOLON: return 0x33U;
    case SDL_SCANCODE_APOSTROPHE: return 0x34U;
    case SDL_SCANCODE_GRAVE: return 0x35U;
    case SDL_SCANCODE_COMMA: return 0x36U;
    case SDL_SCANCODE_PERIOD: return 0x37U;
    case SDL_SCANCODE_SLASH: return 0x38U;
    case SDL_SCANCODE_CAPSLOCK: return 0x39U;
    case SDL_SCANCODE_PRINTSCREEN: return 0x46U;
    case SDL_SCANCODE_SCROLLLOCK: return 0x47U;
    case SDL_SCANCODE_PAUSE: return 0x48U;
    case SDL_SCANCODE_INSERT: return 0x49U;
    case SDL_SCANCODE_HOME: return 0x4AU;
    case SDL_SCANCODE_PAGEUP: return 0x4BU;
    case SDL_SCANCODE_DELETE: return 0x4CU;
    case SDL_SCANCODE_END: return 0x4DU;
    case SDL_SCANCODE_PAGEDOWN: return 0x4EU;
    case SDL_SCANCODE_RIGHT: return 0x4FU;
    case SDL_SCANCODE_LEFT: return 0x50U;
    case SDL_SCANCODE_DOWN: return 0x51U;
    case SDL_SCANCODE_UP: return 0x52U;
    case SDL_SCANCODE_NUMLOCKCLEAR: return 0x53U;
    case SDL_SCANCODE_KP_DIVIDE: return 0x54U;
    case SDL_SCANCODE_KP_MULTIPLY: return 0x55U;
    case SDL_SCANCODE_KP_MINUS: return 0x56U;
    case SDL_SCANCODE_KP_PLUS: return 0x57U;
    case SDL_SCANCODE_KP_ENTER: return 0x58U;
    case SDL_SCANCODE_KP_1: return 0x59U;
    case SDL_SCANCODE_KP_2: return 0x5AU;
    case SDL_SCANCODE_KP_3: return 0x5BU;
    case SDL_SCANCODE_KP_4: return 0x5CU;
    case SDL_SCANCODE_KP_5: return 0x5DU;
    case SDL_SCANCODE_KP_6: return 0x5EU;
    case SDL_SCANCODE_KP_7: return 0x5FU;
    case SDL_SCANCODE_KP_8: return 0x60U;
    case SDL_SCANCODE_KP_9: return 0x61U;
    case SDL_SCANCODE_KP_0: return 0x62U;
    case SDL_SCANCODE_KP_PERIOD: return 0x63U;
    case SDL_SCANCODE_LCTRL: return 0xE0U;
    case SDL_SCANCODE_LSHIFT: return 0xE1U;
    case SDL_SCANCODE_LALT: return 0xE2U;
    case SDL_SCANCODE_LGUI: return 0xE3U;
    case SDL_SCANCODE_RCTRL: return 0xE4U;
    case SDL_SCANCODE_RSHIFT: return 0xE5U;
    case SDL_SCANCODE_RALT: return 0xE6U;
    case SDL_SCANCODE_RGUI: return 0xE7U;
    default: return 0U;
    }
}

static uint8_t zd_map_mouse_button(uint8_t button)
{
    switch (button) {
    case SDL_BUTTON_LEFT: return ZD_MOUSE_LEFT;
    case SDL_BUTTON_MIDDLE: return ZD_MOUSE_MIDDLE;
    case SDL_BUTTON_RIGHT: return ZD_MOUSE_RIGHT;
    case SDL_BUTTON_X1: return ZD_MOUSE_X1;
    case SDL_BUTTON_X2: return ZD_MOUSE_X2;
    default: return 0U;
    }
}

static uint32_t zd_map_mouse_mask(uint32_t mask)
{
    uint32_t result = 0U;

    if ((mask & SDL_BUTTON_LMASK) != 0U) result |= ZD_MOUSE_MASK_LEFT;
    if ((mask & SDL_BUTTON_MMASK) != 0U) result |= ZD_MOUSE_MASK_MIDDLE;
    if ((mask & SDL_BUTTON_RMASK) != 0U) result |= ZD_MOUSE_MASK_RIGHT;
    if ((mask & SDL_BUTTON_X1MASK) != 0U) result |= ZD_MOUSE_MASK_X1;
    if ((mask & SDL_BUTTON_X2MASK) != 0U) result |= ZD_MOUSE_MASK_X2;
    return result;
}

static uint32_t zd_button_mask(uint8_t button)
{
    switch (button) {
    case ZD_MOUSE_LEFT: return ZD_MOUSE_MASK_LEFT;
    case ZD_MOUSE_MIDDLE: return ZD_MOUSE_MASK_MIDDLE;
    case ZD_MOUSE_RIGHT: return ZD_MOUSE_MASK_RIGHT;
    case ZD_MOUSE_X1: return ZD_MOUSE_MASK_X1;
    case ZD_MOUSE_X2: return ZD_MOUSE_MASK_X2;
    default: return 0U;
    }
}

static int zd_debian_emit(const zd_debian_app *app,
                          const zd_debian_event *event,
                          int *running)
{
    int close_requested = 0;

    if (app->event != NULL) {
        close_requested = app->event(event, app->user_data);
    }
    if (event->type == ZD_DEBIAN_EVENT_QUIT || close_requested != 0) {
        *running = 0;
    }
    return close_requested;
}

static SDL_Texture *zd_create_texture(SDL_Renderer *renderer,
                                      uint32_t width, uint32_t height)
{
    SDL_Texture *texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
        (int)width, (int)height);

    if (texture != NULL) {
        (void)SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
    }
    return texture;
}

static zd_result zd_resize_surface(SDL_Renderer *renderer,
                                   SDL_Texture **texture,
                                   uint8_t **pixels,
                                   zd_surface *surface,
                                   uint32_t width, uint32_t height)
{
    uint64_t pitch64;
    uint64_t bytes64;
    uint8_t *new_pixels;
    SDL_Texture *new_texture;

    if (width == 0U || height == 0U) return ZD_OK;
    if (width > (uint32_t)(INT_MAX / 4) ||
        height > (uint32_t)INT_MAX) {
        return ZD_ERR_ARGUMENT;
    }

    pitch64 = (uint64_t)width * 4U;
    bytes64 = pitch64 * height;
    if (bytes64 > (uint64_t)((size_t)-1)) {
        return ZD_ERR_ARGUMENT;
    }

    new_pixels = (uint8_t *)malloc((size_t)bytes64);
    if (new_pixels == NULL) {
        (void)snprintf(zd_debian_error_buffer,
                       sizeof(zd_debian_error_buffer),
                       "cannot allocate %llu-byte framebuffer",
                       (unsigned long long)bytes64);
        return ZD_ERR_BACKEND;
    }
    (void)memset(new_pixels, 0, (size_t)bytes64);

    new_texture = zd_create_texture(renderer, width, height);
    if (new_texture == NULL) {
        free(new_pixels);
        return zd_debian_fail("SDL_CreateTexture");
    }

    if (*texture != NULL) SDL_DestroyTexture(*texture);
    free(*pixels);
    *texture = new_texture;
    *pixels = new_pixels;

    surface->pixels = new_pixels;
    surface->width = width;
    surface->height = height;
    surface->pitch_bytes = (uint32_t)pitch64;
    surface->format = ZD_PIXEL_RGBX8888;
    (void)SDL_RenderSetLogicalSize(renderer, (int)width, (int)height);
    return ZD_OK;
}

zd_result zd_debian_run(const zd_debian_app *app)
{
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    uint8_t *pixels = NULL;
    zd_surface surface;
    zd_result result = ZD_OK;
    int running = 1;
    int renderer_vsync = 0;
    uint32_t mouse_buttons = 0U;
    uint64_t pitch64;
    uint64_t bytes64;
    uint32_t y;

    zd_debian_error_buffer[0] = '\0';
    if (app == NULL || app->render == NULL || app->width == 0U ||
        app->height == 0U || app->width > (uint32_t)(INT_MAX / 4) ||
        app->height > (uint32_t)INT_MAX) {
        return ZD_ERR_ARGUMENT;
    }

    pitch64 = (uint64_t)app->width * 4U;
    bytes64 = pitch64 * app->height;
    if (bytes64 > (uint64_t)((size_t)-1)) {
        return ZD_ERR_ARGUMENT;
    }

    (void)SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        return zd_debian_fail("SDL_Init");
    }

    {
        uint32_t flags = SDL_WINDOW_SHOWN;
        if (app->resizable != 0U) flags |= SDL_WINDOW_RESIZABLE;
        window = SDL_CreateWindow(
            (app->title != NULL) ? app->title : "zdesktop",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            (int)app->width, (int)app->height, flags);
    }
    if (window == NULL) {
        result = zd_debian_fail("SDL_CreateWindow");
        goto cleanup;
    }

    if (app->resizable != 0U) {
        SDL_SetWindowMinimumSize(window, 320, 200);
    }

    {
        uint32_t flags = SDL_RENDERER_ACCELERATED;
        if (app->vsync != 0U) flags |= SDL_RENDERER_PRESENTVSYNC;
        renderer = SDL_CreateRenderer(window, -1, flags);
        if (renderer == NULL && app->vsync != 0U) {
            renderer = SDL_CreateRenderer(
                window, -1, SDL_RENDERER_ACCELERATED);
        }
        if (renderer == NULL) {
            renderer = SDL_CreateRenderer(
                window, -1, SDL_RENDERER_SOFTWARE);
        }
    }
    if (renderer == NULL) {
        result = zd_debian_fail("SDL_CreateRenderer");
        goto cleanup;
    }

    {
        SDL_RendererInfo info;
        if (SDL_GetRendererInfo(renderer, &info) == 0) {
            renderer_vsync =
                ((info.flags & SDL_RENDERER_PRESENTVSYNC) != 0U);
        }
    }

    pixels = (uint8_t *)malloc((size_t)bytes64);
    if (pixels == NULL) {
        (void)snprintf(zd_debian_error_buffer,
                       sizeof(zd_debian_error_buffer),
                       "cannot allocate %llu-byte framebuffer",
                       (unsigned long long)bytes64);
        result = ZD_ERR_BACKEND;
        goto cleanup;
    }
    (void)memset(pixels, 0, (size_t)bytes64);

    surface.pixels = pixels;
    surface.width = app->width;
    surface.height = app->height;
    surface.pitch_bytes = (uint32_t)pitch64;
    surface.format = ZD_PIXEL_RGBX8888;

    texture = zd_create_texture(renderer, app->width, app->height);
    if (texture == NULL) {
        result = zd_debian_fail("SDL_CreateTexture");
        goto cleanup;
    }
    (void)SDL_RenderSetLogicalSize(
        renderer, (int)app->width, (int)app->height);
    SDL_StartTextInput();

    while (running != 0) {
        SDL_Event source_event;

        while (SDL_PollEvent(&source_event) != 0) {
            zd_debian_event event;
            int emit = 0;

            (void)memset(&event, 0, sizeof(event));
            switch (source_event.type) {
            case SDL_QUIT:
                event.type = ZD_DEBIAN_EVENT_QUIT;
                emit = 1;
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                event.type = (source_event.type == SDL_KEYDOWN)
                           ? ZD_DEBIAN_EVENT_KEY_DOWN
                           : ZD_DEBIAN_EVENT_KEY_UP;
                event.key = zd_map_key(source_event.key.keysym.scancode);
                event.modifiers =
                    zd_map_modifiers((SDL_Keymod)source_event.key.keysym.mod);
                event.repeat = (uint8_t)(source_event.key.repeat != 0U);
                emit = 1;
                break;

            case SDL_TEXTINPUT:
                event.type = ZD_DEBIAN_EVENT_TEXT_INPUT;
                (void)snprintf(event.text, sizeof(event.text), "%s",
                               source_event.text.text);
                emit = 1;
                break;

            case SDL_MOUSEMOTION:
                event.type = ZD_DEBIAN_EVENT_POINTER_MOVE;
                event.x = source_event.motion.x;
                event.y = source_event.motion.y;
                event.delta_x = source_event.motion.xrel;
                event.delta_y = source_event.motion.yrel;
                event.buttons =
                    zd_map_mouse_mask(source_event.motion.state);
                mouse_buttons = event.buttons;
                emit = 1;
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
                event.button =
                    zd_map_mouse_button(source_event.button.button);
                if (event.button != 0U) {
                    uint32_t mask = zd_button_mask(event.button);
                    event.type =
                        (source_event.type == SDL_MOUSEBUTTONDOWN)
                        ? ZD_DEBIAN_EVENT_POINTER_DOWN
                        : ZD_DEBIAN_EVENT_POINTER_UP;
                    event.x = source_event.button.x;
                    event.y = source_event.button.y;
                    event.clicks = source_event.button.clicks;
                    if (event.type == ZD_DEBIAN_EVENT_POINTER_DOWN) {
                        mouse_buttons |= mask;
                    } else {
                        mouse_buttons &= ~mask;
                    }
                    event.buttons = mouse_buttons;
                    emit = 1;
                }
                break;

            case SDL_MOUSEWHEEL:
                event.type = ZD_DEBIAN_EVENT_POINTER_WHEEL;
                event.wheel_x = source_event.wheel.x;
                event.wheel_y = source_event.wheel.y;
                if (source_event.wheel.direction ==
                    SDL_MOUSEWHEEL_FLIPPED) {
                    event.wheel_x = -event.wheel_x;
                    event.wheel_y = -event.wheel_y;
                }
                (void)SDL_GetMouseState(&event.x, &event.y);
                emit = 1;
                break;

            case SDL_WINDOWEVENT:
                if (source_event.window.event ==
                    SDL_WINDOWEVENT_SIZE_CHANGED) {
                    if (app->resizable != 0U &&
                        source_event.window.data1 > 0 &&
                        source_event.window.data2 > 0) {
                        result = zd_resize_surface(
                            renderer, &texture, &pixels, &surface,
                            (uint32_t)source_event.window.data1,
                            (uint32_t)source_event.window.data2);
                        if (result != ZD_OK) {
                            running = 0;
                            break;
                        }
                        event.type = ZD_DEBIAN_EVENT_WINDOW_RESIZED;
                        event.width = surface.width;
                        event.height = surface.height;
                        emit = 1;
                    }
                } else if (source_event.window.event ==
                           SDL_WINDOWEVENT_FOCUS_GAINED) {
                    event.type = ZD_DEBIAN_EVENT_FOCUS_GAINED;
                    emit = 1;
                } else if (source_event.window.event ==
                           SDL_WINDOWEVENT_FOCUS_LOST) {
                    event.type = ZD_DEBIAN_EVENT_FOCUS_LOST;
                    mouse_buttons = 0U;
                    emit = 1;
                } else if (source_event.window.event ==
                           SDL_WINDOWEVENT_CLOSE) {
                    event.type = ZD_DEBIAN_EVENT_QUIT;
                    emit = 1;
                }
                break;

            default:
                break;
            }

            if (emit != 0) {
                (void)zd_debian_emit(app, &event, &running);
            }
            if (running == 0) break;
        }

        if (running == 0) break;

        app->render(&surface, app->user_data);

        /* RGBX is uploaded as byte-ordered RGBA; X becomes opaque alpha. */
        for (y = 0U; y < surface.height; ++y) {
            uint32_t x;
            uint8_t *row = surface.pixels +
                (size_t)y * surface.pitch_bytes;
            for (x = 0U; x < surface.width; ++x) {
                row[(size_t)x * 4U + 3U] = 0xFFU;
            }
        }

        if (SDL_UpdateTexture(texture, NULL, surface.pixels,
                              (int)surface.pitch_bytes) != 0) {
            result = zd_debian_fail("SDL_UpdateTexture");
            break;
        }
        if (SDL_RenderClear(renderer) != 0 ||
            SDL_RenderCopy(renderer, texture, NULL, NULL) != 0) {
            result = zd_debian_fail("SDL_Render");
            break;
        }
        SDL_RenderPresent(renderer);

        if (renderer_vsync == 0) SDL_Delay(1U);
    }

    SDL_StopTextInput();

cleanup:
    if (texture != NULL) SDL_DestroyTexture(texture);
    if (renderer != NULL) SDL_DestroyRenderer(renderer);
    if (window != NULL) SDL_DestroyWindow(window);
    free(pixels);
    SDL_Quit();
    return result;
}
