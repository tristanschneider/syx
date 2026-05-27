#include <sandbox/Renderer.h>

#include <std/Buffer.h>
#include <std/Diagnostics.h>
#include <std/Map.h>
#include <std/Vector.h>
#include <clm/mat4.h>
#include <sokol_gfx.h>
#include <sokol_glue.h>
#include <sandbox/shaders/Mesh.h>

#include <sokol_app.h>
#include <Nuklear/nuklear.h>
#include <util/sokol_nuklear.h>

#define sbx_renderableCtxM(renderer) (std_VectorCtxM) { .traits = &RENDERABLE_TRAITS, .vector = &renderer->renderables }
#define sbx_renderableCtxA(renderer) (std_VectorCtxA) { .vector = &renderer->renderables, .traits = &(std_VectorAllocTraits) { .traits = &RENDERABLE_TRAITS, .allocator = renderer->alloc } }

static std_VectorTraits RENDERABLE_TRAITS;

struct sbx_MeshPass {
  bool updateInstanceData;
  sg_pipeline pipeline;
  sg_bindings bindings;
  std_Buffer instanceData;
};
typedef struct sbx_MeshPass sbx_MeshPass;

struct sbx_RendererModel {
  sg_buffer buffer;
  sbx_ModelVertices vertices;
};
typedef struct sbx_RendererModel sbx_RendererModel;

struct sbx_RendererRenderable {
  const uint32_t id;
  sbx_RendererModel* model;
};
typedef struct sbx_RendererRenderable sbx_RendererRenderable;

struct sbx_Renderer {
  std_Allocator* alloc;
  sbx_MeshPass meshPass;
  std_Vector renderables;
  std_VoidMap renderableMap;
  uint32_t idGen;
  sg_image emptyImage;
};

void sbx_setInstanceChanged(sbx_Renderer* renderer) {
  renderer->meshPass.updateInstanceData = true;
}

sbx_RendererModel* sbx_unwrapModel(sbx_Model model) {
  return (sbx_RendererModel*)model.data;
}

uint32_t sbx_unwrapRenderable(sbx_Renderable renderable) {
  return (uint32_t)(uint64_t)renderable.data;
}

sbx_Renderable sbx_wrapRenderable(uint32_t id) {
  return (sbx_Renderable) {
    .data = (void*)(uint64_t)id
  };
}

sbx_RendererRenderable* sbx_tryGetRenderable(sbx_Renderer* renderer, sbx_Renderable handle) {
  //Look up the id mapping to get the index
  std_VoidMapPair* pair = std_VoidMap_find(&renderer->renderableMap, sbx_unwrapRenderable(handle));
  if(pair) {
    //Index into the renderable container with the index corresponding to the id
    const uint32_t index = (uint32_t)(uint64_t)pair->value;
    STD_ASSERT(index < renderer->renderables.size);
    return (sbx_RendererRenderable*)std_Vector_get(&sbx_renderableCtxM(renderer), index);
  }
  return NULL;
}

void sbx_setBuffer(sg_buffer* dst, sg_buffer src) {
  if (dst->id) {
    sg_destroy_buffer(*dst);
  }
  *dst = src;
}

sbx_MeshPass renderer_createMeshPass(std_Allocator* alloc) {
  sg_pipeline_desc desc = { 0 };
  desc.shader = sg_make_shader(Mesh_shader_desc(sg_query_backend()));
  desc.layout.attrs[ATTR_Mesh_vertPos].format = SG_VERTEXFORMAT_FLOAT3;
  desc.layout.attrs[ATTR_Mesh_vertUV].format = SG_VERTEXFORMAT_FLOAT2;
  desc.layout.attrs[ATTR_Mesh_vertColor].format = SG_VERTEXFORMAT_FLOAT4;
  desc.color_count = 1;
  desc.depth = (sg_depth_state) {
    .pixel_format = SG_PIXELFORMAT_DEPTH_STENCIL,
    .compare = SG_COMPAREFUNC_LESS,
    .write_enabled = true
  };
  sg_pipeline pipeline = sg_make_pipeline(&desc);

  sg_bindings bindings = { 0 };
  size_t maxSize = 1000;
  sg_buffer_desc bdesc = {
    .type = SG_BUFFERTYPE_STORAGEBUFFER,
    .usage = SG_USAGE_STREAM,
    .size = sizeof(INSTANCE_t) * maxSize
  };
  bindings.storage_buffers[SBUF_instance] = sg_make_buffer(&bdesc);
  bindings.samplers[SMP_sam] = sg_make_sampler(&(sg_sampler_desc){ 0 });

  return (sbx_MeshPass){
    .pipeline = pipeline,
    .bindings = bindings,
    .instanceData = (std_Buffer){
      .data = std_Allocator_alloc(alloc, bdesc.size),
      .sizeBytes = bdesc.size
    }
  };
}

