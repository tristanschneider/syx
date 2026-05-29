#include <sandbox/ui/CameraUI.h>

#include <sandbox/Renderer.h>
#include <sandbox/Camera.h>
#include <Nuklear/nuklear.h>
#include <clm/constants.h>

#include <stdbool.h>

//TODO: nk extensions header
//TODO: ui to reflect a transform as it'll be used here and renderables
bool nkx_property_vec3(nk_context* ctx, const char* name, clm_vec3* v, float min, float max, float step, float pixelInc) {
  nk_layout_row_dynamic(ctx, 30, 4);
  nk_label(ctx, name, NK_TEXT_ALIGN_LEFT);
  const bool x = nk_property_float(ctx, "X", min, &v->x, max, step, pixelInc);
  const bool y = nk_property_float(ctx, "Y", min, &v->y, max, step, pixelInc);
  const bool z = nk_property_float(ctx, "Z", min, &v->z, max, step, pixelInc);
  return x || y || z;
}

void sbx_CameraUI_draw(nk_context* ctx, sbx_Renderer* renderer) {
  sbx_Camera camera = *sbx_Renderer_getCamera(renderer);
  nk_flags flags = NK_HEADER_RIGHT | NK_WINDOW_BORDER | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE;
  if(nk_begin_titled(ctx, "camera", "Camera", nk_rect(9, 9, 300, 200), flags)) {
    nk_layout_row_dynamic(ctx, 30, 1);
    nk_property_float(ctx, "FOV", 0.1f, &camera.fovY, CLM_PI2F, CLM_DEGRADF, CLM_PI2F/100.f);
    nkx_property_vec3(ctx, "Position", &camera.transform.pos, -100.f, 100.f, 0.1f, 0.1f);
  }

  nk_end(ctx);
  sbx_Renderer_setCamera(renderer, &camera);
}
