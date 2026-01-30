#include "raylib.h"
#include <stdint.h>

void ray_draw_text_in_rect(Rectangle r, const char* text, uint16_t font_size, uint16_t padding, Color color);
void ray_draw_text_in_rect_ex(Rectangle r, const char* text, Font font, float font_size, int padding, Color color);
Font ray_load_system_font_or_default(const char* fontFileName, int fontSize);
