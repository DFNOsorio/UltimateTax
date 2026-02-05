// src/main.c
#define _CRT_SECURE_NO_WARNINGS

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// --- Sokol ---
#define SOKOL_IMPL
#define SOKOL_D3D11
#define SOKOL_WIN32_FORCE_MAIN
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "util/sokol_gl.h"

// --- Clay ---
#define CLAY_IMPLEMENTATION
#include "clay.h"

// --- fontstash pulls in stb_truetype internally ---
// IMPORTANT: do NOT include stb_truetype.h yourself.
#define STB_TRUETYPE_IMPLEMENTATION
#define FONTSTASH_IMPLEMENTATION
#include "fontstash.h"

// --- sokol_fontstash backend ---
#define SOKOL_FONTSTASH_IMPL
#include "util/sokol_fontstash.h"

// --------------------
// App state
// --------------------
typedef enum Page {
    PAGE_A = 0,
    PAGE_B = 1,
} Page;

static struct {
    // input
    float mx, my;
    bool mouse_down;
    bool mouse_pressed;   // edge
    bool mouse_released;  // edge

    // UI
    bool nav_open;
    Page page;

    // click capture
    bool has_active;
    Clay_ElementId active_id;

    // Clay arena
    void* arena_mem;
    uint64_t arena_cap;
    Clay_Arena arena;
} g;

// framebuffer size (actual render target)
static float g_fb_w = 1.0f, g_fb_h = 1.0f;
// live window client size (changes during drag)
static float g_client_w = 1.0f, g_client_h = 1.0f;

// Option 2C heuristic (no WM hooks available):
// Scale only while client size != framebuffer size.
// Baseline captured when mismatch begins.
static bool  g_scaling_active = false;
static float g_scale_x = 1.0f, g_scale_y = 1.0f;

static FONScontext* g_fons = NULL;
static int g_font_ui = -1;

// --------------------
// Helpers
// --------------------
static Clay_ElementId clay_null_id(void) {
    Clay_ElementId id;
    memset(&id, 0, sizeof(id));
    return id;
}

static bool clay_id_equal(Clay_ElementId a, Clay_ElementId b) {
    return 0 == memcmp(&a, &b, sizeof(Clay_ElementId));
}

static float f_abs(float x) { return (x < 0.0f) ? -x : x; }

// Read framebuffer size and live client size.
// Decide whether to scale (2C) based on mismatch.
static void update_sizes_and_scale(void) {
    // framebuffer size (render target)
    g_fb_w = sapp_widthf();
    g_fb_h = sapp_heightf();
    if (g_fb_w < 1.0f) g_fb_w = 1.0f;
    if (g_fb_h < 1.0f) g_fb_h = 1.0f;

    // client size (typically in logical pixels / DIPs while high_dpi=true)
#if defined(_WIN32)
    HWND hwnd = (HWND)(uintptr_t)sapp_win32_get_hwnd();
    if (hwnd) {
        RECT r;
        if (GetClientRect(hwnd, &r)) {
            float cw = (float)(r.right - r.left);
            float ch = (float)(r.bottom - r.top);
            if (cw > 1.0f) g_client_w = cw;
            if (ch > 1.0f) g_client_h = ch;
        }
    }
#endif

    if (g_client_w < 1.0f) g_client_w = g_fb_w;
    if (g_client_h < 1.0f) g_client_h = g_fb_h;

    // Convert client (logical) -> framebuffer pixels
    float dpi = sapp_dpi_scale();
    if (dpi < 0.01f) dpi = 1.0f;

    const float client_px_w = g_client_w * dpi;
    const float client_px_h = g_client_h * dpi;

    // mismatch threshold (in framebuffer pixels)
    const bool mismatch =
        (f_abs(client_px_w - g_fb_w) > 2.0f) ||
        (f_abs(client_px_h - g_fb_h) > 2.0f);

    g_scaling_active = mismatch;

    if (g_scaling_active) {
        // Scale the stale framebuffer to match the live client size (both in fb pixels now)
        g_scale_x = client_px_w / g_fb_w;
        g_scale_y = client_px_h / g_fb_h;

        // clamp
        if (g_scale_x < 0.25f) g_scale_x = 0.25f;
        if (g_scale_y < 0.25f) g_scale_y = 0.25f;
        if (g_scale_x > 4.00f) g_scale_x = 4.00f;
        if (g_scale_y > 4.00f) g_scale_y = 4.00f;
    } else {
        g_scale_x = 1.0f;
        g_scale_y = 1.0f;
    }
}