sg_image renderer_createEmptyImage() {
  sg_image_desc desc = {
    .width = 1,
    .height = 1,
    .pixel_format = SG_PIXELFORMAT_RGBA8
  };
  uint32_t whitePixel = 0xffffffff;
  desc.data.subimage[0][0] = (sg_range){ .ptr = &whitePixel, .size = sizeof(whitePixel) };
  return sg_make_image(&desc);
}

void renderer_init(sbx_Renderer* renderer) {
  RENDERABLE_TRAITS = (std_VectorTraits) {
    .elementSize = sizeof(sbx_RendererRenderable),
    .growthFactor = 2,
    .initialCapacity = 100
  };
  renderer->meshPass = renderer_createMeshPass(renderer->alloc);
  renderer->emptyImage = renderer_createEmptyImage();
}

sbx_Renderer* sbx_Renderer_ctor(std_Allocator* alloc) {
  sbx_Renderer* result = std_Allocator_zalloc(alloc, sizeof(sbx_Renderer));
  result->alloc = alloc;

  renderer_init(result);

  return result;
}

void sbx_Renderer_dtor(sbx_Renderer* renderer) {
  std_Allocator* alloc = renderer->alloc;
  std_Buffer_dtor(&renderer->meshPass.instanceData, alloc);
  std_Vector_dtor(&sbx_renderableCtxA(renderer));
  std_VoidMap_dtor(&renderer->renderableMap, alloc);
  std_Allocator_dealloc(alloc, renderer);
}

void sbx_updateInstanceData(sbx_Renderer* renderer) {
  //Recompute all instance data and upload it.
  //sokol only allows one buffer upload per frame.
  //It would be more efficient to only update the relevant instances cpu side and upload the entire contents.
  const uint32_t maxInstanceCount = renderer->meshPass.instanceData.sizeBytes / sizeof(INSTANCE_t);
  const uint32_t instanceCount = std_min(renderer->renderables.size, maxInstanceCount);
  INSTANCE_t* instances = renderer->meshPass.instanceData.data;

  for(uint32_t i = 0; i < instanceCount; ++i) {
    INSTANCE_t* instance = &instances[i];
    clm_mat4 m = clm_mat4_identity();
    memcpy(instance->transform, &m, sizeof(clm_mat4));
  }
  sg_update_buffer(renderer->meshPass.bindings.storage_buffers[SBUF_instance], &(sg_range){
    .ptr = instances,
    .size = instanceCount * sizeof(INSTANCE_t)
  });
}

void sbx_renderMeshPass(sbx_Renderer* renderer) {
  sbx_MeshPass* pass = &renderer->meshPass;
  //Activate mesh pass pipeline
  sg_apply_pipeline(pass->pipeline);

  //Set uniform
  clm_mat4 worldToView = clm_mat4_identity();
  uniforms_t uniforms;
  uniforms.instanceOffset = 0;
  memcpy(&uniforms.worldToView, &worldToView, sizeof(clm_mat4));

  //Draw call for each instance. Would be more efficient to sort into batches
  sbx_RendererRenderable* renderables = (sbx_RendererRenderable*)renderer->renderables.data;
  for(uint32_t i = 0; i < renderer->renderables.size; ++i) {
    sbx_RendererRenderable* r = &renderables[i];
    if(!r->model) {
      continue;
    }

    uniforms.instanceOffset = (int)i;
    sg_apply_uniforms(UB_uniforms, &(sg_range){ &uniforms, sizeof(uniforms) });

    pass->bindings.images[IMG_tex] = renderer->emptyImage;
    pass->bindings.vertex_buffers[0] = r->model->buffer;
    sg_apply_bindings(&renderer->meshPass.bindings);

    sg_draw(0, (int)r->model->vertices.count, 1);
  }
}

void sbx_Renderer_render(sbx_Renderer* renderer) {
  if(renderer->meshPass.updateInstanceData) {
    sbx_updateInstanceData(renderer);
    renderer->meshPass.updateInstanceData = false;
  }

  //Begin pass render to screen
  sg_begin_pass(&(sg_pass){
    .action = {
      .colors[0] = {
        .load_action = SG_LOADACTION_CLEAR, .clear_value = { 0.25f, 0.5f, 0.7f, 1.0f }
      },
      .depth = (sg_depth_attachment_action){
        0
      }
    },
    .swapchain = sglue_swapchain()
  });

  sbx_renderMeshPass(renderer);
  snk_render(sapp_width(), sapp_height());

  sg_end_pass();
  sg_commit();
}

sbx_Model sbx_Renderer_createModel(sbx_Renderer* renderer) {
  STD_UNUSED(renderer);
  sbx_RendererModel* model = std_Allocator_zalloc(renderer->alloc, sizeof(sbx_RendererModel));
  return (sbx_Model){
    .data = model
  };
}

