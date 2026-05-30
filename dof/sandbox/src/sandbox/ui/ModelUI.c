#include <sandbox/ui/ModelUI.h>

#include <sandbox/Renderer.h>
#include <sandbox/ui/nkExt.h>
#include <std/Allocator.h>
#include <Nuklear/nuklear.h>

//Lazy creation for a copy of original. The resulting object object and buffer are allocated in a single block.
sbx_ModelVertices* modelui_getOrCreate(sbx_ModelVertices* copy, const sbx_ModelVertices* original, std_Allocator* alloc) {
  if(copy) {
    return copy;
  }
  const size_t pointsSize = sizeof(sbx_ModelVertex) * original->count;
  const size_t headerSize = sizeof(sbx_ModelVertices);
  //Allocate the block of the ModelVertices and the vertices themselves
  sbx_ModelVertices* result = std_Allocator_alloc(alloc, headerSize + pointsSize);
  //Point the vertices at the memory after it
  result->data = (void*)(result + 1);
  result->count = original->count;
  //Copy the points to the memory after the vertices that it's pointing at.
  memcpy((void*)result->data, original->data, pointsSize);
  return result;
}

sbx_ModelVertices* sbx_ModelUI_draw(nk_context* ctx, sbx_ModelVertices in, std_Allocator* alloc) {
  sbx_ModelVertices* copy = NULL;
  if(nk_tree_push(ctx, NK_TREE_TAB, "Model", NK_MAXIMIZED)) {
    for(size_t i = 0; i < in.count; ++i) {
      nk_layout_row_dynamic(ctx, 0, 8);
      sbx_ModelVertex v = in.data[i];
      //They have the same layout
      struct nk_colorf* color = (void*)&v.color;

      bool changed = nkx_property_vec3(ctx, "Pos", &v.pos, -100.f, 100.f, 0.1f, 0.05f);
      changed = nk_color_pick(ctx, color, NK_RGBA) || changed;
      changed = nkx_property_vec2(ctx, "UV", &v.uv, 0.f, 1.f, 0.1f, 0.05f) || changed;

      if (changed) {
        copy = modelui_getOrCreate(copy, &in, alloc);
        (*(sbx_ModelVertex*)(&copy->data[i])) = v;
      }
    }
    nk_tree_pop(ctx);
  }
  return copy;
}
