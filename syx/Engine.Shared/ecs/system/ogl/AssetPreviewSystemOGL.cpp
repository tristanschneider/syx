#include "Precompile.h"
#include "ecs/system/ogl/AssetPreviewSystemOGL.h"

#include "ecs/component/AssetComponent.h"
#include "ecs/component/EditorComponents.h"
#include "ecs/component/ogl/OGLHandleComponents.h"
#include "ecs/component/ImGuiContextComponent.h"
#include "imgui/imgui.h"

namespace Preview {
  using namespace Engine;
  using ImGuiView = View<Include<ImGuiContextComponent>>;
  using DialogView = View<Read<AssetPreviewDialogComponent>>;
  using AssetView = View<Read<TextureHandleOGLComponent>, Read<AssetInfoComponent>>;
  void previewTexture(SystemContext<ImGuiView, DialogView, AssetView>& context) {
    if(!context.get<ImGuiView>().tryGetFirst() || !context.get<DialogView>().tryGetFirst()) {
      return;
    }

    auto& assets = context.get<AssetView>();
    ImGui::Begin("Preview");
    for(auto&& dialog : context.get<DialogView>()) {
      const auto& dialogComponent = dialog.get<const AssetPreviewDialogComponent>();
      if(auto assetIt = assets.find(dialogComponent.mAsset); assetIt != assets.end()) {
        const FilePath& path = (*assetIt).get<const AssetInfoComponent>().mPath;
        GLHandle textureHandle = (*assetIt).get<const TextureHandleOGLComponent>().mTexture;
        ImGui::Text(path.cstr());
        ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<size_t>(textureHandle)), ImVec2(100, 100));
      }
      else if(dialogComponent.mAsset == Entity{}) {
        ImGui::Text("None");
      }
      else {
        ImGui::Text("Not found");
      }
    }
    ImGui::End();
  }
};

std::shared_ptr<Engine::System> AssetPreviewSystemOGL::previewTexture() {
  return ecx::makeSystem("PreviewTexture", &Preview::previewTexture, IMGUI_THREAD);
}