#if defined(_WIN32)
static bool file_existsA(const char* p) {
    DWORD a = GetFileAttributesA(p);
    return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static void get_windows_ui_font_path(char out_path[MAX_PATH]) {
    // Prefer Segoe UI (typical Windows UI font)
    // Fallbacks: Segoe UI Variable (Win11), Arial
    char win[MAX_PATH] = {0};
    UINT n = GetWindowsDirectoryA(win, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        out_path[0] = 0;
        return;
    }

    const char* candidates[] = {
        "\\Fonts\\segoeui.ttf",
        "\\Fonts\\seguiemj.ttf",
        "\\Fonts\\SegoeUI.ttf",              // sometimes different casing
        "\\Fonts\\SegoeUIVariable.ttf",      // some Win11 installs
        "\\Fonts\\arial.ttf",
    };

    for (int i = 0; i < (int)(sizeof(candidates)/sizeof(candidates[0])); i++) {
        snprintf(out_path, MAX_PATH, "%s%s", win, candidates[i]);
        if (file_existsA(out_path)) return;
    }

    out_path[0] = 0;
}
#endif

// --------------------
// Minimal drawing (sokol_gl + sokol_debugtext)
// --------------------
static void draw_rect(float x, float y, float w, float h, Clay_Color c) {
    sgl_c4b((uint8_t)c.r, (uint8_t)c.g, (uint8_t)c.b, (uint8_t)c.a);
    sgl_begin_quads();
    sgl_v2f(x,     y);
    sgl_v2f(x + w, y);
    sgl_v2f(x + w, y + h);
    sgl_v2f(x,     y + h);
    sgl_end();
}

static void draw_text_px(float x, float y, Clay_StringSlice s, Clay_Color c, float font_px) {
    if (!g_fons || g_font_ui < 0 || s.length <= 0) return;

    // fontstash expects null-terminated C string; Clay gives slice
    // For simplicity: stack buffer for short strings; heap for long.
    char stack[512];
    char* tmp = stack;

    int len = (int)s.length;
    if (len >= (int)sizeof(stack)) {
        tmp = (char*)malloc((size_t)len + 1);
        if (!tmp) return;
    }
    memcpy(tmp, s.chars, (size_t)len);
    tmp[len] = 0;

    fonsSetSize(g_fons, font_px);
    fonsSetFont(g_fons, g_font_ui);
    fonsSetColor(g_fons, sfons_rgba((uint8_t)c.r, (uint8_t)c.g, (uint8_t)c.b, (uint8_t)c.a));
    fonsSetAlign(g_fons, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

    // Draw at exact pixel coords (obeys your current sgl transforms!)
    fonsDrawText(g_fons, x, y, tmp, NULL);

    if (tmp != stack) free(tmp);
}


static Clay_Dimensions measure_text(Clay_StringSlice text, Clay_TextElementConfig* cfg, void* userData) {
    (void)userData;
    Clay_Dimensions out = {0};

    float font_px = (cfg && cfg->fontSize > 0) ? (float)cfg->fontSize : 16.0f;

    if (!g_fons || g_font_ui < 0 || text.length <= 0) {
        // fallback estimate
        out.width  = (float)text.length * font_px * 0.60f;
        out.height = font_px;
        return out;
    }

    char stack[512];
    char* tmp = stack;
    int len = (int)text.length;
    if (len >= (int)sizeof(stack)) {
        tmp = (char*)malloc((size_t)len + 1);
        if (!tmp) {
            out.width  = (float)text.length * font_px * 0.60f;
            out.height = font_px;
            return out;
        }
    }
    memcpy(tmp, text.chars, (size_t)len);
    tmp[len] = 0;

    fonsSetSize(g_fons, font_px);
    fonsSetFont(g_fons, g_font_ui);
    fonsSetAlign(g_fons, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

    float bounds[4];
    fonsTextBounds(g_fons, 0.0f, 0.0f, tmp, NULL, bounds);
    out.width  = bounds[2] - bounds[0];
    out.height = font_px;

    if (tmp != stack) free(tmp);
    return out;
}

// static void debug_overlay(void) {
//     if (!g_fons || g_font_ui < 0) return;

//     char buf[512];

//     float dpi = sapp_dpi_scale();
//     if (dpi < 0.01f) dpi = 1.0f;

//     // what you actually use for scaling (if you applied my DPI-correct version)
//     float client_px_w = g_client_w * dpi;
//     float client_px_h = g_client_h * dpi;

//     snprintf(buf, sizeof(buf),
//         "fb:     %.1f x %.1f\n"
//         "client: %.1f x %.1f\n"
//         "dpi:    %.3f\n"
//         "client_px: %.1f x %.1f\n"
//         "mismatch: %s\n"
//         "scale:  %.4f x %.4f\n",
//         g_fb_w, g_fb_h,
//         g_client_w, g_client_h,
//         dpi,
//         client_px_w, client_px_h,
//         g_scaling_active ? "YES" : "no",
//         g_scale_x, g_scale_y
//     );

//     // draw a small background so it’s readable
//     draw_rect(8.0f, 8.0f, 360.0f, 150.0f, (Clay_Color){ 0, 0, 0, 170 });

//     // draw debug text (top-left)
//     fonsSetSize(g_fons, 16.0f);
//     fonsSetFont(g_fons, g_font_ui);
//     fonsSetColor(g_fons, sfons_rgba(255, 255, 255, 255));
//     fonsSetAlign(g_fons, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

//     // IMPORTANT:
//     // If you are scaling the whole scene via sgl_scale() while resizing,
//     // and you want the debug to NOT scale, you must draw it BEFORE scaling
//     // or temporarily reset the modelview. For now it’s fine if it scales.
//     fonsDrawText(g_fons, 16.0f, 16.0f, buf, NULL);
// }


// --------------------
// Clay UI IDs
// --------------------
static Clay_ElementId ID_NAV_A;
static Clay_ElementId ID_NAV_B;
static Clay_ElementId ID_TOGGLE;

// --------------------
// Build UI
// --------------------
static Clay_RenderCommandArray build_ui(void) {
    // Layout is in framebuffer coords (crisp). We only scale the final draw while resizing.
    Clay_SetLayoutDimensions((Clay_Dimensions){ g_fb_w, g_fb_h });

    // pointer must match layout coords; during scaling, unscale pointer
    const float px = (g_scale_x != 0.0f) ? (g.mx / g_scale_x) : g.mx;
    const float py = (g_scale_y != 0.0f) ? (g.my / g_scale_y) : g.my;
    Clay_SetPointerState((Clay_Vector2){ px, py }, g.mouse_down);

    ID_NAV_A  = Clay_GetElementId(CLAY_STRING("NavA"));
    ID_NAV_B  = Clay_GetElementId(CLAY_STRING("NavB"));
    ID_TOGGLE = Clay_GetElementId(CLAY_STRING("Toggle"));

    const float nav_w = g.nav_open ? 220.0f : 60.0f;

    Clay_BeginLayout();

    CLAY(Clay_GetElementId(CLAY_STRING("Root")), (Clay_ElementDeclaration){
        .layout = {
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
        },
        .backgroundColor = (Clay_Color){ 24, 24, 26, 255 },
    }) {
        CLAY(Clay_GetElementId(CLAY_STRING("Nav")), (Clay_ElementDeclaration){
            .layout = {
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .sizing = { CLAY_SIZING_FIXED(nav_w), CLAY_SIZING_GROW(0) },
                .padding = { 12, 12, 12, 12 },
                .childGap = 10,
            },
            .backgroundColor = (Clay_Color){ 35, 35, 40, 255 },
        }) {
            CLAY_TEXT(
                g.nav_open ? CLAY_STRING("NAV") : CLAY_STRING("N"),
                CLAY_TEXT_CONFIG((Clay_TextElementConfig){
                    .fontSize = 22,
                    .textColor = (Clay_Color){ 240, 240, 245, 255 },
                })
            );

            const bool hover_a = Clay_PointerOver(ID_NAV_A);
            CLAY(ID_NAV_A, (Clay_ElementDeclaration){
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(44) },
                    .padding = { 10, 10, 10, 10 },
                },
                .backgroundColor = hover_a ? (Clay_Color){ 70, 70, 80, 255 } : (Clay_Color){ 55, 55, 65, 255 },
            }) {
                CLAY_TEXT(
                    g.nav_open ? CLAY_STRING("Page A") : CLAY_STRING("A"),
                    CLAY_TEXT_CONFIG((Clay_TextElementConfig){
                        .fontSize = 18,
                        .textColor = (Clay_Color){ 255, 255, 255, 255 },
                    })
                );
            }

            const bool hover_b = Clay_PointerOver(ID_NAV_B);
            CLAY(ID_NAV_B, (Clay_ElementDeclaration){
                .layout = {
                    .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(44) },
                    .padding = { 10, 10, 10, 10 },
                },
                .backgroundColor = hover_b ? (Clay_Color){ 70, 70, 80, 255 } : (Clay_Color){ 55, 55, 65, 255 },
            }) {
                CLAY_TEXT(
                    g.nav_open ? CLAY_STRING("Page B") : CLAY_STRING("B"),
                    CLAY_TEXT_CONFIG((Clay_TextElementConfig){
                        .fontSize = 18,
                        .textColor = (Clay_Color){ 255, 255, 255, 255 },
                    })
                );
            }
        }

        CLAY(Clay_GetElementId(CLAY_STRING("Content")), (Clay_ElementDeclaration){
            .layout = {
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                .padding = { 18, 18, 18, 18 },
                .childGap = 12,
            },
            .backgroundColor = (Clay_Color){ 28, 28, 32, 255 },
        }) {
            CLAY_TEXT(
                (g.page == PAGE_A) ? CLAY_STRING("Page A") : CLAY_STRING("Page B"),
                CLAY_TEXT_CONFIG((Clay_TextElementConfig){
                    .fontSize = 28,
                    .textColor = (Clay_Color){ 245, 245, 245, 255 },
                })
            );

            const bool hover_t = Clay_PointerOver(ID_TOGGLE);
            CLAY(ID_TOGGLE, (Clay_ElementDeclaration){
                .layout = {
                    .sizing = { CLAY_SIZING_FIXED(260), CLAY_SIZING_FIXED(44) },
                    .padding = { 10, 10, 10, 10 },
                },
                .backgroundColor = hover_t ? (Clay_Color){ 90, 90, 95, 255 } : (Clay_Color){ 70, 70, 75, 255 },
            }) {
                CLAY_TEXT(
                    g.nav_open ? CLAY_STRING("Toggle Nav (Collapse)") : CLAY_STRING("Toggle Nav (Expand)"),
                    CLAY_TEXT_CONFIG((Clay_TextElementConfig){
                        .fontSize = 18,
                        .textColor = (Clay_Color){ 245, 245, 245, 255 },
                    })
                );
            }
        }
    }

    return Clay_EndLayout();
}

