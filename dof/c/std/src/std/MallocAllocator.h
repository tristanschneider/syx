#pragma once

struct std_Allocator;

// No dtor because no state is stored on the returned type
struct std_Allocator std_MallocAllocator_ctor();
