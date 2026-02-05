#include "ray_pages.h"
#include "raylib.h"

AppLayout ray_compute_layout(int screenW, int screenH, float navWidth, float gap) {
    AppLayout L = {0};

    L.navWidth = navWidth;
    L.gap = gap;

    L.nav = (Rectangle){ 0.0f, 0.0f, navWidth, (float)screenH };

    float contentX = navWidth + gap;
    float contentW =(float)screenW - contentX;
    if (contentW < 0.0f) contentW = 0.0f;

    L.content = (Rectangle){ contentX, 0.0f, contentW, (float)screenH };

    return L;
}
