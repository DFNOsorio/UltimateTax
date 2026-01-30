#ifndef TRADE_TABLE_H
#define TRADE_TABLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h> // size_t
#include <stdbool.h>

// One row in the table
typedef struct TradeRow {
    char ticker[32];
    char country[8];

    char sale_datetime[32];
    double sale_value;

    char purchase_datetime[32];
    double purchase_amount;

    double commissions;
    double pl; // profit/loss
} TradeRow;

// Table container
typedef struct TradeTable {
    TradeRow* rows;
    size_t count;
    size_t capacity;

    // UI state (optional)
    int scroll_row;     // first visible row index
    int selected_row;   // -1 if none
} TradeTable;

// Lifecycle
void trade_table_init(TradeTable* t);
void trade_table_free(TradeTable* t);
void trade_table_clear(TradeTable* t);

// Data
bool trade_table_add(TradeTable* t, const TradeRow* row);
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
);

// Rendering (raylib)
struct Rectangle; // forward declare to avoid raylib.h in header
void trade_table_draw(
    const TradeTable* t,
    struct Rectangle bounds,
    int font_size,
    int row_height,
    int header_height
);

// Basic interaction helper (mouse wheel scrolling)
void trade_table_handle_scroll(TradeTable* t, int wheel_move, int visible_rows);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // TRADE_TABLE_H
