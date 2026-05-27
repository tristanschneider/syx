#include <std/Vector.h>

#include <std/Allocator.h>
#include <std/Buffer.h>
#include <std/Diagnostics.h>
#include <string.h>

//Compound literals are lvalues which allow for more convenient conversions but can't be used in the header shared with c++, as c++ doesn't support compound literals.
#define vctx_mc(ctxM) (std_VectorCtxC){ .vector = ctxM->vector, .traits = ctxM->traits }
#define vctx_am(ctxA) (std_VectorCtxM){ .vector = ctxA->vector, .traits = ctxA->traits->traits }
#define vctx_ac(ctxA) (std_VectorCtxC){ .vector = ctxA->vector, .traits = ctxA->traits->traits }

std_VectorCtxC std_Vector_ctxmc(std_VectorCtxM* ctx) {
  return vctx_mc(ctx);
}

std_VectorCtxM std_Vector_ctxam(std_VectorCtxA* ctx) {
  return vctx_am(ctx);
}

std_VectorCtxC std_Vector_ctxac(std_VectorCtxA* ctx) {
  std_VectorCtxC result = {
    .vector = ctx->vector,
    .traits = ctx->traits->traits
  };
  return result;
}

void std_Vector_dtor(std_VectorCtxA* ctx) {
  if(ctx->vector->data) {
    std_Allocator_dealloc(ctx->traits->allocator, ctx->vector->data);
    ctx->vector->data = NULL;
    ctx->vector->size = ctx->vector->capacity = 0;
  }
}

std_VectorTraits std_Vector_defaultTraits(uint32_t size) {
  std_VectorTraits result = {
    .elementSize = size,
    .initialCapacity = 32,
    .growthFactor = 2
  };
  return result;
}

uint32_t std_Vector_size(const std_Vector* vector) {
  return vector->size;
}

void* std_Vector_data(std_Vector* vector) {
  return vector->data;
}

const void* std_Vector_cdata(const std_Vector* vector) {
  return vector->data;
}

size_t std_Vector_sizeBytes(const std_VectorCtxC* vector) {
  return vector->vector->size * vector->traits->elementSize;
}

void* std_Vector_get(std_VectorCtxM* vector, uint32_t i) {
  STD_ASSERT(i < vector->vector->size);
  return std_Buffer_get(vector->vector->data, i*vector->traits->elementSize);
}

void* std_Vector_back(std_VectorCtxM* vector) {
  return std_Vector_get(vector, vector->vector->size - 1);
}

void* std_Vector_end(std_VectorCtxM* vector) {
  return std_Buffer_get(vector->vector->data, vector->vector->size*vector->traits->elementSize);
}

const void* std_Vector_cget(const std_VectorCtxC* vector, uint32_t i) {
  return std_Buffer_getC(vector->vector->data, i*vector->traits->elementSize);
}

//Convert the currently used portion (within size) of the vector to a buffer
std_Buffer std_Vector_toSizeBuffer(std_VectorCtxM* vector) {
  std_Buffer result = {
    .data = vector->vector->data,
    .sizeBytes = std_Vector_sizeBytes(&vctx_mc(vector))
  };
  return result;
}

std_Vector std_Vector_fromBuffer(std_Buffer* buffer, const std_VectorTraits* traits, uint32_t size) {
  STD_ASSERT((buffer->sizeBytes % traits->elementSize) == 0);
  STD_ASSERT(size <= buffer->sizeBytes);
  std_Vector result = {
    .data = buffer->data,
    .size = size,
    .capacity = buffer->sizeBytes / traits->elementSize
  };
  return result;
}

//Reallocate the buffer to at least `neededCap`.
//Capacity is updated while size remains the same.
void std_details_Vector_grow(std_VectorCtxA* vector, uint32_t neededCap) {
  STD_ASSERT(vector->traits->traits->growthFactor > 1 && vector->traits->traits->initialCapacity > 0);
  //Choose new size by multiplying by growth factor unless this is empty, then use the initial size
  size_t growthCap = vector->vector->capacity ? vector->vector->capacity : vector->traits->traits->initialCapacity;
  while(growthCap < neededCap) {
    growthCap *= vector->traits->traits->growthFactor;
  }
  const size_t newCapBytes = vector->traits->traits->elementSize * growthCap;

  //Try to allocate the new buffer
  std_Buffer vb = std_Vector_toSizeBuffer(&vctx_am(vector));
  if(!std_Buffer_reallocBytes(&vb, newCapBytes, vector->traits->allocator)) {
    return;
  }

  *vector->vector = std_Vector_fromBuffer(&vb, vector->traits->traits, vector->vector->size);
}

