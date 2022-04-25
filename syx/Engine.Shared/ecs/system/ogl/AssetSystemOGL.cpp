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
};

std::shared_ptr<Engine::System> AssetSystemOGL::uploadTextures() {
  return ecx::makeSystem("uploadTextures", &ogl::uploadTextures, OGL_THREAD);
}
