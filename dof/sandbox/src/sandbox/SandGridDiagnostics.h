#pragma once

#include <clm/byte4.h>
#include <clm/vec2.h>
#include <std/Diagnostics.h>
#include <stdarg.h>

//Implementation supplies this, functions below used to call into it and build off of the minimal implementation.
struct sbx_SandGridDiagnostics {
  void* data;
  //printf format
  void(*vprint)(void*, const char*, va_list);
  //Start a new triangle strip.
  void(*startTriStrip)(void*, clm_vec2, clm_vec2);
  //Add to the most recently called startTriStrip. Undefined if called without ever starting.
  void(*addTriStrip)(void*, clm_vec2);
  //Set the color that all subsequent drawing operations use.
  void(*setColor)(void*, clm_byte4);

  //Configuration set by user
  bool drawIslandGraph;
  bool drawContactGraph;
  float drawConstraintError;
  float lineThickness;

  //Convenience for implementation of the wrapping methods
  clm_vec2 temp;
  bool inStrip;
};

//Call via STD_DEBUG_CALL to omit in optimized builds
void sbx_SandGridDiagnostics_print(struct sbx_SandGridDiagnostics* d, const char* format, ...);
void sbx_SandGridDiagnostics_startTriStrip(struct sbx_SandGridDiagnostics* d, clm_vec2 a, clm_vec2 b);
void sbx_SandGridDiagnostics_addTriStrip(struct sbx_SandGridDiagnostics* d, clm_vec2 v);
void sbx_SandGridDiagnostics_setColor(struct sbx_SandGridDiagnostics* d, clm_byte4 color);
void sbx_SandGridDiagnostics_startLineStrip(struct sbx_SandGridDiagnostics* d, clm_vec2 p);
void sbx_SandGridDiagnostics_addLineStrip(struct sbx_SandGridDiagnostics* d, clm_vec2 p);
void sbx_SandGridDiagnostics_addLine(struct sbx_SandGridDiagnostics* d, clm_vec2 a, clm_vec2 b);
void sbx_SandGridDiagnostics_addVector(struct sbx_SandGridDiagnostics* d, clm_vec2 start, clm_vec2 dir);
