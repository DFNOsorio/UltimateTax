#include "ray_dropdown.h"
#include "ray_general.h"
#include "raylib.h"

#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

// ----------------------------
// Default style constants
// ----------------------------
#define RDD_BG_R 245
#define RDD_BG_G 246
#define RDD_BG_B 248
#define RDD_BG_A 255

#define RDD_BORDER_R 170
#define RDD_BORDER_G 180
#define RDD_BORDER_B 200
#define RDD_BORDER_A 255

#define RDD_TEXT_R 30
#define RDD_TEXT_G 30
#define RDD_TEXT_B 30
#define RDD_TEXT_A 255

#define RDD_SEL_R 60
#define RDD_SEL_G 130
#define RDD_SEL_B 220
#define RDD_SEL_A 255

#define RDD_HOVER_R 220
#define RDD_HOVER_G 235
#define RDD_HOVER_B 255
#define RDD_HOVER_A 255

#define RDD_ROUNDNESS        0.20f
#define RDD_PADDING          12
#define RDD_FONT_SIZE        18
#define RDD_HEADER_HEIGHT    44
#define RDD_ITEM_HEIGHT      40
#define RDD_MAX_VISIBLE      6
#define RDD_BORDER_THICKNESS 2
#define RDD_ROUND_SEGMENTS   8
#define RDD_LIST_GAP         12

#define RDD_ANIM_SECONDS      0.12f  // open/close duration

