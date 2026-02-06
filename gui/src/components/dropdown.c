#include "dropdown.h"

#include <string.h>
#include <stdio.h>

// ---------- internal helpers ----------

static Clay_String dd_cstr_(const char* s) {
    if (!s) s = "";
    return (Clay_String){ .chars = (char*)s, .length = (int)strlen(s) };
}

static Clay_ElementId dd_id_from_cstr_(const char* s) {
    return Clay_GetElementId(dd_cstr_(s));
}

static Clay_ElementId dd_id_root_(const Dropdown_Config* cfg) {
    return dd_id_from_cstr_(cfg->id_str);
}

static Clay_ElementId dd_id_trigger_(const Dropdown_Config* cfg, char* buf, size_t cap) {
    (void)snprintf(buf, cap, "%s/trigger", cfg->id_str);
    return dd_id_from_cstr_(buf);
}

static Clay_ElementId dd_id_list_(const Dropdown_Config* cfg, char* buf, size_t cap) {
    (void)snprintf(buf, cap, "%s/list", cfg->id_str);
    return dd_id_from_cstr_(buf);
}

static Clay_ElementId dd_id_option_(const Dropdown_Config* cfg, int i, char* buf, size_t cap) {
    (void)snprintf(buf, cap, "%s/opt/%d", cfg->id_str, i);
    return dd_id_from_cstr_(buf);
}

static bool dd_id_is_ours_(const Dropdown_Config* cfg, Clay_ElementId id) {
    char buf[128];
    if (app_id_equal(id, dd_id_trigger_(cfg, buf, sizeof(buf)))) return true;
    for (int i = 0; i < cfg->option_count; i++) {
        if (app_id_equal(id, dd_id_option_(cfg, i, buf, sizeof(buf)))) return true;
    }
    return false;
}

static int dd_over_option_index_(const Dropdown_Config* cfg) {
    char buf[128];
    for (int i = 0; i < cfg->option_count; i++) {
        Clay_ElementId id_opt = dd_id_option_(cfg, i, buf, sizeof(buf));
        if (Clay_PointerOver(id_opt)) return i;
    }
    return -1;
}

// ---------- public API ----------

void dropdown_build(const Dropdown_Config* cfg, Dropdown_State* st) {
    if (!cfg || !st || !cfg->id_str) return;
    if (!cfg->options || cfg->option_count <= 0) return;

    // Clamp selection
    if (st->selectedIndex < -1) st->selectedIndex = -1;
    if (st->selectedIndex >= cfg->option_count) st->selectedIndex = cfg->option_count - 1;

    char buf[128];
    Clay_ElementId id_root    = dd_id_root_(cfg);
    Clay_ElementId id_trigger = dd_id_trigger_(cfg, buf, sizeof(buf));

    const bool hover_trigger = Clay_PointerOver(id_trigger);

    CLAY(id_root, (Clay_ElementDeclaration){
        .layout = {
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) },
            .childGap = 6,
        },
    }) {
        // Trigger
        CLAY(id_trigger, (Clay_ElementDeclaration){
            .layout = {
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                .sizing = { CLAY_SIZING_FIXED(260), CLAY_SIZING_FIXED(40) },
                .padding = { 10, 10, 10, 10 },
                .childGap = 8,
            },
            .backgroundColor = hover_trigger
                ? (Clay_Color){ 90, 90, 95, 255 }
                : (Clay_Color){ 70, 70, 75, 255 },
            .cornerRadius = 8,
        }) {
            // Prefix (stable string)
            if (cfg->label_prefix && cfg->label_prefix[0] != '\0') {
                CLAY_TEXT(
                    dd_cstr_(cfg->label_prefix),
                    CLAY_TEXT_CONFIG((Clay_TextElementConfig){
                        .fontSize = 16,
                        .textColor = (Clay_Color){ 245, 245, 245, 255 },
                    })
                );
            }

            // Selected option (stable string)
            const char* display_text = "Select..."; // Default placeholder
            if (st->selectedIndex >= 0) {
                display_text = cfg->options[st->selectedIndex] ? cfg->options[st->selectedIndex] : "";
            }

            CLAY_TEXT(
                dd_cstr_(display_text),
                CLAY_TEXT_CONFIG((Clay_TextElementConfig){
                    .fontSize = 16,
                    .textColor = (Clay_Color){ 245, 245, 245, 255 },
                })
            );

            // Caret
            CLAY_TEXT(
                st->isOpen ? CLAY_STRING("▲") : CLAY_STRING("▼"),
                CLAY_TEXT_CONFIG((Clay_TextElementConfig){
                    .fontSize = 16,
                    .textColor = (Clay_Color){ 220, 220, 220, 255 },
                })
            );
        }

        // Options list
        if (st->isOpen) {
            Clay_ElementId id_list = dd_id_list_(cfg, buf, sizeof(buf));

            CLAY(id_list, (Clay_ElementDeclaration){
                .layout = {
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .sizing = { CLAY_SIZING_FIXED(260), CLAY_SIZING_FIT(0) },
                    .padding = { 6, 6, 6, 6 },
                    .childGap = 4,
                },
                .backgroundColor = (Clay_Color){ 55, 55, 60, 255 },
                .cornerRadius = 10,
            }) {
                for (int i = 0; i < cfg->option_count; i++) {
                    Clay_ElementId id_opt = dd_id_option_(cfg, i, buf, sizeof(buf));
                    const bool hover = Clay_PointerOver(id_opt);
                    const bool sel = (i == st->selectedIndex);

                    CLAY(id_opt, (Clay_ElementDeclaration){
                        .layout = {
                            .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(34) },
                            .padding = { 10, 8, 10, 8 },
                        },
                        .backgroundColor = hover
                            ? (Clay_Color){ 85, 85, 90, 255 }
                            : (sel ? (Clay_Color){ 75, 75, 80, 255 }
                                   : (Clay_Color){ 0, 0, 0, 0 }),
                        .cornerRadius = 8,
                    }) {
                        const char* txt = cfg->options[i] ? cfg->options[i] : "";
                        CLAY_TEXT(
                            dd_cstr_(txt),
                            CLAY_TEXT_CONFIG((Clay_TextElementConfig){
                                .fontSize = 15,
                                .textColor = (Clay_Color){ 235, 235, 235, 255 },
                            })
                        );
                    }
                }
            }
        }
    }
}

