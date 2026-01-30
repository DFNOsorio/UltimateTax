#include "raylib.h"
#include "ray_dropdown.h"
#include "ray_general.h"
#include <stdint.h>





int main(void) {
    InitWindow(900, 500, "Dropdown demo");
    SetTargetFPS(60);

    const char* years[] = { "All Years", "2025", "2024", "2023", "2022", "2021", "2020" };

    rayDropDownStyle style = ray_dropdown_style_with_colors(
        (Color){245,246,248,255},   // bg
        (Color){150,170,210,255},   // border
        (Color){60,130,220,255}     // selection
    );

    Font ui = ray_load_system_font_or_default("verdana.ttf", 48);

    SetTextureFilter(ui.texture, TEXTURE_FILTER_BILINEAR);

    ray_dropdown_style_change_font(&style, ui);

    rayDropdown dd;
    ray_dropdown_init_ex(
        &dd,
        (Rectangle){40, 40, 260, 44},
        "Filter by year",
        years,
        (int32_t) (sizeof(years)/sizeof(years[0])),
        &style);


    while (!WindowShouldClose()) {
        ray_dropdown_update(&dd);

        BeginDrawing();
        ClearBackground((Color){ 235, 240, 245, 255 });

        ray_dropdown_draw(&dd);

        EndDrawing();
    }

    UnloadFont(ui);

    CloseWindow();
    return 0;
}

    // int main(void)
    // {
    // const int screenWidth = 1200;
    // const int screenHeight = 700;

    // InitWindow(screenWidth, screenHeight, "ultimateTax - Trades");
    // SetTargetFPS(60);

    // // ----------------------------
    // // Table model
    // // ----------------------------
    // TradeTable table;
    // trade_table_init(&table);

    // // Demo rows (replace later with real data)
    // trade_table_add_fields(&table, "AAPL", "US",
    //     "2026-01-28 10:30", 1950.00,
    //     "2025-12-10 09:15", 1800.00,
    //     1.25, 148.75);

    // trade_table_add_fields(&table, "TSLA", "US",
    //     "2026-01-15 14:05", 900.00,
    //     "2026-01-02 10:00", 950.00,
    //     1.10, -51.10);

    // trade_table_add_fields(&table, "MSFT", "US",
    //     "2026-01-20 11:10", 1500.00,
    //     "2025-11-21 15:45", 1200.00,
    //     1.35, 298.65);

    // // Add more rows to demonstrate scrolling
    // for (int i = 0; i < 40; ++i) {
    //     char ticker[32];
    //     char sale_dt[32];
    //     char buy_dt[32];

    // #if defined(_MSC_VER)
    //     sprintf_s(ticker, sizeof(ticker), "TICK%02d", i);
    //     sprintf_s(sale_dt, sizeof(sale_dt), "2026-01-%02d 12:%02d", (i % 28) + 1, i % 60);
    //     sprintf_s(buy_dt, sizeof(buy_dt), "2025-12-%02d 09:%02d", (i % 28) + 1, (i * 3) % 60);
    // #else
    //     snprintf(ticker, sizeof(ticker), "TICK%02d", i);
    //     snprintf(sale_dt, sizeof(sale_dt), "2026-01-%02d 12:%02d", (i % 28) + 1, i % 60);
    //     snprintf(buy_dt, sizeof(buy_dt), "2025-12-%02d 09:%02d", (i % 28) + 1, (i * 3) % 60);
    // #endif

    //     double sale_value = 1000.0 + i * 25.0;
    //     double purchase_amount = 950.0 + i * 23.0;
    //     double commissions = 1.0;
    //     double pl = sale_value - purchase_amount - commissions;

    //     trade_table_add_fields(&table, ticker, "US",
    //         sale_dt, sale_value,
    //         buy_dt, purchase_amount,
    //         commissions, pl);
    // }

    // // ----------------------------
    // // UI layout constants
    // // ----------------------------
    // const int fontSize = 18;
    // const int rowH = 34;
    // const int headerH = 40;

    // while (!WindowShouldClose())
    // {
    //     // Layout: full window padding
    //     int w = GetScreenWidth();
    //     int h = GetScreenHeight();

    //     Rectangle tableBounds = (Rectangle){ 20, 60, (float)(w - 40), (float)(h - 80) };

    //     // Mouse wheel scroll support
    //     int wheel = GetMouseWheelMove();
    //     int visibleRows = (int)((tableBounds.height - headerH) / rowH);
    //     trade_table_handle_scroll(&table, wheel, visibleRows);

    //     // Optional: row selection on click (simple)
    //     if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    //         Vector2 m = GetMousePosition();
    //         if (CheckCollisionPointRec(m, tableBounds)) {
    //             float bodyY = tableBounds.y + headerH;
    //             if (m.y >= bodyY) {
    //                 int idxInView = (int)((m.y - bodyY) / rowH);
    //                 int clickedIndex = table.scroll_row + idxInView;
    //                 if (clickedIndex >= 0 && clickedIndex < (int)table.count) {
    //                     table.selected_row = clickedIndex;
    //                 }
    //             }
    //         }
    //     }

    //     BeginDrawing();
    //     ClearBackground(RAYWHITE);

    //     // Title bar
    //     DrawText("ultimateTax - Trades", 20, 18, 24, DARKGRAY);
    //     DrawLine(20, 50, w - 20, 50, (Color){ 200, 200, 200, 255 });

    //     // Table
    //     trade_table_draw(&table, tableBounds, fontSize, rowH, headerH);

    //     // Status text
    //     if (table.selected_row >= 0 && table.selected_row < (int)table.count) {
    //         const TradeRow* r = &table.rows[table.selected_row];
    //         char status[256];
    // #if defined(_MSC_VER)
    //         sprintf_s(status, sizeof(status),
    //             "Selected: %s (%s) | Sale: %s | Buy: %s | P/L: %.2f",
    //             r->ticker, r->country, r->sale_datetime, r->purchase_datetime, r->pl);
    // #else
    //         snprintf(status, sizeof(status),
    //             "Selected: %s (%s) | Sale: %s | Buy: %s | P/L: %.2f",
    //             r->ticker, r->country, r->sale_datetime, r->purchase_datetime, r->pl);
    // #endif
    //         DrawText(status, 20, h - 18 - 8, 18, (Color){ 80, 80, 80, 255 });
    //     } else {
    //         DrawText("Mouse wheel to scroll. Click a row to select.", 20, h - 18 - 8, 18, (Color){ 80, 80, 80, 255 });
    //     }

    //     EndDrawing();
    // }

    // trade_table_free(&table);
    // CloseWindow();
    // return 0;
    // }
