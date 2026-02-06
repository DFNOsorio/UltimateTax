#pragma once

#include "app.h"

void page_a_build(void);
void page_b_build(void);

// Per-page input (press-capture + release-trigger) and UI interactions.
// Called from the global click handler so pages can own their own widgets.
void page_a_handle_input(App_State* app);
void page_b_handle_input(App_State* app);
