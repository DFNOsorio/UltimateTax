#include "ray_general.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>


void ray_draw_text_in_rect(Rectangle r, const char *text, uint16_t font_size, uint16_t padding, Color color) {
    int x = (int) r.x + (int) padding;
    int y = (int) r.y + (int) ((r.height - (int) font_size) * 0.5f);

    DrawText(text ? text : "", x, y, (int) font_size, color);
}

void ray_draw_text_in_rect_ex(Rectangle r, const char *text, Font font, float font_size, int padding, Color color) {
    int x = (int) r.x + (int) padding;
    int y = (int) r.y + (int) ((r.height - (int) font_size) * 0.5f);

    DrawTextEx(font, text, (Vector2){x,y}, font_size, 0.0f, color);
}

static void GetCurrencyCodepoints(int* codepoints, int* count)
{
    int n = 0;

    // 1. Basic ASCII (A-Z, a-z, 0-9, punctuation, and '$')
    for (int i = 32; i <= 126; i++) {
        codepoints[n++] = i;
    }

    // 2. Add extra Currency Symbols
    codepoints[n++] = 0x20AC; // € (Euro)
    codepoints[n++] = 0x00A3; // £ (Pound Sterling)
    codepoints[n++] = 0x00A5; // ¥ (Yen / Yuan)
    codepoints[n++] = 0x20B9; // ₹ (Indian Rupee)
    codepoints[n++] = 0x20BD; // ₽ (Russian Ruble)
    codepoints[n++] = 0x20A9; // ₩ (Korean Won)
    codepoints[n++] = 0x20BA; // ₺ (Turkish Lira)
    codepoints[n++] = 0x20BF; // ₿ (Bitcoin)
    codepoints[n++] = 0x00A2; // ¢ (Cent)

    *count = n;
}

static char* dup_env_var(const char* name)
{
#if defined(_WIN32) && defined(_MSC_VER)
    char* value = NULL;
    size_t len = 0;
    if (_dupenv_s(&value, &len, name) != 0 || value == NULL) {
        return NULL;
    }
    return value; // caller must free(value)
#else
    const char* v = getenv(name);
    if (!v) return NULL;
    // duplicate so caller can free consistently
    size_t n = strlen(v);
    char* out = (char*)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, v, n + 1);
    return out;
#endif
}

Font ray_load_system_font_or_default(const char* fontFileName, int fontSize)
{
    char* windir = dup_env_var("WINDIR"); // e.g. C:\Windows
    if (windir && fontFileName) {
        char path[512];
        // Prefer forward slashes for raylib compatibility
#if defined(_MSC_VER)
        snprintf(path, sizeof(path), "%s/Fonts/%s", windir, fontFileName);
#else
        snprintf(path, sizeof(path), "%s/Fonts/%s", windir, fontFileName);
#endif
        free(windir);

        if (FileExists(path)) {
            int codepoints[512];
            int count = 0;
            GetCurrencyCodepoints(codepoints, &count);
            return LoadFontEx(path, fontSize, codepoints, count);
        }
    } else if (windir) {
        free(windir);
    }

    return GetFontDefault();
}
