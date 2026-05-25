#include <std/Diagnostics.h>
#include <stdio.h>
#include <stdarg.h>

void std_diagnostics_logImpl(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
}
