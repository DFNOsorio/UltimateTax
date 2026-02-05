#include "raylib.h"

typedef struct AppLayout {
    Rectangle nav;
    Rectangle content;
    float navWidth, gap;
} AppLayout;

AppLayout ray_compute_layout(int screenW, int screenH, float navWidth, float gap);
