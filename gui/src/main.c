#include "app.h"
#include "ui_renderer.h"
#include "app_ui.h"

// Add sokol_time for precise timing
#define SOKOL_TIME_IMPL
#include "sokol_time.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(_WIN32)
    #include <windows.h>
    #include <timeapi.h> // Required for high-res timer
    #pragma comment(lib, "winmm.lib") // Links the multimedia library
#else
    #include <unistd.h>  // For usleep()
#endif

static App_State  g_app;
static UiRenderer g_rend;

// Frame Timing State
static uint64_t g_last_time = 0;
// 1.0 / 60.0 = ~0.01667 seconds per frame
#define TARGET_FRAME_TIME_SEC (1.0 / 60.0)

// ... [app_update_window_sizes remains the same] ...
static void app_update_window_sizes(App_State* app) {
    app->win.fb_w = sapp_widthf();
    app->win.fb_h = sapp_heightf();
    if (app->win.fb_w < 1.0f) app->win.fb_w = 1.0f;
    if (app->win.fb_h < 1.0f) app->win.fb_h = 1.0f;

#if defined(_WIN32)
    HWND hwnd = (HWND)(uintptr_t)sapp_win32_get_hwnd();
    if (hwnd) {
        RECT r;
        if (GetClientRect(hwnd, &r)) {
            app->win.client_w = (float)(r.right - r.left);
            app->win.client_h = (float)(r.bottom - r.top);
        }
    }
    if (app->win.client_w < 1.0f) app->win.client_w = app->win.fb_w;
    if (app->win.client_h < 1.0f) app->win.client_h = app->win.fb_h;
#endif
}

static void init(void) {
    // 1. Enable High-Res Timer on Windows (1ms resolution)
    // This prevents Sleep(1) from sleeping for 15ms.
    #if defined(_WIN32)
        timeBeginPeriod(1);
    #endif

    // 2. Initialize Sokol Time
    stm_setup();
    g_last_time = stm_now();

    memset(&g_app, 0, sizeof(g_app));
    g_app.nav_open = true;
    g_app.page = APP_PAGE_A;
    g_app.active_id = app_null_id();

    // No Sokol logger
    sg_setup(&(sg_desc){
        .environment = sglue_environment(),
        .logger = {0},
    });

    UiRenderer_Config rcfg = {0};
    (void)ui_renderer_init(&g_rend, &rcfg);

    app_update_window_sizes(&g_app);

    // Clay init
    g_app.arena_cap = Clay_MinMemorySize();
    g_app.arena_mem = malloc((size_t)g_app.arena_cap);
    g_app.arena = Clay_CreateArenaWithCapacityAndMemory(g_app.arena_cap, g_app.arena_mem);

    Clay_Initialize(
        g_app.arena,
        (Clay_Dimensions){ g_app.win.fb_w, g_app.win.fb_h },
        (Clay_ErrorHandler){ 0 }
    );

    Clay_SetMeasureTextFunction(ui_renderer_measure_text, &g_rend);
}

static void frame(void) {
    // --- 1. Frame Limiter Logic (Start of frame) ---
    // Measure time since the last frame started
    double elapsed_sec = stm_sec(stm_diff(stm_now(), g_last_time));

    if (elapsed_sec < TARGET_FRAME_TIME_SEC) {
        double diff = TARGET_FRAME_TIME_SEC - elapsed_sec;

        // HYBRID SLEEP STRATEGY:
        // Only call OS sleep if we have a "safe" amount of time (> 2ms).
        // Calling Sleep() for small durations is unreliable.
        if (diff > 0.002) {
            #if defined(_WIN32)
                // Sleep for the bulk of the time (minus 2ms buffer)
                Sleep((DWORD)((diff - 0.002) * 1000.0));
            #else
                usleep((useconds_t)((diff - 0.002) * 1000000.0));
            #endif
        }

        // Spin-lock for the final precision (consumes CPU for last 0-2ms)
        // This ensures we hit the deadline exactly.
        while (stm_sec(stm_diff(stm_now(), g_last_time)) < TARGET_FRAME_TIME_SEC) {
            // spin
        }
    }
    // Update the time marker for the NEXT frame
    g_last_time = stm_now();
    // ----------------------------------------------


    if (sapp_width() == 0 || sapp_height() == 0) return;

    app_update_window_sizes(&g_app);

    sgl_defaults();
    sgl_matrix_mode_projection();
    sgl_ortho(0.0f, g_app.win.fb_w, g_app.win.fb_h, 0.0f, -1.0f, +1.0f);
    sgl_matrix_mode_modelview();
    sgl_load_identity();

    AppUi_Ids ids;
    Clay_RenderCommandArray cmds = app_ui_build(&g_app, &ids);
    app_ui_handle_clicks(&g_app, &ids);

    sg_pass_action action = {0};
    action.colors[0].load_action = SG_LOADACTION_CLEAR;
    action.colors[0].clear_value = (sg_color){ 0.10f, 0.10f, 0.11f, 1.0f };

    sg_begin_pass(&(sg_pass){
        .action = action,
        .swapchain = sglue_swapchain()
    });

    ui_renderer_render_clay(&g_rend, cmds);

    #if defined(_WIN32)
        // --- FPS CALCULATION ---
        static uint64_t last_fps_time = 0;
        double fps_dt = stm_sec(stm_laptime(&last_fps_time));
        double fps = (fps_dt > 0.0) ? (1.0 / fps_dt) : 0.0;

        char l1[128], l2[128];
        snprintf(l1, sizeof(l1), "FPS: %.0f | fb: %.0fx%.0f", fps, g_app.win.fb_w, g_app.win.fb_h);
        snprintf(l2, sizeof(l2), "client: %.0fx%.0f", g_app.win.client_w, g_app.win.client_h);

        // Calculate position: Screen Width - Approximate Box Width - Padding
        // Assuming ~280px is enough width for the text
        float dbg_w = 280.0f;
        float dbg_x = g_app.win.fb_w - dbg_w - 10.0f;
        float dbg_y = 10.0f;

        // Ensure it doesn't go off-screen if window is tiny
        if (dbg_x < 10.0f) dbg_x = 10.0f;

        ui_renderer_debug_overlay(&g_rend, dbg_x, dbg_y, l1, l2);
    #endif

    ui_renderer_flush(&g_rend);

    sg_end_pass();
    sg_commit();
}

static void cleanup(void) {
    ui_renderer_shutdown(&g_rend);
    sg_shutdown();
    free(g_app.arena_mem);

    // Reset timer resolution
    #if defined(_WIN32)
        timeEndPeriod(1);
    #endif
}

static void event(const sapp_event* e) {
    switch (e->type) {
        case SAPP_EVENTTYPE_MOUSE_MOVE:
            g_app.in.mx = e->mouse_x;
            g_app.in.my = e->mouse_y;
            break;
        case SAPP_EVENTTYPE_MOUSE_DOWN:
            if (e->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
                g_app.in.mouse_down = true;
                g_app.in.mouse_pressed = true;
            }
            break;
        case SAPP_EVENTTYPE_MOUSE_UP:
            if (e->mouse_button == SAPP_MOUSEBUTTON_LEFT) {
                g_app.in.mouse_down = false;
                g_app.in.mouse_released = true;
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
        .window_title = "Sokol + Clay (Locked 60 FPS)",
        .high_dpi = true,

        // Keep this at 1 (VSync) to prevent tearing.
        .swap_interval = 1,
    };
}
