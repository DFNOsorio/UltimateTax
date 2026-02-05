#pragma once
#include "app.h"

typedef struct AppUi_Ids {
    Clay_ElementId id_nav_a;
    Clay_ElementId id_nav_b;
    Clay_ElementId id_toggle;
} AppUi_Ids;

// Build the full UI tree and return render commands + the interactive IDs used this frame.
Clay_RenderCommandArray app_ui_build(App_State* app, AppUi_Ids* out_ids);

// Press-capture + release-trigger click handling.
void app_ui_handle_clicks(App_State* app, const AppUi_Ids* ids);
