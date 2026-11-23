#include <sandbox/SandGridDiagnostics.h>

#include <stdarg.h>

void sbx_SandGridDiagnostics_print(struct sbx_SandGridDiagnostics* d, const char* format, ...) {
  va_list list;
  va_start(list, format);
  d->vprint(d->data, format, list);
  va_end(list);
}

void sbx_SandGridDiagnostics_startTriStrip(struct sbx_SandGridDiagnostics* d, clm_vec2 a, clm_vec2 b) {
  d->startTriStrip(d->data, a, b);
}

void sbx_SandGridDiagnostics_addTriStrip(struct sbx_SandGridDiagnostics* d, clm_vec2 v) {
  d->addTriStrip(d->data, v);
}

void sbx_SandGridDiagnostics_setColor(struct sbx_SandGridDiagnostics* d, clm_byte4 color) {
  d->setColor(d->data, color);
}

void sbx_SandGridDiagnostics_startLineStrip(struct sbx_SandGridDiagnostics* d, clm_vec2 p) {
  d->temp = p;
  d->inStrip = false;
}

void sbx_SandGridDiagnostics_addLineStrip(struct sbx_SandGridDiagnostics* d, clm_vec2 p) {
  //Use temp as previous starting point, then update it
  const clm_vec2 a = d->temp;
  d->temp = p;
  const clm_vec2 b = p;
  //Compute normalized line normal
  const clm_vec2 dir = clm_vec2_sub(&b, &a);
  clm_vec2 normal = clm_vec2_ortho(&dir);
  float len = clm_vec2_len(&normal);
  if(len > 0) {
    normal = clm_vec2_scale(&normal, 1.f / len);
  }
  else {
    len = 1.f;
    normal = clm_vec2_ctor(1, 0);
  }
  const float lineThickness = 0.05f;
  normal = clm_vec2_scale(&normal, lineThickness);

  //Start of strip needs to push the starting points that were deferred by startLineStrip
  if(!d->inStrip) {
    d->inStrip = true;
    d->startTriStrip(d->data, clm_vec2_add(&a, &normal), clm_vec2_sub(&a, &normal));
  }
  //Continuing a strip must connect the end of the old quad by pushing the near end of the quad
  else {
    d->addTriStrip(d->data, clm_vec2_add(&a, &normal));
    d->addTriStrip(d->data, clm_vec2_sub(&a, &normal));
  }
  //Push the far end of the quad
  d->addTriStrip(d->data, clm_vec2_add(&b, &normal));
  d->addTriStrip(d->data, clm_vec2_sub(&b, &normal));
}