//Grow for the desird size in elements if necessary. Returns true if there is space for the elements.
bool std_details_Vector_tryGrow(std_VectorCtxA* vector, uint32_t neededElements) {
  if(neededElements >= vector->vector->capacity) {
    std_details_Vector_grow(vector, neededElements);
  }
  return neededElements <= vector->vector->capacity;
}

bool std_Vector_pushBack(std_VectorCtxA* vector, const void* element) {
  if(std_details_Vector_tryGrow(vector, vector->vector->size + 1)) {
    //Write the new element at old size before incrementing it
    memcpy(std_Vector_end(&vctx_am(vector)), element, vector->traits->traits->elementSize);
    ++vector->vector->size;
    return true;
  }
  return false;
}

void std_Vector_popBack(std_Vector* vector) {
  STD_ASSERT(vector->size);
  --vector->size;
}

bool std_Vector_reserve(std_VectorCtxA* vector, uint32_t newCap) {
  return std_details_Vector_tryGrow(vector, newCap);
}

//Resize to the desired size, new contents will be uninitialized
bool std_Vector_resize(std_VectorCtxA* vector, uint32_t newSize) {
  if(std_details_Vector_tryGrow(vector, newSize)) {
    vector->vector->size = newSize;
    return true;
  }
  return false;
}

void std_Vector_clear(std_Vector* vector) {
  vector->size = 0;
}

std_Vector std_Vector_clone(const std_VectorCtxC* src, std_Allocator* alloc) {
  std_Vector result = { 0 };
  std_VectorAllocTraits traits = {
    .traits = src->traits,
    .allocator = alloc,
  };
  std_VectorCtxA ctx = {
    .vector = &result,
    .traits = &traits
  };
  if(std_Vector_resize(&ctx, src->vector->size) && result.data && src->vector->data) {
    memcpy(result.data, src->vector->data, std_Vector_sizeBytes(src));
  }
  return result;
}

bool std_Vector_insert(std_VectorCtxA* ctx, uint32_t at, const void* elements, uint32_t count) {
  STD_ASSERT(at < ctx->vector->size);
  const uint32_t begin = at;
  const uint32_t end = at + count;
  const uint32_t oldSize = ctx->vector->size;
  if(!std_Vector_resize(ctx, ctx->vector->size + count)) {
    return false;
  }

  std_VectorCtxM ctxm = std_Vector_ctxam(ctx);
  uint8_t* beginPtr = (uint8_t*)std_Vector_get(&ctxm, begin);
  uint8_t* endPtr = (uint8_t*)std_Vector_get(&ctxm, end);
  uint8_t* vectorEnd = (uint8_t*)std_Vector_end(&ctxm);
  uint8_t* oldVectorEnd = (uint8_t*)std_Vector_get(&ctxm, oldSize);
  //Shift over `at` and everything to the right to the newly resized space
  const errno_t errMove = memmove_s(endPtr, vectorEnd - endPtr, beginPtr, oldVectorEnd - beginPtr);
  //Error would indicate invalid buffer sizes provided above which would be a logic error within the vector
  STD_ASSERT(errMove == 0);

  //Copy the elements into the hole from the above move
  const errno_t errCopy = memcpy_s(beginPtr, endPtr - beginPtr, elements, count * ctx->traits->traits->elementSize);
  STD_ASSERT(errCopy == 0);

  return !errCopy && !errMove;
}

void std_Vector_erase(std_VectorCtxM* ctx, uint32_t at, uint32_t count) {
  STD_ASSERT(at + count <= ctx->vector->size);
  uint8_t* holeBegin = (uint8_t*)std_Vector_get(ctx, at);
  uint8_t* holeEnd = holeBegin + count*ctx->traits->elementSize;
  uint8_t* vectorEnd = (uint8_t*)std_Vector_end(ctx);
  uint8_t* newEnd = (uint8_t*)std_Vector_get(ctx, ctx->vector->size - count);
  //Copy the elements to the right of the erased range into the erased range
  memmove_s(holeBegin, newEnd - holeBegin, holeEnd, vectorEnd - holeEnd);
  //Trim off the end now that everything has been moved down
  ctx->vector->size -= count;
}

uint32_t std_Vector_swapRemove(std_VectorCtxM* ctx, uint32_t at) {
  STD_ASSERT(at < ctx->vector->size);
  //Copy element from end into slot unless this is the last element
  if(at + 1 < ctx->vector->size) {
    memcpy(std_Vector_get(ctx, at), std_Vector_back(ctx), ctx->traits->elementSize);
  }
  //Erase the newly swapped-from element
  std_Vector_popBack(ctx->vector);
  return ctx->vector->size;
}