// ----------------------------
// Small helpers
// ----------------------------
static int32_t clampi(int32_t v, int32_t lo, int32_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float clamp01(float t) {
    if (t < 0.0f) return 0.0f;
    if (t > 1.0f) return 1.0f;
    return t;
}

static float smoothstep(float t) {
    t = clamp01(t);
    return t * t * (3.0f - 2.0f * t);
}

static float approachf(float cur, float target, float max_delta) {
    if (cur < target) {
        cur += max_delta;
        if (cur > target) cur = target;
    } else if (cur > target) {
        cur -= max_delta;
        if (cur < target) cur = target;
    }
    return cur;
}

// Convert header roundness to consistent pixel radius across differently-sized rects
static float roundness_for_rect(const rayDropdown* d, Rectangle r) {
    float base_min = fminf(d->bounds.width, d->bounds.height);
    float radius_px = d->style.roundness * base_min;

    float min_dim = fminf(r.width, r.height);
    if (min_dim <= 0.0f) return 0.0f;

    float rr = radius_px / min_dim;  // raylib expects 0..1
    if (rr < 0.0f) rr = 0.0f;
    if (rr > 1.0f) rr = 1.0f;
    return rr;
}

static void reset_selected(rayDropdown* d) {
    if (d->selected >= d->count) d->selected = d->count - 1;
    if (d->selected < -1) d->selected = -1;
}

static void fix_selection_and_scroll(rayDropdown* d) {
    if (d->count <= 0) {
        d->selected = -1;
        d->hovered = -1;
        d->scroll = 0;
        return;
    }

    reset_selected(d);

    int32_t max_scroll = 0;
    if (d->count > (int32_t)d->style.max_visible_items) {
        max_scroll = d->count - (int32_t)d->style.max_visible_items;
    }
    d->scroll = clampi(d->scroll, 0, max_scroll);
}

static Rectangle dropdown_list_full_bounds(const rayDropdown* d) {
    int32_t visible = d->count;
    if (visible > (int32_t)d->style.max_visible_items) visible = (int32_t)d->style.max_visible_items;
    if (visible < 0) visible = 0;

    Rectangle r = d->bounds;
    r.y = d->bounds.y + d->bounds.height + (float)d->style.list_gap;
    r.height = (float)(visible * (int32_t)d->style.item_height);
    return r;
}

static Vector2 rotate_point(Vector2 p, Vector2 c, float ang) {
    float s = sinf(ang);
    float co = cosf(ang);
    Vector2 v = { p.x - c.x, p.y - c.y };
    Vector2 out = {
        c.x + v.x * co - v.y * s,
        c.y + v.x * s + v.y * co
    };
    return out;
}

// Draw a triangle arrow that rotates smoothly with t in [0..1]
// t=0 => down, t=1 => up (rotate by PI)
static void draw_rotating_arrow(float ax, float ay, float w, float h, float t, Color col) {
    float half_w = w * 0.5f;
    float half_h = h * 0.5f;

    // Base "down" triangle (clockwise)
    Vector2 p0 = { ax,          ay - half_h };
    Vector2 p1 = { ax + half_w, ay + half_h };
    Vector2 p2 = { ax + w,      ay - half_h };

    Vector2 c = { ax + half_w, ay };

    // Use eased t for nicer rotation timing
    float tt = smoothstep(t);
    float ang = (float)M_PI * tt;

    p0 = rotate_point(p0, c, ang);
    p1 = rotate_point(p1, c, ang);
    p2 = rotate_point(p2, c, ang);

    DrawTriangle(p0, p1, p2, col);
}

static void draw_text_in_rect(const rayDropDownStyle* s, Rectangle r, const char* text, Color c) {
    if (s->use_custom_font) {
        ray_draw_text_in_rect_ex(r, text, s->font, (float)s->font_size, (int)s->padding, c);
    } else {
        ray_draw_text_in_rect(r, text, (int)s->font_size, (int)s->padding, c);
    }
}

// ----------------------------
// Style API
// ----------------------------
rayDropDownStyle ray_dropdown_style_default(void) {
    rayDropDownStyle s;

    s.bg        = (Color){ RDD_BG_R, RDD_BG_G, RDD_BG_B, RDD_BG_A };
    s.border    = (Color){ RDD_BORDER_R, RDD_BORDER_G, RDD_BORDER_B, RDD_BORDER_A };
    s.text      = (Color){ RDD_TEXT_R, RDD_TEXT_G, RDD_TEXT_B, RDD_TEXT_A };
    s.selection = (Color){ RDD_SEL_R, RDD_SEL_G, RDD_SEL_B, RDD_SEL_A };
    s.hover     = (Color){ RDD_HOVER_R, RDD_HOVER_G, RDD_HOVER_B, RDD_HOVER_A };

    s.roundness = RDD_ROUNDNESS;

    s.padding       = RDD_PADDING;
    s.font_size     = RDD_FONT_SIZE;
    s.header_height = RDD_HEADER_HEIGHT;
    s.item_height   = RDD_ITEM_HEIGHT;
    s.list_gap      = RDD_LIST_GAP;

    s.max_visible_items = RDD_MAX_VISIBLE;
    s.border_thickness  = RDD_BORDER_THICKNESS;
    s.round_segments    = RDD_ROUND_SEGMENTS;

    s.use_custom_font = 0;
    s.font = GetFontDefault();

    s.anim_seconds = RDD_ANIM_SECONDS;

    return s;
}

rayDropDownStyle ray_dropdown_style_with_colors(Color bg, Color border, Color selection) {
    rayDropDownStyle s = ray_dropdown_style_default();
    s.bg = bg;
    s.border = border;
    s.selection = selection;
    return s;
}

void ray_dropdown_style_change_font(rayDropDownStyle* s, Font font) {
    if (!s) return;
    s->font = font;
    s->use_custom_font = 1;
}

// ----------------------------
// Component API
// ----------------------------
void ray_dropdown_init(rayDropdown* d, Rectangle bounds, const char* placeholder,
                      const char* const* items, int32_t count)
{
    ray_dropdown_init_ex(d, bounds, placeholder, items, count, NULL);
}

void ray_dropdown_init_ex(rayDropdown* d, Rectangle bounds, const char* placeholder,
                         const char* const* items, int32_t count,
                         const rayDropDownStyle* style_or_null)
{
    memset(d, 0, sizeof(*d));

    d->bounds = bounds;
    d->placeholder = placeholder ? placeholder : "Select...";
    d->items = items;
    d->count = count;

    d->selected = -1;
    d->hovered = -1;
    d->scroll = 0;

    d->expanded = 0; // start closed by default
    d->style = style_or_null ? *style_or_null : ray_dropdown_style_default();

    // Enforce header height from style
    d->bounds.height = (float)d->style.header_height;

    // Animation state
    d->open_t = d->expanded ? 1.0f : 0.0f;

    fix_selection_and_scroll(d);
}

void ray_dropdown_set_items(rayDropdown* d, const char* const* items, int32_t count) {
    if (!d) return;
    d->items = items;
    d->count = count;
    reset_selected(d);
    fix_selection_and_scroll(d);
}

const char* ray_dropdown_selected_text(const rayDropdown* d) {
    if (!d || d->selected < 0 || d->selected >= d->count) return NULL;
    return d->items[d->selected];
}

int32_t ray_dropdown_selected_index(const rayDropdown* d) {
    if (!d) return -1;
    return d->selected;
}

bool ray_dropdown_update(rayDropdown* d) {
    if (!d) return false;

    fix_selection_and_scroll(d);

    // Animate open_t towards target (expanded)
    {
        float dt = GetFrameTime();
        float target = d->expanded ? 1.0f : 0.0f;

        float max_step = 1.0f;
        if (d->style.anim_seconds > 0.0f) {
            max_step = dt / d->style.anim_seconds;
        }

        d->open_t = approachf(d->open_t, target, max_step);
        d->open_t = clamp01(d->open_t);
    }

    bool changed = false;
    Vector2 m = GetMousePosition();

    Rectangle header = d->bounds;
    bool point_in_header = CheckCollisionPointRec(m, header);
    bool left_pressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

    // Toggle target state via header click
    if (left_pressed && point_in_header) {
        d->expanded = (uint8_t)!d->expanded;
        d->hovered = -1;
        return false;
    }

    // List rectangles (full + animated visible)
    Rectangle list_full = dropdown_list_full_bounds(d);
    float t = smoothstep(d->open_t);

    Rectangle list_vis = list_full;
    list_vis.height *= t;

    bool list_visible = (d->open_t > 0.001f);
    bool allow_interaction = (d->expanded && d->open_t > 0.95f);

    if (list_visible) {
        bool point_in_list = CheckCollisionPointRec(m, list_vis);

        // Click outside closes (target close)
        if (left_pressed && !point_in_header && !point_in_list) {
            d->expanded = 0;
            d->hovered = -1;
        }

        // Only interact (hover/select/scroll) when nearly fully open
        if (allow_interaction) {
            int wheel = (int)GetMouseWheelMove();
            if (wheel && point_in_list && d->count > (int32_t)d->style.max_visible_items) {
                int32_t max_scroll = d->count - (int32_t)d->style.max_visible_items;
                d->scroll = clampi(d->scroll - wheel, 0, max_scroll);
            }

            d->hovered = -1;
            if (point_in_list) {
                float rel_y = m.y - list_full.y; // row math uses full list origin
                int32_t idx_in_view = (int32_t)(rel_y / (float)d->style.item_height);

                int32_t visible = d->count;
                if (visible > (int32_t)d->style.max_visible_items) visible = (int32_t)d->style.max_visible_items;

                if (idx_in_view >= 0 && idx_in_view < visible) {
                    int32_t abs_idx = d->scroll + idx_in_view;
                    if (abs_idx >= 0 && abs_idx < d->count) {
                        d->hovered = abs_idx;

                        if (left_pressed) {
                            d->selected = abs_idx;
                            d->expanded = 0; // target close
                            d->hovered = -1;
                            changed = true;
                        }
                    }
                }
            }
        }
    } else {
        d->hovered = -1;
    }

    return changed;
}

void ray_dropdown_draw(const rayDropdown* d) {
    if (!d) return;

    Rectangle header = d->bounds;
    float rr_header = roundness_for_rect(d, header);

    DrawRectangleRounded(header, rr_header, (int)d->style.round_segments, d->style.bg);
    DrawRectangleRoundedLinesEx(header, rr_header, (int)d->style.round_segments,
                               (float)d->style.border_thickness, d->style.border);

    const char* txt = ray_dropdown_selected_text(d);
    if (!txt) txt = d->placeholder;

    draw_text_in_rect(&d->style, header, txt, d->style.text);

    // Arrow rotation based on open_t (polish)
    {
        const float arrow_w = 12.0f;
        const float arrow_h = 8.0f;

        float ax = header.x + header.width - (float)d->style.padding - arrow_w;
        float ay = header.y + header.height * 0.5f;

        draw_rotating_arrow(ax, ay, arrow_w, arrow_h, d->open_t, d->style.text);
    }

    // Animated list draw
    float t = smoothstep(d->open_t);
    if (t <= 0.001f) return;

    Rectangle list_full = dropdown_list_full_bounds(d);
    Rectangle list_vis = list_full;
    list_vis.height *= t;

    // Protect against degenerate scissor sizes
    if (list_vis.height < 1.0f) return;

    float rr_list = roundness_for_rect(d, list_vis);

    DrawRectangleRounded(list_vis, rr_list, (int)d->style.round_segments, d->style.bg);
    DrawRectangleRoundedLinesEx(list_vis, rr_list, (int)d->style.round_segments,
                               (float)d->style.border_thickness, d->style.border);

    // Clip rows to animated height
    BeginScissorMode((int)list_vis.x, (int)list_vis.y, (int)list_vis.width, (int)list_vis.height);

    int32_t visible = d->count;
    if (visible > (int32_t)d->style.max_visible_items) visible = (int32_t)d->style.max_visible_items;

    for (int i = 0; i < visible; ++i) {
        int32_t idx = d->scroll + i;
        if (idx < 0 || idx >= d->count) break;

        Rectangle row = list_full;
        row.y += (float)(i * (int32_t)d->style.item_height);
        row.height = (float)d->style.item_height;

        bool is_top_row = (i == 0);
        bool is_bottom_row = (i == (visible - 1));
        bool want_round = is_top_row || is_bottom_row;

        if (idx == d->selected) {
            if (want_round) {
                float rr_row = roundness_for_rect(d, row);
                DrawRectangleRounded(row, rr_row, (int)d->style.round_segments, d->style.selection);
            } else {
                DrawRectangleRec(row, d->style.selection);
            }
        } else if (idx == d->hovered) {
            if (want_round) {
                float rr_row = roundness_for_rect(d, row);
                DrawRectangleRounded(row, rr_row, (int)d->style.round_segments, d->style.hover);
            } else {
                DrawRectangleRec(row, d->style.hover);
            }
        }

        if (i > 0) {
            DrawLine((int)row.x, (int)row.y, (int)(row.x + row.width), (int)row.y,
                     (Color){ 220, 220, 220, 255 });
        }

        Color tc = (idx == d->selected) ? (Color){ 255, 255, 255, 255 } : d->style.text;
        draw_text_in_rect(&d->style, row, d->items[idx], tc);
    }

    EndScissorMode();
}