// --------------------
// Click handling (press-capture, release-trigger)
// --------------------
static void handle_clicks(void) {
    const bool pressed  = g.mouse_pressed;
    const bool released = g.mouse_released;
    g.mouse_pressed = false;
    g.mouse_released = false;

    if (pressed) {
        g.has_active = false;
        g.active_id = clay_null_id();

        if (Clay_PointerOver(ID_NAV_A)) { g.active_id = ID_NAV_A; g.has_active = true; }
        else if (Clay_PointerOver(ID_NAV_B)) { g.active_id = ID_NAV_B; g.has_active = true; }
        else if (Clay_PointerOver(ID_TOGGLE)) { g.active_id = ID_TOGGLE; g.has_active = true; }
    }

    if (released) {
        if (g.has_active && Clay_PointerOver(g.active_id)) {
            if (clay_id_equal(g.active_id, ID_NAV_A)) g.page = PAGE_A;
            else if (clay_id_equal(g.active_id, ID_NAV_B)) g.page = PAGE_B;
            else if (clay_id_equal(g.active_id, ID_TOGGLE)) g.nav_open = !g.nav_open;
        }
        g.has_active = false;
        g.active_id = clay_null_id();
    }
}

// --------------------
// Render Clay commands (your Clay field names)
// --------------------
static void render_clay(Clay_RenderCommandArray cmds) {
    const int32_t count = cmds.length;
    Clay_RenderCommand* arr = cmds.internalArray;
    if (!arr || count <= 0) return;

    for (int32_t i = 0; i < count; i++) {
        Clay_RenderCommand* cmd = &arr[i];
        Clay_BoundingBox bb = cmd->boundingBox;

        switch (cmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                Clay_RectangleRenderData* r = &cmd->renderData.rectangle;
                draw_rect(bb.x, bb.y, bb.width, bb.height, r->backgroundColor);
            } break;

            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                Clay_TextRenderData* t = &cmd->renderData.text;
                float font_px = 16.0f; // adjust to your clay.h field name
                draw_text_px(bb.x, bb.y, t->stringContents, t->textColor, font_px);
            } break;

            default:
                break;
        }
    }
}

