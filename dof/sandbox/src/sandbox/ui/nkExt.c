#include <sandbox/ui/nkExt.h>

#include <clm/constants.h>
#include <clm/transform25.h>
#include <Nuklear/nuklear.h>

bool nkx_property_vecN(nk_context* ctx, const char* groupLabel, const char** names, float* values, int count, float min, float max, float step, float pixelInc) {
  nk_layout_row_dynamic(ctx, 30, count + 1);
  nk_label(ctx, groupLabel, NK_TEXT_ALIGN_LEFT);
  bool result = false;
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

  if(nk_property_float(ctx, name, 0, &deg, 360.f, 1.f, 0.5f)) {
    *r = clm_vec2_rad(deg * CLM_DEGRADF);
    return true;
  }
  return false;
}

bool nkx_property_transform25(nk_context* ctx, const char* name, clm_transform25* transform) {
  nk_label(ctx, name, NK_TEXT_ALIGN_LEFT);
  bool result = nkx_property_vec3(ctx, "Position", &transform->pos, -100.f, 100.f, 0.1f, 0.05f);
  result = nkx_property_rotation(ctx, "Rotation", &transform->rot) || result;
  result = nkx_property_vec2(ctx, "Scale", &transform->scale, 0.1f, 100.f, 0.1f, 0.05f) || result;
  return result;
}
