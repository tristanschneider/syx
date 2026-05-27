#pragma once

#include <stdint.h>
#include <stdbool.h>

struct std_Allocator_t;
struct std_Buffer_t;
typedef struct std_Allocator_t std_Allocator;
typedef struct std_Buffer_t std_Buffer;

//Can be zero initialized
struct std_Vector_t {
  void* data;
  uint32_t size;
  uint32_t capacity;
};
typedef struct std_Vector_t std_Vector;

struct std_VectorTraits_t {
  //Size in bytes of the elements
  uint32_t elementSize;
  //Capacity on the first resize. Must be > 0
  uint32_t initialCapacity;
  //Growth factor for all resizes after the first. Must be > 0
  uint8_t growthFactor;
};
typedef struct std_VectorTraits_t std_VectorTraits;

struct std_VectorAllocTraits_t {
  const std_VectorTraits* traits;
  std_Allocator* allocator;
};
typedef struct std_VectorAllocTraits_t std_VectorAllocTraits;

//"C"onst context
struct std_VectorCtxC_t {
  const std_Vector* vector;
  const std_VectorTraits* traits;
};
typedef struct std_VectorCtxC_t std_VectorCtxC;

//"M"utable context
struct std_VectorCtxM_t {
  std_Vector* vector;
  const std_VectorTraits* traits;
};
typedef struct std_VectorCtxM_t std_VectorCtxM;

//Mutable context with "A"llocator
struct std_VectorCtxA_t {
  std_Vector* vector;
  const std_VectorAllocTraits* traits;
};
typedef struct std_VectorCtxA_t std_VectorCtxA;

//Context conversions: std_Vector_ctx<src><dst>
//So ctxmc means convert from std_VectorCtxM to std_VectorCtxC
std_VectorCtxC std_Vector_ctxmc(std_VectorCtxM* ctx);
std_VectorCtxM std_Vector_ctxam(std_VectorCtxA* ctx);
std_VectorCtxC std_Vector_ctxac(std_VectorCtxA* ctx);

//ctor may be zero initialized
void std_Vector_dtor(std_VectorCtxA* ctx);

std_VectorTraits std_Vector_defaultTraits(uint32_t size);

uint32_t std_Vector_size(const std_Vector* vector);
void* std_Vector_data(std_Vector* vector);
const void* std_Vector_cdata(const std_Vector* vector);
size_t std_Vector_sizeBytes(const std_VectorCtxC* vector);
void* std_Vector_get(std_VectorCtxM* vector, uint32_t i);
void* std_Vector_back(std_VectorCtxM* vector);
void* std_Vector_end(std_VectorCtxM* vector);
const void* std_Vector_cget(const std_VectorCtxC* vector, uint32_t i);

//Convert the currently used portion (within size) of the vector to a buffer
std_Buffer std_Vector_toSizeBuffer(std_VectorCtxM* vector);
std_Vector std_Vector_fromBuffer(std_Buffer* buffer, const std_VectorTraits* traits, uint32_t size);

bool std_Vector_pushBack(std_VectorCtxA* vector, const void* element);
void std_Vector_popBack(std_Vector* vector);
bool std_Vector_reserve(std_VectorCtxA* vector, uint32_t newCap);
//Resize to the desired size, new contents will be uninitialized
bool std_Vector_resize(std_VectorCtxA* vector, uint32_t newSize);
void std_Vector_clear(std_Vector* vector);
std_Vector std_Vector_clone(const std_VectorCtxC* src, std_Allocator* alloc);
bool std_Vector_insert(std_VectorCtxA* ctx, uint32_t at, const void* elements, uint32_t count);
void std_Vector_erase(std_VectorCtxM* ctx, uint32_t at, uint32_t count);
//Swap remove and return the index of the removed element, which is also the new size
uint32_t std_Vector_swapRemove(std_VectorCtxM* ctx, uint32_t at);

//Reallocate the buffer to at least `neededCap`.
//Capacity is updated while size remains the same.
void std_details_Vector_grow(std_VectorCtxA* vector, uint32_t neededCap);
//Grow for the desird size in elements if necessary. Returns true if there is space for the elements.
bool std_details_Vector_tryGrow(std_VectorCtxA* vector, uint32_t neededElements);