// --------------------
// Sokol lifecycle
// --------------------
static void init(void) {
    sg_setup(&(sg_desc){ .environment = sglue_environment() });
    sgl_setup(&(sgl_desc_t){});

    // Create font atlas (power-of-two sizes are typical)
    g_fons = sfons_create(&(sfons_desc_t){ .width = 1024, .height = 1024 });
    if (!g_fons) {
        // hard fail; you can also fallback to no-text mode
        abort();
    }

    #if defined(_WIN32)
    char font_path[MAX_PATH];
    get_windows_ui_font_path(font_path);
    if (font_path[0]) {
        g_font_ui = fonsAddFont(g_fons, "ui", font_path);
    }
    #endif

    // If font load failed, you can still run; text just won't render.

    g.nav_open = true;
    g.page = PAGE_A;
    g.has_active = false;
    g.active_id = clay_null_id();

    update_sizes_and_scale();

    g.arena_cap = Clay_MinMemorySize();
    g.arena_mem = malloc((size_t)g.arena_cap);
    g.arena = Clay_CreateArenaWithCapacityAndMemory(g.arena_cap, g.arena_mem);

    Clay_Initialize(
        g.arena,
        (Clay_Dimensions){ g_fb_w, g_fb_h },
        (Clay_ErrorHandler){ 0 }
    );
    Clay_SetMeasureTextFunction(measure_text, NULL);
}

