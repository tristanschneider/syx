#include "Precompile.h"
#include "allocator/LIFOAllocator.h"
thread_local std::unique_ptr<LIFOAllocatorPage> gLifoAllocatorPage = std::make_unique<LIFOAllocatorPage>(LIFOAllocator<uint8_t>::SIZE);
thread_local std::unique_ptr<StrictLIFOAllocatorPage> gStrictLIFOAllocatorPage = std::make_unique<StrictLIFOAllocatorPage>();
