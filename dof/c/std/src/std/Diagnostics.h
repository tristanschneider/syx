#pragma once

#include <assert.h>

void std_diagnostics_logImpl(const char* format, ...);

#define STD_UNUSED(...) (void)(__VA_ARGS__)
#define STD_ASSERT(...) assert(__VA_ARGS__)
#define STD_UNREACHABLE(void) assert(false)
#define LOGI(...) std_diagnostics_logImpl(__VA_ARGS__)

//Call something that may be disabled in optimized builds
#define STD_DEBUG_CALL(fn, ...) fn(__VA_ARGS__)