#pragma once

#include <assert.h>

void std_diagnostics_logImpl(const char* format, ...);

#define STD_UNUSED(...) (void)(__VA_ARGS__)
#define STD_ASSERT(...) assert(__VA_ARGS__)
#define LOGI(...) std_diagnostics_logImpl(__VA_ARGS__)
