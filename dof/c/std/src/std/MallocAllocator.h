#pragma once

struct std_Allocator_t;

// No dtor because no state is stored on the returned type
struct std_Allocator_t std_MallocAllocator_ctor();
