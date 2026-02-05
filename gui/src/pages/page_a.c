#include "app.h"
#include "pages.h"

void page_a_build(void) {
    CLAY_TEXT(
        CLAY_STRING("Page A"),
        CLAY_TEXT_CONFIG((Clay_TextElementConfig){
            .fontSize = 28,
            .textColor = (Clay_Color){ 245, 245, 245, 255 },
        })
    );
}
