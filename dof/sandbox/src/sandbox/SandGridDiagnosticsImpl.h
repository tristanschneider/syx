#pragma once

#include <std/Allocator.h>

struct clm_transform25;
struct sbx_Renderer;
struct sbx_SandGridDiagnostics;

//Implementation of sand grid diagnostics using renderer. Renders in a mesh up to `capacity` vertices
struct sbx_SandGridDiagnostics sbx_SandGridDiagnosticsImpl_ctor(std_Allocator* alloc, sbx_Renderer* renderer, uint32_t capacity);
void sbx_SandGridDiagnosticsImpl_dtor(struct sbx_SandGridDiagnostics* self, std_Allocator* alloc, sbx_Renderer* renderer);

//Reset draw vertices. Without this they will wrap back to the beginning of the buffer.
void sbx_SandGridDiagnosticsImpl_clear(struct sbx_SandGridDiagnostics* self);
//Upload latest buffer to gpu if needed
void sbx_SandGridDiagnosticsImpl_update(struct sbx_SandGridDiagnostics* self, sbx_Renderer* renderer);
//Set the transform so that grid coordinates can be used to draw in the grid, where each cell is width/height 1
void sbx_SandGridDiagnosticsImpl_setTransform(
  struct sbx_SandGridDiagnostics* self,
  int32_t gridWidth,
  int32_t gridHeight,
  const clm_transform25* gridTransform,
  sbx_Renderer* renderer
);
