#include "app.h"
#include "pages.h"
#include "dropdown.h"

#include <stdio.h>
#include <string.h>

// Persistent Page A UI state
static Dropdown_State g_dd = { .isOpen = false, .selectedIndex = 0 };

// Options must live long-term (static is perfect)
static const char* g_opts[] = { "Option 1", "Option 2", "Option 3", "Option 4" };

// IMPORTANT: this buffer must be persistent (NOT stack), because Clay may render after build returns
static char g_sel_buf[256];

static Clay_ElementId id_page_a_button(void) {
    return Clay_GetElementId(CLAY_STRING("PageA/Button"));
}

void page_a_build(void) {
    // Button, Text, Dropdown (vertical stack)
    CLAY(Clay_GetElementId(CLAY_STRING("PageA/Stack")), (Clay_ElementDeclaration){
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) },
            .childGap = 12,
        },
    }) {
        CLAY_TEXT(
            CLAY_STRING("Page A"),
            CLAY_TEXT_CONFIG((Clay_TextElementConfig){
                .fontSize = 28,
                .textColor = (Clay_Color){ 245, 245, 245, 255 },
            })
        );

        // Button
        Clay_ElementId id_btn = id_page_a_button();
        const bool hover_btn = Clay_PointerOver(id_btn);

        CLAY(id_btn, (Clay_ElementDeclaration){
            .layout = {
                .sizing = { CLAY_SIZING_FIXED(300), CLAY_SIZING_FIXED(44) },
                .padding = { 10, 10, 10, 10 },
            },
            .backgroundColor = hover_btn ? (Clay_Color){ 90, 90, 95, 255 }
                                         : (Clay_Color){ 70, 70, 75, 255 },
        }) {
            CLAY_TEXT(
                CLAY_STRING("Page A Button"),
                CLAY_TEXT_CONFIG((Clay_TextElementConfig){
                    .fontSize = 18,
                    .textColor = (Clay_Color){ 245, 245, 245, 255 },
                })
            );
        }

        // Selection text box (persistent buffer!)
        const char* sel = g_opts[g_dd.selectedIndex] ? g_opts[g_dd.selectedIndex] : "";
        (void)snprintf(g_sel_buf, sizeof(g_sel_buf), "Selected: %s", sel);

        Clay_String sel_text = (Clay_String){ .chars = g_sel_buf, .length = (int)strlen(g_sel_buf) };

        CLAY(Clay_GetElementId(CLAY_STRING("PageA/SelectionText")), (Clay_ElementDeclaration){
            .layout = {
                .sizing = { CLAY_SIZING_FIXED(300), CLAY_SIZING_FIXED(40) },
                .padding = { 10, 10, 10, 10 },
            },
            .backgroundColor = (Clay_Color){ 55, 55, 60, 255 },
            .cornerRadius = 8,
        }) {
            CLAY_TEXT(
                sel_text,
                CLAY_TEXT_CONFIG((Clay_TextElementConfig){
                    .fontSize = 16,
                    .textColor = (Clay_Color){ 235, 235, 235, 255 },
                })
            );
        }

        // Dropdown
        const Dropdown_Config cfg = {
            .id_str = "PageA/Dropdown",
            .options = g_opts,
            .option_count = (int)(sizeof(g_opts) / sizeof(g_opts[0])),
            .label_prefix = "Pick: ",
        };
        dropdown_build(&cfg, &g_dd);
    }
}

void page_a_handle_input(App_State* app) {
    if (!app) return;

    const Dropdown_Config cfg = {
        .id_str = "PageA/Dropdown",
        .options = g_opts,
        .option_count = (int)(sizeof(g_opts) / sizeof(g_opts[0])),
        .label_prefix = "Pick: ",
    };

    // Let dropdown capture / act (respects app->has_active)
    (void)dropdown_handle_input(app, &cfg, &g_dd);

    // Page A button capture + action (optional; matches your click model)
    Clay_ElementId id_btn = id_page_a_button();

    if (app->in.mouse_pressed && !app->has_active) {
        if (Clay_PointerOver(id_btn)) {
            app->active_id = id_btn;
            app->has_active = true;
        }
    }

    if (app->in.mouse_released && app->has_active && app_id_equal(app->active_id, id_btn)) {
        if (Clay_PointerOver(id_btn)) {
            // Example action: toggle dropdown
            // g_dd.isOpen = !g_dd.isOpen;
        }
        app->has_active = false;
        app->active_id = app_null_id();
    }
}
