#include "Precompile.h"
#include "AssetPreview.h"

#include "asset/Asset.h"
#include "asset/Texture.h"
#include "editor/event/EditorEvents.h"
#include <event/EventHandler.h>
#include "provider/MessageQueueProvider.h"
#include "system/AssetRepo.h"
#include "ImGuiImpl.h"
#include <imgui/imgui.h>

AssetPreview::~AssetPreview() = default;

AssetPreview::AssetPreview(MessageQueueProvider& msg, EventHandler& handler, AssetRepo& assets)
  : mMsg(msg)
  , mAssets(assets) {

  mListeners.push_back(handler.registerEventListener([this](const PreviewAssetEvent& e) {
    mPreview = !e.mAsset.isEmpty() ? mAssets.getAsset(e.mAsset) : nullptr;
    //TODO: request asset thumbnail if necessary
  }));

  //TODO: register for a response event to generate assets for previews
}

void AssetPreview::editorUpdate() {
  ImGui::Begin("Preview");
  if(mPreview && mPreview->isReady()) {
    auto lock = mPreview->getLock().getReader();
    ImGui::Text(mPreview->getInfo().mUri.c_str());
    //TODO: Might need separate thumbnail asset for previews that aren't textures
    if(mPreview->isOfType<Texture>()) {
      ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<size_t>(static_cast<Texture&>(*mPreview).mTexture)), ImVec2(100, 100));
    }
  }
  else {
    ImGui::Text("None");
  }

  ImGui::End();
}
