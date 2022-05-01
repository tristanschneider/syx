#include "Precompile.h"
#include "ecs/system/ogl/AssetSystemOGL.h"

#include "ecs/component/AssetComponent.h"
#include "ecs/component/GraphicsComponents.h"
#include "ecs/component/ogl/OGLContextComponent.h"
#include "ecs/component/ogl/OGLHandleComponents.h"
#include <GL/glew.h>

namespace ogl {
  using namespace Engine;
  using OGLView = View<Write<OGLContextComponent>>;
  using TextureView = View<Include<NeedsGpuUploadComponent>, Read<TextureComponent>>;
  using TextureModifier = EntityModifier<NeedsGpuUploadComponent, AssetComponent, TextureHandleOGLComponent, AssetLoadFailedComponent>;
  void uploadTextures(SystemContext<TextureView, TextureModifier, OGLView>& context) {
    auto& view = context.get<TextureView>();
    auto modifier = context.get<TextureModifier>();
    while(auto tex = view.tryGetFirst()) {
      const Entity entity = tex->entity();
      const TextureComponent& baseTexture = tex->get<const TextureComponent>();
      TextureHandleOGLComponent oglTexture;

      glGenTextures(1, &oglTexture.mTexture);
      glBindTexture(GL_TEXTURE_2D, oglTexture.mTexture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GLsizei(baseTexture.mWidth), GLsizei(baseTexture.mHeight), 0, GL_RGBA, GL_UNSIGNED_BYTE, baseTexture.mBuffer.data());
      //Define sampling mode, no mip maps snap to nearest
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

      //TODO: add asset load failure on ogl error

      //Add the handle info to the entity
      modifier.removeComponent<NeedsGpuUploadComponent>(entity);
      //Remove request to upload to gpu now that it's done
      modifier.addComponent<TextureHandleOGLComponent>(entity, oglTexture);
      //Mark the asset as completely loaded
      modifier.addComponent<AssetComponent>(entity);
    }
  }

  using ModelView = View<Include<NeedsGpuUploadComponent>, Read<GraphicsModelComponent>>;
  using ModelModifier = EntityModifier<NeedsGpuUploadComponent, AssetComponent, GraphicsModelHandleOGLComponent, AssetLoadFailedComponent>;
  void uploadModels(SystemContext<ModelView, ModelModifier, OGLView>& context) {
    auto& view = context.get<ModelView>();
    auto modifier = context.get<ModelModifier>();
    while(auto model = view.tryGetFirst()) {
      const Entity entity = model->entity();
      const GraphicsModelComponent& baseModel = model->get<const GraphicsModelComponent>();
      GraphicsModelHandleOGLComponent oglModel;

      //Generate and upload vertex buffer
      glGenBuffers(1, &oglModel.mVertexBuffer);
      glBindBuffer(GL_ARRAY_BUFFER, oglModel.mVertexBuffer);
      glBufferData(GL_ARRAY_BUFFER, sizeof(GraphicsModelComponent::Vertex)*baseModel.mVertices.size(), baseModel.mVertices.data(), GL_STATIC_DRAW);

      //Generate and upload index buffer
      glGenBuffers(1, &oglModel.mIndexBuffer);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, oglModel.mIndexBuffer);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(size_t)*baseModel.mIndices.size(), baseModel.mIndices.data(), GL_STATIC_DRAW);

      //Generate vertex array
      glGenVertexArrays(1, &oglModel.mVertexArray);
      //Bind this array so we can fill it in
      glBindVertexArray(oglModel.mVertexArray);

      //Define vertex attributes
      glEnableVertexAttribArray(0);
      GLsizei stride = sizeof(GraphicsModelComponent::Vertex);
      size_t start = 0;
      //Position
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(start));
      start += sizeof(float)*3;
      //Normal
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(start));
      start += sizeof(float)*3;
      //UV
      glEnableVertexAttribArray(2);
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(start));
      glBindVertexArray(0);

      //Add the handle info to the entity
      modifier.removeComponent<NeedsGpuUploadComponent>(entity);
      //Remove request to upload to gpu now that it's done
      modifier.addComponent<GraphicsModelHandleOGLComponent>(entity, oglModel);
      //Mark the asset as completely loaded
      modifier.addComponent<AssetComponent>(entity);
    }
  }
};

std::shared_ptr<Engine::System> AssetSystemOGL::uploadTextures() {
  return ecx::makeSystem("uploadTextures", &ogl::uploadTextures, OGL_THREAD);
}

std::shared_ptr<Engine::System> AssetSystemOGL::uploadModels() {
  return ecx::makeSystem("uploadModels", &ogl::uploadModels, OGL_THREAD);
}
