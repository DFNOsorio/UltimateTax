#include "trade_table.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "raylib.h"

// ----- internal helpers -----

static void copy_cstr(char* dst, size_t dst_sz, const char* src) {
    if (!dst || dst_sz == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
#if defined(_MSC_VER)
    strncpy_s(dst, dst_sz, src, _TRUNCATE);
#else
    strncpy(dst, src, dst_sz - 1);
    dst[dst_sz - 1] = '\0';
#endif
}

static bool ensure_capacity(TradeTable* t, size_t need) {
    if (t->capacity >= need) return true;

    size_t new_cap = (t->capacity == 0) ? 16 : t->capacity;
    while (new_cap < need) new_cap *= 2;

    TradeRow* p = (TradeRow*)realloc(t->rows, new_cap * sizeof(TradeRow));
    if (!p) return false;

    t->rows = p;
    t->capacity = new_cap;
    return true;
}

static void draw_cell_text(Rectangle r, const char* text, int font_size, Color color) {
    const int pad = 6;
    int x = (int)r.x + pad;
    int y = (int)r.y + (int)(r.height - font_size) / 2;
    DrawText(text ? text : "", x, y, font_size, color);
}

// ----- public API -----

void trade_table_init(TradeTable* t) {
    if (!t) return;
    t->rows = NULL;
    t->count = 0;
    t->capacity = 0;
    t->scroll_row = 0;
    t->selected_row = -1;
}

void trade_table_free(TradeTable* t) {
    if (!t) return;
    free(t->rows);
    t->rows = NULL;
    t->count = 0;
    t->capacity = 0;
    t->scroll_row = 0;
    t->selected_row = -1;
}

void trade_table_clear(TradeTable* t) {
    if (!t) return;
    t->count = 0;
    t->scroll_row = 0;
    t->selected_row = -1;
}

bool trade_table_add(TradeTable* t, const TradeRow* row) {
    if (!t || !row) return false;
    if (!ensure_capacity(t, t->count + 1)) return false;
    t->rows[t->count++] = *row;
    return true;
}

bool trade_table_add_fields(
    TradeTable* t,
    const char* ticker,
    const char* country,
    const char* sale_datetime,
    double sale_value,
    const char* purchase_datetime,
    double purchase_amount,
    double commissions,
    double pl
) {
    if (!t) return false;

    TradeRow r;
    memset(&r, 0, sizeof(r));

    copy_cstr(r.ticker, sizeof(r.ticker), ticker);
    copy_cstr(r.country, sizeof(r.country), country);
    copy_cstr(r.sale_datetime, sizeof(r.sale_datetime), sale_datetime);
    copy_cstr(r.purchase_datetime, sizeof(r.purchase_datetime), purchase_datetime);

    r.sale_value = sale_value;
    r.purchase_amount = purchase_amount;
    r.commissions = commissions;
    r.pl = pl;

    return trade_table_add(t, &r);
}

void trade_table_handle_scroll(TradeTable* t, int wheel_move, int visible_rows) {
    if (!t) return;

    if (visible_rows < 1) visible_rows = 1;

    // wheel_move: positive means scroll up in raylib
    t->scroll_row -= wheel_move;

    int max_scroll = 0;
    if ((int)t->count > visible_rows) {
        max_scroll = (int)t->count - visible_rows;
    }

    if (t->scroll_row < 0) t->scroll_row = 0;
    if (t->scroll_row > max_scroll) t->scroll_row = max_scroll;
}

void trade_table_draw(
    const TradeTable* t,
    Rectangle bounds,
    int font_size,
    int row_height,
    int header_height
) {
    if (!t) return;

    // Columns (fixed widths for now)
    const char* headers[] = {
        "Ticker", "Country", "Sale datetime", "Sale value",
        "Purchase datetime", "Purchase amount", "Commissions", "P/L"
    };

    // You can tweak these widths to your taste
    const int col_w[] = {
        110, 80, 170, 110,
        170, 140, 120, 90
    };

    const int col_count = (int)(sizeof(col_w) / sizeof(col_w[0]));

    // Background + border
    DrawRectangleRec(bounds, (Color){ 245, 245, 245, 255 });
    DrawRectangleLinesEx(bounds, 1, (Color){ 60, 60, 60, 255 });

    // Header area
    Rectangle header = bounds;
    header.height = (float)header_height;
    DrawRectangleRec(header, (Color){ 230, 230, 230, 255 });
    DrawLine((int)bounds.x, (int)(bounds.y + header.height),
             (int)(bounds.x + bounds.width), (int)(bounds.y + header.height),
             (Color){ 60, 60, 60, 255 });

    // Calculate visible rows
    float body_y = bounds.y + header.height;
    float body_h = bounds.height - header.height;
    int visible_rows = (row_height > 0) ? (int)(body_h / row_height) : 0;
    if (visible_rows < 0) visible_rows = 0;

    // Draw columns + header text
    float x = bounds.x;
    for (int c = 0; c < col_count; ++c) {
        Rectangle cell = { x, bounds.y, (float)col_w[c], (float)header_height };
        DrawRectangleLinesEx(cell, 1, (Color){ 200, 200, 200, 255 });
        draw_cell_text(cell, headers[c], font_size, (Color){ 20, 20, 20, 255 });
        x += col_w[c];
    }

    // Rows
    int start = t->scroll_row;
    int end = start + visible_rows;
    if (end > (int)t->count) end = (int)t->count;

    for (int i = start; i < end; ++i) {
        float row_y = body_y + (float)((i - start) * row_height);
        Rectangle row_rect = { bounds.x, row_y, bounds.width, (float)row_height };

        // Alternating row fill
        if (((i - start) & 1) == 1) {
            DrawRectangleRec(row_rect, (Color){ 250, 250, 250, 255 });
        }

        // Selected highlight
        if (t->selected_row == i) {
            DrawRectangleRec(row_rect, (Color){ 210, 235, 255, 255 });
        }

        // Per-cell text
        char buf[128];

        x = bounds.x;

        // 1) Ticker
        Rectangle c0 = { x, row_y, (float)col_w[0], (float)row_height };
        DrawRectangleLinesEx(c0, 1, (Color){ 235, 235, 235, 255 });
        draw_cell_text(c0, t->rows[i].ticker, font_size, (Color){ 30, 30, 30, 255 });
        x += col_w[0];

        // 2) Country
        Rectangle c1 = { x, row_y, (float)col_w[1], (float)row_height };
        DrawRectangleLinesEx(c1, 1, (Color){ 235, 235, 235, 255 });
        draw_cell_text(c1, t->rows[i].country, font_size, (Color){ 30, 30, 30, 255 });
        x += col_w[1];

        // 3) Sale datetime
        Rectangle c2 = { x, row_y, (float)col_w[2], (float)row_height };
        DrawRectangleLinesEx(c2, 1, (Color){ 235, 235, 235, 255 });
        draw_cell_text(c2, t->rows[i].sale_datetime, font_size, (Color){ 30, 30, 30, 255 });
        x += col_w[2];

        // 4) Sale value
        Rectangle c3 = { x, row_y, (float)col_w[3], (float)row_height };
        DrawRectangleLinesEx(c3, 1, (Color){ 235, 235, 235, 255 });
#if defined(_MSC_VER)
        sprintf_s(buf, sizeof(buf), "%.2f", t->rows[i].sale_value);
#else
        snprintf(buf, sizeof(buf), "%.2f", t->rows[i].sale_value);
#endif
        draw_cell_text(c3, buf, font_size, (Color){ 30, 30, 30, 255 });
        x += col_w[3];

        // 5) Purchase datetime
        Rectangle c4 = { x, row_y, (float)col_w[4], (float)row_height };
        DrawRectangleLinesEx(c4, 1, (Color){ 235, 235, 235, 255 });
        draw_cell_text(c4, t->rows[i].purchase_datetime, font_size, (Color){ 30, 30, 30, 255 });
        x += col_w[4];

        // 6) Purchase amount
        Rectangle c5 = { x, row_y, (float)col_w[5], (float)row_height };
        DrawRectangleLinesEx(c5, 1, (Color){ 235, 235, 235, 255 });
#if defined(_MSC_VER)
        sprintf_s(buf, sizeof(buf), "%.2f", t->rows[i].purchase_amount);
#else
        snprintf(buf, sizeof(buf), "%.2f", t->rows[i].purchase_amount);
#endif
        draw_cell_text(c5, buf, font_size, (Color){ 30, 30, 30, 255 });
        x += col_w[5];

        // 7) Commissions
        Rectangle c6 = { x, row_y, (float)col_w[6], (float)row_height };
        DrawRectangleLinesEx(c6, 1, (Color){ 235, 235, 235, 255 });
#if defined(_MSC_VER)
        sprintf_s(buf, sizeof(buf), "%.2f", t->rows[i].commissions);
#else
        snprintf(buf, sizeof(buf), "%.2f", t->rows[i].commissions);
#endif
        draw_cell_text(c6, buf, font_size, (Color){ 30, 30, 30, 255 });
        x += col_w[6];

        // 8) P/L (colorized)
        Rectangle c7 = { x, row_y, (float)col_w[7], (float)row_height };
        DrawRectangleLinesEx(c7, 1, (Color){ 235, 235, 235, 255 });
#if defined(_MSC_VER)
        sprintf_s(buf, sizeof(buf), "%.2f", t->rows[i].pl);
#else
        snprintf(buf, sizeof(buf), "%.2f", t->rows[i].pl);
#endif
        Color plColor = (t->rows[i].pl >= 0.0) ? (Color){ 20, 120, 20, 255 } : (Color){ 160, 20, 20, 255 };
        draw_cell_text(c7, buf, font_size, plColor);
    }

    // Footer hint
    if (visible_rows > 0 && t->count > (size_t)visible_rows) {
        char hint[128];
#if defined(_MSC_VER)
        sprintf_s(hint, sizeof(hint), "Scroll: %d / %d", t->scroll_row, (int)t->count - visible_rows);
#else
        snprintf(hint, sizeof(hint), "Scroll: %d / %d", t->scroll_row, (int)t->count - visible_rows);
#endif
        DrawText(hint, (int)bounds.x + 6, (int)(bounds.y + bounds.height) - font_size - 4, font_size, (Color){ 80, 80, 80, 255 });
    }
}
