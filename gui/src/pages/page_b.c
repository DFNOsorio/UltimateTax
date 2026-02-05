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
