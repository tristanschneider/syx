#pragma once

#include <std/Allocator.h>
#include <std/Compare.h>
#include <string.h>
#include <stdbool.h>

struct std_Buffer_t {
  void* data;
  size_t sizeBytes;
};
typedef struct std_Buffer_t std_Buffer;

inline bool std_Buffer_assign(std_Buffer* dst, std_Buffer src, std_Allocator* alloc) {
  //Try to allocate the new storage unless none is needed
  void* newData = NULL;
  if(src.sizeBytes) {
    // Allocate and fail if allocation fails, but if no allocation is needed then it's not a failure
    newData = std_Allocator_alloc(alloc, src.sizeBytes);
    if(!newData) {
      return false;
    }
  }

  //Copy over the old contents if applicable
  if(src.data) {
    memcpy(newData, src.data, src.sizeBytes);
  }
  //Free old buffer if applicable
  if(dst->data) {
    std_Allocator_dealloc(alloc, dst->data);
  }

  dst->data = newData;
  dst->sizeBytes = src.sizeBytes;
  return true;
}

//Allocate a new buffer of the desired size and copy over the old number of bytes into the new buffer.
//Uncopied bytes are uninitialized
inline bool std_Buffer_reallocBytes(std_Buffer* buffer, size_t newBytes, std_Allocator* alloc) {
  //Try to allocate the new storage
  void* newData = std_Allocator_alloc(alloc, newBytes);
  if(!newData) {
    return false;
  }

  //Copy over the old contents if applicable
  if(buffer->data) {
    memcpy(newData, buffer->data, std_min(buffer->sizeBytes, newBytes));
    std_Allocator_dealloc(alloc, buffer->data);
  }

  buffer->data = newData;
  buffer->sizeBytes = newBytes;
  return true;
}

inline std_Buffer std_Buffer_empty() {
  std_Buffer result = { 0 };
  return result;
}

inline void std_Buffer_dtor(std_Buffer* buffer, std_Allocator* alloc) {
  if(buffer->data) {
    std_Allocator_dealloc(alloc, buffer->data);
    buffer->data = NULL;
  }
}

inline std_Buffer std_Buffer_copyCtor(void* data, size_t sizeBytes, std_Allocator* alloc) {
  std_Buffer result = {
    .data = std_Allocator_alloc(alloc, sizeBytes),
    .sizeBytes = sizeBytes
  };
  if(!result.data) {
    return std_Buffer_empty();
  }
  memcpy(result.data, data, sizeBytes);
  return result;
}

inline void* std_Buffer_get(void* data, size_t i) {
  return (uint8_t*)data + i;
}

inline const void* std_Buffer_getC(const void* data, size_t i) {
  return (const uint8_t*)data + i;
}
