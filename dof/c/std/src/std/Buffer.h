#pragma once

#include <std/Allocator.h>
#include <std/Compare.h>
#include <string.h>

struct std_Buffer_t {
  void* data;
  size_t sizeBytes;
};
typedef struct std_Buffer_t std_Buffer;

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

inline void* std_Buffer_get(void* data, size_t i) {
  return (uint8_t*)data + i;
}

inline const void* std_Buffer_getC(const void* data, size_t i) {
  return (const uint8_t*)data + i;
}
