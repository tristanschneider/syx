#pragma once

#include <std/Allocator.h>

struct std_CountingAllocator_t {
  std_Allocator* parent;
  size_t bytesInUse;
};
typedef struct std_CountingAllocator_t std_CountingAllocator;

std_Allocator std_CountingAllocator_toAlloc(std_CountingAllocator* alloc);
