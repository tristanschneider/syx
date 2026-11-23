#include <sandbox/SandGridDiagnostics.h>

#include <sandbox/Renderer.h>
#include <std/Diagnostics.h>
#include <clm/transform25.h>
#include <stdio.h>
#include <stdarg.h>

struct sbx_SandGridDiagnosticsImpl {
  bool dirty;
  sbx_Renderable renderable;
  sbx_Model model;
  sbx_ModelVertex* vertices;
  //Count desired in draw calls
  size_t vertexCount;
  //Current head new pushVertex calls should write to
  size_t writeHead;
  //Full size of vertex buffer
  size_t vertexCapacity;
  //Current color to use in pushVertex calls
  clm_vec4 color;
};

void sbx_vprint_impl(void* data, const char* format, va_list list) {
  STD_UNUSED(data);
  vprintf(format, list);
}

void sbx_pushVertex(struct sbx_SandGridDiagnosticsImpl* impl, const clm_vec2* v) {
  impl->dirty = true;
  if(impl->writeHead >= impl->vertexCapacity) {
    impl->writeHead = 0;
  }
  impl->vertices[impl->writeHead++] = (sbx_ModelVertex){
    .pos = clm_vec3_ctor(v->x, v->y, 0.f),
    .uv = clm_vec2_zero(),
    .color = impl->color
  };
  //Set count to write head unless the head has wrapped
  if(impl->writeHead > impl->vertexCount) {
    impl->vertexCount = impl->writeHead;
  }
}

void sbx_startTriStrip_impl(void* data, clm_vec2 a, clm_vec2 b) {
  struct sbx_SandGridDiagnosticsImpl* impl = data;
  //Stop previous strip by emitting last and next, which causes the triangle that would normally
  //draw between them to be empty
  if(impl->writeHead > 0) {
    const sbx_ModelVertex* lastVertex = &impl->vertices[impl->writeHead - 1];
    clm_vec2 last = clm_vec2_ctor(lastVertex->pos.x, lastVertex->pos.y);
    sbx_pushVertex(impl, &last);
    sbx_pushVertex(impl, &a);
  }

  sbx_pushVertex(impl, &a);
  sbx_pushVertex(impl, &b);
}

void sbx_addTriStrip_impl(void* data, clm_vec2 a) {
  sbx_pushVertex(data, &a);
}

void sbx_setColor_impl(void* data, clm_byte4 c) {
  //Doesn't set dirty flag because setting the color without pushing new vertices doesn't change anything.
  struct sbx_SandGridDiagnosticsImpl* impl = data;
  impl->color = clm_vec4_ctor(
    clm_byte4_toFloat(&c, 0),
    clm_byte4_toFloat(&c, 1),
    clm_byte4_toFloat(&c, 2),
    clm_byte4_toFloat(&c, 3)
  );
}

struct sbx_SandGridDiagnostics sbx_SandGridDiagnosticsImpl_ctor(std_Allocator* alloc, sbx_Renderer* renderer, uint32_t capacity) {
  const size_t bodySize = sizeof(struct sbx_SandGridDiagnosticsImpl);
  const size_t bufferSize = sizeof(sbx_ModelVertex) * capacity;
  struct sbx_SandGridDiagnosticsImpl* impl = std_Allocator_zalloc(alloc, bodySize + bufferSize);
  //Vertices off the end of the struct
  impl->vertices = (sbx_ModelVertex*)((uint8_t*)impl + bodySize);
  impl->vertexCapacity = capacity;

  impl->model = sbx_Renderer_createModel(renderer);

  impl->renderable = sbx_Renderer_createRenderable(renderer);
  sbx_Renderer_setRenderablePipeline(renderer, impl->renderable, SBX_RENDERPIPELINE_STRIP);
  sbx_Renderer_setRenderableModel(renderer, impl->renderable, impl->model);
  //TODO: make this match the desired coordinate space for sand grid
  clm_transform25 transform = clm_transform25_identity();
  const float s = 1.f;
  transform.scale = clm_vec2_ctor(10 / 3.f, 10 / 3.f);
  transform.pos = clm_vec3_ctor(-s * 5.f, -s * 5.f, -0.1f);
  sbx_Renderer_setTransform(renderer, impl->renderable, &transform);

  return (struct sbx_SandGridDiagnostics) {
    .data = impl,
    .vprint = &sbx_vprint_impl,
    .startTriStrip = &sbx_startTriStrip_impl,
    .addTriStrip = &sbx_addTriStrip_impl,
    .setColor = &sbx_setColor_impl
  };
}

void sbx_SandGridDiagnosticsImpl_dtor(struct sbx_SandGridDiagnostics* self, std_Allocator* alloc, sbx_Renderer* renderer) {
  struct sbx_SandGridDiagnosticsImpl* impl = self->data;
  sbx_Renderer_destroyRenderable(renderer, impl->renderable);
  sbx_Renderer_destroyModel(renderer, impl->model);
  std_Allocator_dealloc(alloc, impl);
  self->data = NULL;
}

void sbx_SandGridDiagnosticsImpl_clear(struct sbx_SandGridDiagnostics* self) {
  struct sbx_SandGridDiagnosticsImpl* impl = self->data;
  impl->writeHead = impl->vertexCount = 0;
  impl->dirty = true;
}

void sbx_SandGridDiagnosticsImpl_update(struct sbx_SandGridDiagnostics* self, sbx_Renderer* renderer) {
  struct sbx_SandGridDiagnosticsImpl* impl = self->data;
  if(impl->dirty) {
    impl->dirty = false;
    sbx_Renderer_setModelVertices(renderer, impl->model, &(sbx_ModelVertices){
      .data = impl->vertices,
      .count = impl->vertexCount
    });
  }
}