static void frame(void) {
    if (sapp_width() == 0 || sapp_height() == 0) return;

    update_sizes_and_scale();

    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, g_fb_w, g_fb_h, 0.0f, -1.0f, +1.0f);

    // debugtext canvas must be >0
    int cw = (int)g_fb_w; if (cw < 1) cw = 1;
    int ch = (int)g_fb_h; if (ch < 1) ch = 1;

    Clay_RenderCommandArray cmds = build_ui();
    handle_clicks();

    sg_pass_action action = {0};
    action.colors[0].load_action = SG_LOADACTION_CLEAR;
    action.colors[0].clear_value = (sg_color){ 0.10f, 0.10f, 0.11f, 1.0f };

    sg_begin_pass(&(sg_pass){
        .action = action,
        .swapchain = sglue_swapchain()
    });

    sgl_matrix_mode_modelview();
    sgl_load_identity();

    render_clay(cmds);

    if (g_scaling_active) {
        sgl_scale(g_scale_x, g_scale_y, 1.0f);
    }

    if (g_fons) {
        sfons_flush(g_fons);
    }
    sgl_draw();

    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    if (g_fons) {
        sfons_destroy(g_fons);
        g_fons = NULL;
    }
    sgl_shutdown();
    sg_shutdown();
    free(g.arena_mem);
}

static void event(const sapp_event* e) {
    switch (e->type) {
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            g.mx = e->mouse_x;
            g.my = e->mouse_y;
            break;

        case SAPP_EVENTTYPE_MOUSE_DOWN:
            if (e->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
                g.mouse_down = true;
                g.mouse_pressed = true;
            }
            break;

        case SAPP_EVENTTYPE_MOUSE_UP:
            if (e->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
                g.mouse_down = false;
                g.mouse_released = true;
            }
            break;

        default:
            break;
    }
}

sapp_desc sokol_main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    return (sapp_desc){
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .width = 1100,
        .height = 720,
        .window_title = "Sokol + Clay (2C heuristic: scale while resizing, snap crisp on release)",
        .high_dpi = true,
    };
}
