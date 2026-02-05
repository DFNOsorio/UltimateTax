#include "app_ui.h"
#include "navbar.h"
#include "pages.h"

static Clay_ElementId g_id_toggle(void) {
    return Clay_GetElementId(CLAY_STRING("Toggle"));
}

Clay_RenderCommandArray app_ui_build(App_State* app, AppUi_Ids* out_ids) {
    // Layout in framebuffer coordinates (stable, no weird scaling artifacts)
    Clay_SetLayoutDimensions((Clay_Dimensions){ app->win.fb_w, app->win.fb_h });

    // Pointer in same coordinate space as layout
    Clay_SetPointerState((Clay_Vector2){ app->in.mx, app->in.my }, app->in.mouse_down);

    out_ids->id_toggle = g_id_toggle();

    Clay_BeginLayout();

    CLAY(Clay_GetElementId(CLAY_STRING("Root")), (Clay_ElementDeclaration){
        .layout = {
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
        },
        .backgroundColor = (Clay_Color){ 24, 24, 26, 255 },
    }) {
        // NAVBAR (component)
        Navbar_Result nav_ids = navbar_build(app->nav_open);
        out_ids->id_nav_a = nav_ids.id_nav_a;
        out_ids->id_nav_b = nav_ids.id_nav_b;

        // CONTENT
        CLAY(Clay_GetElementId(CLAY_STRING("Content")), (Clay_ElementDeclaration){
            .layout = {
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
                .padding = { 18, 18, 18, 18 },
                .childGap = 12,
            },
            .backgroundColor = (Clay_Color){ 28, 28, 32, 255 },
        }) {
            // page content
            if (app->page == APP_PAGE_A) page_a_build();
            else page_b_build();

            // toggle button
            const bool hover_t = Clay_PointerOver(out_ids->id_toggle);
            CLAY(out_ids->id_toggle, (Clay_ElementDeclaration){
                .layout = {
                    .sizing = { CLAY_SIZING_FIXED(300), CLAY_SIZING_FIXED(44) },
                    .padding = { 10, 10, 10, 10 },
                },
                .backgroundColor = hover_t ? (Clay_Color){ 90, 90, 95, 255 } : (Clay_Color){ 70, 70, 75, 255 },
            }) {
                CLAY_TEXT(
                    app->nav_open ? CLAY_STRING("Toggle Nav (Collapse)") : CLAY_STRING("Toggle Nav (Expand)"),
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

void app_ui_handle_clicks(App_State* app, const AppUi_Ids* ids) {
    const bool pressed  = app->in.mouse_pressed;
    const bool released = app->in.mouse_released;
    app->in.mouse_pressed = false;
    app->in.mouse_released = false;

    if (pressed) {
        app->has_active = false;
        app->active_id = app_null_id();

        if (Clay_PointerOver(ids->id_nav_a)) { app->active_id = ids->id_nav_a; app->has_active = true; }
        else if (Clay_PointerOver(ids->id_nav_b)) { app->active_id = ids->id_nav_b; app->has_active = true; }
        else if (Clay_PointerOver(ids->id_toggle)) { app->active_id = ids->id_toggle; app->has_active = true; }
    }

    if (released) {
        if (app->has_active && Clay_PointerOver(app->active_id)) {
            if (app_id_equal(app->active_id, ids->id_nav_a)) app->page = APP_PAGE_A;
            else if (app_id_equal(app->active_id, ids->id_nav_b)) app->page = APP_PAGE_B;
            else if (app_id_equal(app->active_id, ids->id_toggle)) app->nav_open = !app->nav_open;
        }
        app->has_active = false;
        app->active_id = app_null_id();
    }
}
