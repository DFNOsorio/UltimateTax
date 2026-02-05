#pragma once

// Standard
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#endif

// Sokol (NO *_IMPL here)
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "util/sokol_gl.h"

// Clay (NO CLAY_IMPLEMENTATION here)
#include "clay.h"

// Fontstash + sokol_fontstash backend (NO implementations here)
#include "fontstash.h"
#include "util/sokol_fontstash.h"

// --------------------
// App domain types
// --------------------
typedef enum App_Page {
    APP_PAGE_A = 0,
    APP_PAGE_B = 1,
} App_Page;

typedef struct App_Input {
    float mx, my;
    bool mouse_down;
    bool mouse_pressed;   // edge
    bool mouse_released;  // edge
} App_Input;

typedef struct App_Window {
    float fb_w, fb_h;         // sapp_widthf/heightf
#if defined(_WIN32)
    float client_w, client_h; // GetClientRect (best-effort)
#endif
} App_Window;

typedef struct App_State {
    App_Window win;
    App_Input  in;

    bool nav_open;
    App_Page page;

    // click capture
    bool has_active;
    Clay_ElementId active_id;

    // clay arena
    void* arena_mem;
    uint64_t arena_cap;
    Clay_Arena arena;
} App_State;

// Utility
static inline Clay_ElementId app_null_id(void) {
    Clay_ElementId id;
    for (size_t i = 0; i < sizeof(id); i++) ((uint8_t*)&id)[i] = 0;
    return id;
}

static inline bool app_id_equal(Clay_ElementId a, Clay_ElementId b) {
    const uint8_t* pa = (const uint8_t*)&a;
    const uint8_t* pb = (const uint8_t*)&b;
    for (size_t i = 0; i < sizeof(Clay_ElementId); i++) if (pa[i] != pb[i]) return false;
    return true;
}
