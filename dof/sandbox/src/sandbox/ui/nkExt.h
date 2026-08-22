#pragma once

#include <stdbool.h>
#include <stdint.h>

//Extensions for nuklear (nk)

struct clm_vec2;
struct clm_vec3;
struct clm_vec4;
struct clm_transform25;
struct nk_context;
struct nk_color;

typedef struct clm_vec2 clm_vec2;
typedef struct clm_vec3 clm_vec3;
typedef struct clm_vec4 clm_vec4;
typedef struct nk_context nk_context;
typedef struct nk_color nk_color;
typedef struct clm_transform25 clm_transform25;

bool nkx_property_vecN(nk_context* ctx, const char* groupLabel, const char** names, float* values, int count, float min, float max, float step, float pixelInc);
bool nkx_property_vec2(nk_context* ctx, const char* name, clm_vec2* v, float min, float max, float step, float pixelInc);
bool nkx_property_vec3(nk_context* ctx, const char* name, clm_vec3* v, float min, float max, float step, float pixelInc);
bool nkx_property_rotation(nk_context* ctx, const char* name, clm_vec2* r);
bool nkx_property_transform25(nk_context* ctx, const char* name, clm_transform25* transform);
void nkx_label_format(nk_context* ctx, const char* format, ...);
void nkx_readonly_vec2(nk_context* ctx, const char* name, clm_vec2 v);

nk_color nkx_vec4ToColor(clm_vec4 color);
clm_vec4 nkx_colorToVec4(nk_color color);

uint32_t nkx_titledWindow();
uint32_t nkx_basicWindow();
