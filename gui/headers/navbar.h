#pragma once
#include "app.h"

typedef struct Navbar_Result {
    Clay_ElementId id_nav_a;
    Clay_ElementId id_nav_b;
} Navbar_Result;

Navbar_Result navbar_build(bool nav_open);
