#define _CRT_SECURE_NO_WARNINGS

// Backend must be visible to ALL TUs too (we also define it in build flags)
#ifndef SOKOL_D3D11
#define SOKOL_D3D11
#endif

#define SOKOL_IMPL
#define SOKOL_WIN32_FORCE_MAIN
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"

#define SOKOL_GL_IMPL
#include "util/sokol_gl.h"

#define CLAY_IMPLEMENTATION
#include "clay.h"

// IMPORTANT: fontstash includes stb_truetype internally
#define FONTSTASH_IMPLEMENTATION
#include "fontstash.h"

#define SOKOL_FONTSTASH_IMPL
#include "util/sokol_fontstash.h"
