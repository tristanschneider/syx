#pragma once

//Use macros for ispc syntax that differs from C syntax to allow copy pasting to C for debugging
#define UNIFORM uniform
#define FLOAT2 float<2>
#define EXPORT export
#define FOREACH(value, initial, count) foreach(value = initial ... count)
#define UINT32 uint32
#define PRAGMA_IGNORE_PERF #pragma ignore warning(perf)