void sbx_Renderer_destroyModel(sbx_Renderer* renderer, sbx_Model model) {
  STD_UNUSED(renderer);
  //Should always be created through createModel which would always be non-null unless allocation failed.
  STD_ASSERT(model.data);
  sbx_RendererModel* r = sbx_unwrapModel(model);
  if(!r) {
    return;
  }

  //Unlink renderables pointing at this model
  std_VectorCtxM ctx = sbx_renderableCtxM(renderer);
  for(uint32_t i = 0; i < renderer->renderables.size; ++i) {
    sbx_RendererRenderable* e = (sbx_RendererRenderable*)std_Vector_get(&ctx, i);
    if(e->model == r) {
      e->model = NULL;
    }
  }

  //Destroy sg resource which would exist unless none was ever assigned after createModel
  sbx_setBuffer(&r->buffer, (sg_buffer){ 0 });
  //Destroy the model itself
  std_Allocator_dealloc(renderer->alloc, model.data);
}

sbx_ModelVertices sbx_Renderer_getModelVertices(sbx_Renderer* renderer, sbx_Model model) {
  STD_UNUSED(renderer);
  return sbx_unwrapModel(model)->vertices;
}

std_Buffer sbx_modelToBuffer(const sbx_ModelVertices* model) {
  return (std_Buffer) {
    //Hack casting away const, but it won't be changed other than deallocating the buffer
    .data = (void*)model->data,
    .sizeBytes = model->count * sizeof(model->data[0])
  };
}

void sbx_Renderer_setModelVertices(sbx_Renderer* renderer, sbx_Model model, const sbx_ModelVertices* vertices) {
  sbx_RendererModel* r = sbx_unwrapModel(model);
  //Convert to buffer for convenient reallocation
  std_Buffer storage = sbx_modelToBuffer(&r->vertices);
  std_Buffer_assign(&storage, sbx_modelToBuffer(vertices), renderer->alloc);
  //I don't know what to do if this fails
  STD_ASSERT(storage.data);

  //Store new buffer, old one was freed above
  r->vertices = (sbx_ModelVertices){
    .data = (sbx_ModelVertex*)storage.data,
    .count = vertices->count
  };
  //Update sg resource, which frees the old one if applicable
  sbx_setBuffer(&r->buffer, sg_make_buffer(&(sg_buffer_desc) {
    .data = (sg_range) {
      .ptr = storage.data,
      .size = storage.sizeBytes
    }
  }));
}

sbx_Renderable sbx_Renderer_createRenderable(sbx_Renderer* renderer) {
  //Generate a new id
  uint32_t id = ++renderer->idGen;
  if(!id) {
    id = ++renderer->idGen;
  }

  sbx_setInstanceChanged(renderer);

  //Allocate storage for the new element
  uint32_t index = renderer->renderables.size;
  std_Vector_pushBack(&sbx_renderableCtxA(renderer), &(sbx_RendererRenderable) { .id = id });

  //Map the id to the index of the new storage
  std_VoidMap_insert(&renderer->renderableMap, id, (void*)(uint64_t)index, STD_MAP_LOAD_FACTOR, renderer->alloc);

  //Return handle referencing the new id
  return (sbx_Renderable){
    .data = (void*)(uint64_t)id
  };
}

void sbx_Renderer_destroyRenderable(sbx_Renderer* renderer, sbx_Renderable renderable) {
  //Find index corresponding to the id
  std_VoidMapPair* removePair = std_VoidMap_find(&renderer->renderableMap, sbx_unwrapRenderable(renderable));
  if(!removePair) {
    return;
  }

  sbx_setInstanceChanged(renderer);

  const uint32_t removedIndex = (uint32_t)(uint64_t)removePair->value;
  //Erase the mapping to the removed element
  std_VoidMap_eraseIt(&renderer->renderableMap, removePair);

  //Remove the storage for the element and swap the end into it
  const uint32_t swappedIndex = std_Vector_swapRemove(&sbx_renderableCtxM(renderer), removedIndex);
  //If this was removing from the end, nothing else to do
  if(swappedIndex <= removedIndex) {
    return;
  }
  //Update the id mapping for the swapped element to the new location
  const sbx_RendererRenderable* swappedElement = (const sbx_RendererRenderable*)std_Vector_get(&sbx_renderableCtxM(renderer), removedIndex);

  std_VoidMapPair* swappedPair = std_VoidMap_find(&renderer->renderableMap, swappedElement->id);
  //Should always exist, otherwise there was a bookkeeping error
  STD_ASSERT(swappedPair);
  //Point the swapped element at its new location
  swappedPair->value = (void*)(uint64_t)removedIndex;
}

void sbx_Renderer_setRenderableModel(sbx_Renderer* renderer, sbx_Renderable renderable, sbx_Model model) {
  sbx_RendererRenderable* r = sbx_tryGetRenderable(renderer, renderable);
  sbx_RendererModel* m = sbx_unwrapModel(model);
  if(r && m) {
    r->model = m;
  }
}
