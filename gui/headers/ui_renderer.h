#pragma once
#include "app.h"

typedef struct UiRenderer_Config {
    // Optional: override font path (Windows). If NULL, renderer will try system UI font.
    const char* override_font_path;
} UiRenderer_Config;

typedef struct UiRenderer {
    FONScontext* fons;
    int font_ui;
} UiRenderer;

bool ui_renderer_init(UiRenderer* r, const UiRenderer_Config* cfg);
void ui_renderer_shutdown(UiRenderer* r);

// Clay text measure callback (uses fontstash)
Clay_Dimensions ui_renderer_measure_text(Clay_StringSlice text, Clay_TextElementConfig* cfg, void* userData);

// Render Clay commands (rectangles + text)
void ui_renderer_render_clay(UiRenderer* r, const Clay_RenderCommandArray cmds);

// Flush fontstash draws (call once per frame after render_clay)
void ui_renderer_flush(UiRenderer* r);

// (Optional) quick debug overlay
void ui_renderer_debug_overlay(UiRenderer* r, float x, float y, const char* line1, const char* line2);
