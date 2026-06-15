#pragma once

struct std_Allocator;
struct sbx_Scene;

typedef struct std_Allocator std_Allocator;
typedef struct sbx_Scene sbx_Scene;

sbx_Scene* sbx_NarrowphaseScene_ctor(std_Allocator* alloc);
