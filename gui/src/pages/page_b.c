#include "app.h"
#include "pages.h"

void page_b_build(void) {
    CLAY_TEXT(
        CLAY_STRING("Page B"),
        CLAY_TEXT_CONFIG((Clay_TextElementConfig){
            .fontSize = 28,
            .textColor = (Clay_Color){ 245, 245, 245, 255 },
        })
    );
}

void page_b_handle_input(App_State* app) {
    (void)app;
    // No interactive widgets on Page B yet.
}
