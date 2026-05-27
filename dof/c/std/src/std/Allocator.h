#pragma once

#include <stdint.h>
#include <string.h>

struct std_Allocator_t {
  void* data;
  void*(*alloc)(void*, size_t);
  void(*dealloc)(void*, void*);
};
typedef struct std_Allocator_t std_Allocator;

inline void* std_Allocator_alloc(std_Allocator* allocator, size_t size) {
  return allocator->alloc(allocator->data, size);
}

inline void* std_Allocator_zalloc(std_Allocator* allocator, size_t size) {
  void* result = allocator->alloc(allocator->data, size);
  memset(result, 0, size);
  return result;
}

inline void std_Allocator_dealloc(std_Allocator* allocator, void* toFree) {
  allocator->dealloc(allocator->data, toFree);
}