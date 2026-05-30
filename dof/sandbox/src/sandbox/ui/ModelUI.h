#pragma once

struct std_Allocator;
struct sbx_Renderer;
struct nk_context;
struct sbx_ModelVertices;

typedef struct nk_context nk_context;
typedef struct std_Allocator std_Allocator;
typedef struct sbx_ModelVertices sbx_ModelVertices;
typedef struct sbx_Renderer sbx_Renderer;

//Presents UI to edit the model and returns a newly allocated model if any values were changed.
//Caller must deallocate the sbx_ModelVertices if not null
sbx_ModelVertices* sbx_ModelUI_draw(nk_context* ctx, sbx_ModelVertices in, std_Allocator* alloc);
