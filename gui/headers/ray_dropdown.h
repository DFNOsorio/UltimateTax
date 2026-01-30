#ifndef DROPDOWN_H
#define DROPDOWN_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "raylib.h"

typedef struct rayDropDownStyle {
    Color bg, border, text, selection, hover;

    float roundness, anim_seconds;
    uint16_t padding, font_size, header_height, item_height, list_gap;

    uint8_t max_visible_items, border_thickness, round_segments;
    uint8_t _dummy;

    Font font;
    uint8_t use_custom_font;
    uint8_t _dummy2[3];
} rayDropDownStyle;

rayDropDownStyle ray_dropdown_style_default(void);
rayDropDownStyle ray_dropdown_style_with_colors(Color bg, Color border, Color selection);

void ray_dropdown_style_change_font(rayDropDownStyle* d, Font font);

typedef struct rayDropdown {
    Rectangle bounds;

    const char* const* items;
    const char* placeholder;

    int32_t count, selected, hovered, scroll;
    float open_t;
    uint8_t expanded, _pad[3];

    rayDropDownStyle style;
} rayDropdown;

void ray_dropdown_init(
    rayDropdown* d,
    Rectangle bounds,
    const char* placeholder,
    const char* const* items,
    int32_t count
);

void ray_dropdown_init_ex(
    rayDropdown* d,
    Rectangle bounds,
    const char* placeholder,
    const char* const* items,
    int32_t count,
    const rayDropDownStyle* style_or_null
);

void ray_dropdow_set_items(rayDropdown* d, const char** items, int count);

bool ray_dropdown_update(rayDropdown* d);

void ray_dropdown_draw(const rayDropdown* d);

const char* ray_dropdown_selected_text(const rayDropdown* d);

int32_t ray_dropdown_selected_index(const rayDropdown* d);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DROPDOWN_H
