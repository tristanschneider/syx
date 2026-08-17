#pragma once

#include <std/Allocator.h>
#include <clm/vec4.h>
#include <clm/vec3.h>
#include <clm/vec2.h>

struct clm_mat4;
struct clm_transform25;
struct sbx_Camera;
struct sbx_Renderer;

//Handle to a model that can be assigned to renderables
//Internally they are allocated into the data field and manually cleared from renderables before deletion.
struct sbx_Model {
  void* data;
};

//Format for model contents
struct sbx_ModelVertex {
  clm_vec3 pos;
  clm_vec2 uv;
  clm_vec4 color;
};
typedef struct sbx_ModelVertex sbx_ModelVertex;

struct sbx_ModelVertices {
  const sbx_ModelVertex* data;
  size_t count;
};

//A handle to an instance of something to render.
//The handle is mapped to an index of renderables in a contiguous array.
struct sbx_Renderable {
  void* data;
};

struct sbx_Texture {
  void* data;
};

struct sbx_TextureContents {
  //rgba8 contents
  const void* data;
  uint32_t width;
  uint32_t height;
};

typedef struct clm_mat4 clm_mat4;
typedef struct clm_transform25 clm_transform25;
typedef struct sbx_Camera sbx_Camera;
typedef struct sbx_Renderer sbx_Renderer;
typedef struct sbx_Model sbx_Model;
typedef struct sbx_Renderable sbx_Renderable;
typedef struct sbx_ModelVertices sbx_ModelVertices;
typedef struct sbx_Texture sbx_Texture;
typedef struct sbx_TextureContents sbx_TextureContents;

//Allocator must outlive the renderer as renderer will use it.
sbx_Renderer* sbx_Renderer_ctor(std_Allocator* alloc);
void sbx_Renderer_dtor(sbx_Renderer* renderer);
void sbx_Renderer_render(sbx_Renderer* renderer);

sbx_Model sbx_Renderer_createModel(sbx_Renderer* renderer);
void sbx_Renderer_destroyModel(sbx_Renderer* renderer, sbx_Model model);
sbx_ModelVertices sbx_Renderer_getModelVertices(sbx_Renderer* renderer, sbx_Model model);
//Set vertices by copy
void sbx_Renderer_setModelVertices(sbx_Renderer* renderer, sbx_Model model, const sbx_ModelVertices* vertices);

sbx_Texture sbx_Renderer_createTexture(sbx_Renderer* renderer);
void sbx_Renderer_destroyTexture(sbx_Renderer* renderer, sbx_Texture texture);
void sbx_Renderer_setTexture(sbx_Renderer* renderer, sbx_Texture texture, const sbx_TextureContents* contents);

sbx_Renderable sbx_Renderer_createRenderable(sbx_Renderer* renderer);
void sbx_Renderer_destroyRenderable(sbx_Renderer* renderer, sbx_Renderable renderable);
void sbx_Renderer_setRenderableModel(sbx_Renderer* renderer, sbx_Renderable renderable, sbx_Model model);
void sbx_Renderer_setRenderableTexture(sbx_Renderer* renderer, sbx_Renderable renderable, sbx_Texture texture);
clm_transform25 sbx_Renderer_getTransform(sbx_Renderer* renderer, sbx_Renderable renderable);
void sbx_Renderer_setTransform(sbx_Renderer* renderer, sbx_Renderable renderable, const clm_transform25* transform);

const sbx_Camera* sbx_Renderer_getCamera(sbx_Renderer* renderer);
void sbx_Renderer_setCamera(sbx_Renderer* renderer, const sbx_Camera* camera);
clm_mat4 sbx_Renderer_getScreenToWorld(sbx_Renderer* renderer);