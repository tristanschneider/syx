#include <sandbox/ui/nkExt.h>

#include <clm/constants.h>
#include <clm/transform25.h>
#include <Nuklear/nuklear.h>
#include <string.h>
#include <std/Compare.h>
#include <stdio.h>

//Can be used to generate a unique id for nk_group_begin which doesn't have the same # unique generation that nk_property does.
struct nkx_UniqueString {
  char value[100];
};
typedef struct nkx_UniqueString nkx_UniqueString;

nkx_UniqueString nkx_buildUniqueString(nk_context* ctx, const char* name) {
  //Generate a unique id from the name and current window similar to how `nk_property` does.
  nkx_UniqueString result;
  //Append id
  snprintf(result.value, sizeof(result), "%s%d", name, ctx->current ? (int)ctx->current->seq : 0);
  return result;
}

bool nkx_property_vecN(nk_context* ctx, const char* groupLabel, const char** names, float* values, int count, float min, float max, float step, float pixelInc) {
  bool result = false;
  nk_label(ctx, groupLabel, NK_TEXT_ALIGN_LEFT | NK_TEXT_ALIGN_MIDDLE);
  for(int i = 0; i < count; ++i) {
    result = nk_property_float(ctx, names[i], min, &values[i], max, step, pixelInc) || result;
  }
  return result;
}

bool nkx_property_vec2(nk_context* ctx, const char* name, clm_vec2* v, float min, float max, float step, float pixelInc) {
  const char* names[] = { "#X", "#Y" };
  return nkx_property_vecN(ctx, name, names, v->xy, 2, min, max, step, pixelInc);
}

bool nkx_property_vec3(nk_context* ctx, const char* name, clm_vec3* v, float min, float max, float step, float pixelInc) {
  const char* names[] = { "#X", "#Y", "#Z" };
  return nkx_property_vecN(ctx, name, names, v->xyz, 3, min, max, step, pixelInc);
}

bool nkx_property_rotation(nk_context* ctx, const char* name, clm_vec2* r) {
  float deg = clm_vec2_getRad(r) * CLM_RADDEGF;

  if(nk_property_float(ctx, name, -360.f, &deg, 360.f, 1.f, 0.5f)) {
    *r = clm_vec2_rad(deg * CLM_DEGRADF);
    return true;
  }
  return false;
}

bool nkx_property_transform25(nk_context* ctx, const char* name, clm_transform25* transform) {
  bool result = false;
  nk_label(ctx, name, NK_TEXT_ALIGN_LEFT);
  nk_layout_row_dynamic(ctx, 0, 4);
  result = nkx_property_vec3(ctx, "Position", &transform->pos, -100.f, 100.f, 0.1f, 0.05f) || result;
  nk_layout_row_dynamic(ctx, 0, 1);
  result = nkx_property_rotation(ctx, "Rotation", &transform->rot) || result;
  nk_layout_row_dynamic(ctx, 0, 3);
  result = nkx_property_vec2(ctx, "Scale", &transform->scale, 0.1f, 100.f, 0.1f, 0.05f) || result;
  return result;
}

nk_color nkx_vec4ToColor(clm_vec4 color) {
  nk_color result;
  nk_byte* r = &result.r;
  for(int i = 0; i < 4; ++i, ++r) {
    *r = (nk_byte)(color.rgba[i] * 255.f);
  }
  return result;
}

clm_vec4 nkx_colorToVec4(nk_color color) {
  clm_vec4 result;
  const nk_byte* r = &color.r;
  for(int i = 0; i < 4; ++i) {
    result.rgba[i] = ((float)*r) / 255.f;
  }
  return result;
}

uint32_t nkx_titledWindow() {
  return nkx_basicWindow() | NK_WINDOW_TITLE;
}

uint32_t nkx_basicWindow() {
  return NK_WINDOW_MINIMIZABLE | NK_HEADER_RIGHT | NK_WINDOW_BORDER | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE;
}