#pragma once

#include <std/Allocator.h>

struct sbx_Renderer;
struct sbx_SandGridDiagnostics;

//Implementation of sand grid diagnostics using renderer. Renders in a mesh up to `capacity` vertices
struct sbx_SandGridDiagnostics sbx_SandGridDiagnosticsImpl_ctor(std_Allocator* alloc, sbx_Renderer* renderer, uint32_t capacity);
void sbx_SandGridDiagnosticsImpl_dtor(struct sbx_SandGridDiagnostics* self, std_Allocator* alloc, sbx_Renderer* renderer);

//Reset draw vertices. Without this they will wrap back to the beginning of the buffer.
void sbx_SandGridDiagnosticsImpl_clear(struct sbx_SandGridDiagnostics* self);
//Upload latest buffer to gpu if needed
void sbx_SandGridDiagnosticsImpl_update(struct sbx_SandGridDiagnostics* self, sbx_Renderer* renderer);
