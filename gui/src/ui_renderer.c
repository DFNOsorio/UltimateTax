#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "ui_renderer.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(_WIN32)
static bool file_existsA(const char* p) {
    DWORD a = GetFileAttributesA(p);
    return (a != INVALID_FILE_ATTRIBUTES) && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static void get_windows_ui_font_path(char out_path[MAX_PATH]) {
    char win[MAX_PATH] = {0};
    UINT n = GetWindowsDirectoryA(win, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) { out_path[0] = 0; return; }

    const char* candidates[] = {
        "\\Fonts\\segoeui.ttf",
        "\\Fonts\\SegoeUI.ttf",
        "\\Fonts\\SegoeUIVariable.ttf",
        "\\Fonts\\arial.ttf",
    };

    for (int i = 0; i < (int)(sizeof(candidates)/sizeof(candidates[0])); i++) {
        snprintf(out_path, MAX_PATH, "%s%s", win, candidates[i]);
        if (file_existsA(out_path)) return;
    }
    out_path[0] = 0;
}
#endif

static void draw_rect(float x, float y, float w, float h, Clay_Color c) {
    sgl_c4b((uint8_t)c.r, (uint8_t)c.g, (uint8_t)c.b, (uint8_t)c.a);
    sgl_begin_quads();
    sgl_v2f(x,     y);
    sgl_v2f(x + w, y);
    sgl_v2f(x + w, y + h);
    sgl_v2f(x,     y + h);
    sgl_end();
}

static void fons_draw_slice(FONScontext* fs, int font, float x, float y, Clay_StringSlice s, Clay_Color c, float font_px) {
    if (!fs || font < 0 || s.length <= 0) return;

    // Copy slice -> null terminated
    char stack[512];
    char* tmp = stack;
    int len = (int)s.length;

    if (len >= (int)sizeof(stack)) {
        tmp = (char*)malloc((size_t)len + 1);
        if (!tmp) return;
    }
    memcpy(tmp, s.chars, (size_t)len);
    tmp[len] = 0;

    fonsSetFont(fs, font);
    fonsSetSize(fs, font_px);
    fonsSetColor(fs, sfons_rgba((uint8_t)c.r, (uint8_t)c.g, (uint8_t)c.b, (uint8_t)c.a));
    fonsSetAlign(fs, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
    fonsDrawText(fs, x, y, tmp, NULL);

    if (tmp != stack) free(tmp);
}

bool ui_renderer_init(UiRenderer* r, const UiRenderer_Config* cfg) {
    if (!r) return false;
    memset(r, 0, sizeof(*r));
    r->font_ui = -1;

    // Sokol gfx must be setup in main before calling this.
    // sgl can be set up here.
    sgl_setup(&(sgl_desc_t){});

    // If font atlas creation fails, we keep running (rectangles still draw).
    r->fons = sfons_create(&(sfons_desc_t){ .width = 1024, .height = 1024 });
    if (!r->fons) {
        return true; // no hard fail
    }

#if defined(_WIN32)
    char path[MAX_PATH] = {0};

    if (cfg && cfg->override_font_path && cfg->override_font_path[0]) {
        #if defined(_MSC_VER)
        strncpy_s(path, MAX_PATH, cfg->override_font_path, _TRUNCATE);
        #else
        strncpy(path, cfg->override_font_path, MAX_PATH - 1);
        path[MAX_PATH - 1] = 0;
        #endif
    } else {
        get_windows_ui_font_path(path);
    }

    if (path[0]) {
        r->font_ui = fonsAddFont(r->fons, "ui", path);
    }
#else
    (void)cfg;
#endif

    // OK if font_ui stays -1 (text just won't render, layout still works)
    return true;
}

void ui_renderer_shutdown(UiRenderer* r) {
    if (!r) return;
    if (r->fons) {
        sfons_destroy(r->fons);
        r->fons = NULL;
    }
    sgl_shutdown();
}

Clay_Dimensions ui_renderer_measure_text(Clay_StringSlice text, Clay_TextElementConfig* cfg, void* userData) {
    UiRenderer* r = (UiRenderer*)userData;
    Clay_Dimensions out = {0};

    float font_px = (cfg && cfg->fontSize > 0) ? (float)cfg->fontSize : 16.0f;

    if (!r || !r->fons || r->font_ui < 0 || text.length <= 0) {
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

    fonsSetFont(r->fons, r->font_ui);
    fonsSetSize(r->fons, font_px);
    fonsSetAlign(r->fons, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

    float bounds[4] = {0};
    fonsTextBounds(r->fons, 0.0f, 0.0f, tmp, NULL, bounds);

    out.width  = bounds[2] - bounds[0];
    out.height = font_px;

    if (tmp != stack) free(tmp);
    return out;
}

void ui_renderer_render_clay(UiRenderer* r, const Clay_RenderCommandArray cmds) {
    if (!cmds.internalArray || cmds.length <= 0) return;

    for (int32_t i = 0; i < cmds.length; i++) {
        Clay_RenderCommand* cmd = &cmds.internalArray[i];
        Clay_BoundingBox bb = cmd->boundingBox;

        switch (cmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                Clay_RectangleRenderData* rd = &cmd->renderData.rectangle;
                draw_rect(bb.x, bb.y, bb.width, bb.height, rd->backgroundColor);
            } break;

            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                Clay_TextRenderData* td = &cmd->renderData.text;

                // Your Clay_TextRenderData does NOT have textConfig.
                // The bounding box height is a good font-size proxy in Clay output.
                float font_px = (bb.height > 1.0f) ? bb.height : 16.0f;

                fons_draw_slice(r ? r->fons : NULL,
                                r ? r->font_ui : -1,
                                bb.x, bb.y,
                                td->stringContents, td->textColor,
                                font_px);
            } break;

            default:
                break;
        }
    }
}

void ui_renderer_flush(UiRenderer* r) {
    if (r && r->fons) sfons_flush(r->fons);
    sgl_draw();
}

void ui_renderer_debug_overlay(UiRenderer* r, float x, float y, const char* line1, const char* line2) {
    if (!r || !r->fons || r->font_ui < 0) return;

    Clay_Color c = (Clay_Color){ 255, 255, 255, 220 };
    if (line1) {
        Clay_StringSlice s = { .chars = line1, .length = (int32_t)strlen(line1) };
        fons_draw_slice(r->fons, r->font_ui, x, y, s, c, 18.0f);
        y += 16.0f;
    }
    if (line2) {
        Clay_StringSlice s = { .chars = line2, .length = (int32_t)strlen(line2) };
        fons_draw_slice(r->fons, r->font_ui, x, y, s, c, 18.0f);
    }
}