bool dropdown_handle_input(App_State* app, const Dropdown_Config* cfg, Dropdown_State* st) {
    if (!app || !cfg || !st || !cfg->id_str) return false;
    if (!cfg->options || cfg->option_count <= 0) return false;

    bool changed = false;

    char buf[128];
    Clay_ElementId id_trigger = dd_id_trigger_(cfg, buf, sizeof(buf));

    const bool pressed  = app->in.mouse_pressed;
    const bool released = app->in.mouse_released;

    // Click-away closes (only if no one else captured the press)
    if (pressed && st->isOpen && !app->has_active) {
        const bool over_trigger = Clay_PointerOver(id_trigger);
        const int over_idx = dd_over_option_index_(cfg);
        if (!over_trigger && over_idx < 0) {
            st->isOpen = false;
        }
    }

    // Capture on press
    if (pressed && !app->has_active) {
        if (Clay_PointerOver(id_trigger)) {
            app->active_id = id_trigger;
            app->has_active = true;
        } else if (st->isOpen) {
            const int over_idx = dd_over_option_index_(cfg);
            if (over_idx >= 0) {
                Clay_ElementId id_opt = dd_id_option_(cfg, over_idx, buf, sizeof(buf));
                app->active_id = id_opt;
                app->has_active = true;
            }
        }
    }

    // Commit on release
    if (released && app->has_active && dd_id_is_ours_(cfg, app->active_id)) {
        if (Clay_PointerOver(app->active_id)) {
            // Trigger toggles open/close
            if (app_id_equal(app->active_id, id_trigger)) {
                st->isOpen = !st->isOpen;
            } else {
                // Option selection
                for (int i = 0; i < cfg->option_count; i++) {
                    Clay_ElementId id_opt = dd_id_option_(cfg, i, buf, sizeof(buf));
                    if (app_id_equal(app->active_id, id_opt)) {
                        if (st->selectedIndex != i) {
                            st->selectedIndex = i;
                            changed = true;
                        }
                        st->isOpen = false;
                        break;
                    }
                }
            }
        }

        // Release capture
        app->has_active = false;
        app->active_id = app_null_id();
    }

    return changed;
}
