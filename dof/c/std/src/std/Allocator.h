#pragma once

#include <stdint.h>

struct std_Allocator_t {
  void* data;
  void*(*alloc)(void*, size_t);
  void(*dealloc)(void*, void*);
};
typedef struct std_Allocator_t std_Allocator;

inline void* std_Allocator_alloc(std_Allocator* allocator, size_t size) {
  return allocator->alloc(allocator->data, size);
}

inline void std_Allocator_dealloc(std_Allocator* allocator, void* toFree) {
  allocator->dealloc(allocator->data, toFree);
}