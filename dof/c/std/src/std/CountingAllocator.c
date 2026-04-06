#include <std/CountingAllocator.h>

struct std_CountingHeader_t {
  size_t size;
};
typedef struct std_CountingHeader_t std_CountingHeader;

void* std_counting_alloc(void* self, size_t size) {
  std_CountingAllocator* alloc = (std_CountingAllocator*)self;

  //Allocate with extra space for the header
  const size_t sizeWithHeader = size + sizeof(std_CountingHeader);
  std_CountingHeader* result = (std_CountingHeader*)std_Allocator_alloc(alloc->parent, sizeWithHeader);

  //If allocation succeeded, write to the header and increment stored size
  if(result) {
    result->size = sizeWithHeader;
    alloc->bytesInUse += sizeWithHeader;
    //Return the memory region after the header
    return result + 1;
  }
  return result;
}

void std_counting_dealloc(void* self, void* toFree) {
  std_CountingAllocator* alloc = (std_CountingAllocator*)self;

  //Go back from the user memory region to the header
  std_CountingHeader* header = ((std_CountingHeader*)toFree) - 1;
  alloc->bytesInUse -= header->size;

  std_Allocator_dealloc(alloc->parent, header);
}

std_Allocator std_CountingAllocator_toAlloc(std_CountingAllocator* alloc) {
  return (std_Allocator){
    .data = alloc,
    .alloc = &std_counting_alloc,
    .dealloc = &std_counting_dealloc
  };
}