#include <std/MallocAllocator.h>

#include <std/Allocator.h>
#include <std/Diagnostics.h>
#include <stdlib.h>

void* mallocAlloc(void* _, size_t size) {
  STD_UNUSED(_);
  return malloc(size);
}

void mallocDealloc(void* _, void* data) {
  STD_UNUSED(_);
  free(data);
}

std_Allocator std_MallocAllocator_ctor() {
  std_Allocator result = {
    .alloc = &mallocAlloc,
    .dealloc = &mallocDealloc
  };
  return result;
}
