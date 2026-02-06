#pragma once
#include <stdint.h>
#include "app.h" // Needed for Clay types

// Persistent state (store in your Page State)
typedef struct Dropdown_State {
    int32_t selectedIndex; // 4 bytes
    uint8_t isOpen;        // 1 byte
    uint8_t _pad[3];       // explicit padding -> total 8 bytes, predictable layout
} Dropdown_State;

// Configuration (construct on the stack each frame)
typedef struct Dropdown_Config {
    const char* id_str;       // 8
    const char** options;     // 8
    const char* label_prefix; // 8
    int32_t option_count;     // 4
    int32_t _pad;             // 4 (keeps 8-byte alignment explicit) -> total 32 bytes
} Dropdown_Config;

// Renders the component layout
void dropdown_build(const Dropdown_Config* cfg, Dropdown_State* state);

// Handles input logic. Returns true if the selection changed.
bool dropdown_handle_input(App_State* app, const Dropdown_Config* cfg, Dropdown_State* state);

// Initializes state with a default selection and open/closed state.
// Does NOT depend on cfg/options (safe to call at page init time).
void dropdown_init_state(Dropdown_State* state, int32_t default_selected_index, bool start_open);

// Clamps state to a valid selection range given the current config.
// Call this if your option list/count changes over time.
void dropdown_clamp_state(const Dropdown_Config* cfg, Dropdown_State* state);
