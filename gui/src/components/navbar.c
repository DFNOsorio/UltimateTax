#include "navbar.h"

Navbar_Result navbar_build(bool nav_open) {
    Navbar_Result out;
    out.id_nav_a = Clay_GetElementId(CLAY_STRING("NavA"));
    out.id_nav_b = Clay_GetElementId(CLAY_STRING("NavB"));

    const float nav_w = nav_open ? 220.0f : 60.0f;

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
            nav_open ? CLAY_STRING("NAV") : CLAY_STRING("N"),
            CLAY_TEXT_CONFIG((Clay_TextElementConfig){
                .fontSize = 22,
                .textColor = (Clay_Color){ 240, 240, 245, 255 },
            })
        );

        const bool hover_a = Clay_PointerOver(out.id_nav_a);
        CLAY(out.id_nav_a, (Clay_ElementDeclaration){
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(44) },
                .padding = { 10, 10, 10, 10 },
            },
            .backgroundColor = hover_a ? (Clay_Color){ 70, 70, 80, 255 } : (Clay_Color){ 55, 55, 65, 255 },
        }) {
            CLAY_TEXT(
                nav_open ? CLAY_STRING("Page A") : CLAY_STRING("A"),
                CLAY_TEXT_CONFIG((Clay_TextElementConfig){
                    .fontSize = 18,
                    .textColor = (Clay_Color){ 255, 255, 255, 255 },
                })
            );
        }

        const bool hover_b = Clay_PointerOver(out.id_nav_b);
        CLAY(out.id_nav_b, (Clay_ElementDeclaration){
            .layout = {
                .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(44) },
                .padding = { 10, 10, 10, 10 },
            },
            .backgroundColor = hover_b ? (Clay_Color){ 70, 70, 80, 255 } : (Clay_Color){ 55, 55, 65, 255 },
        }) {
            CLAY_TEXT(
                nav_open ? CLAY_STRING("Page B") : CLAY_STRING("B"),
                CLAY_TEXT_CONFIG((Clay_TextElementConfig){
                    .fontSize = 18,
                    .textColor = (Clay_Color){ 255, 255, 255, 255 },
                })
            );
        }
    }

    return out;
}
