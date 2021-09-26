#include "Precompile.h"
#include "AssetPreview.h"

#include "asset/Asset.h"
#include "asset/Texture.h"
#include "editor/Editor.h"
#include "editor/event/EditorEvents.h"
#include "event/AssetEvents.h"
#include <event/EventHandler.h>
#include "provider/MessageQueueProvider.h"
#include "ImGuiImpl.h"
#include <imgui/imgui.h>


AssetPreview::~AssetPreview() = default;

AssetPreview::AssetPreview(MessageQueueProvider& msg, EventHandler& handler)
  : mMsg(msg) {

  mListeners.push_back(handler.registerEventListener([this](const PreviewAssetEvent& e) {
    mMsg.getMessageQueue()->push(GetAssetRequest(e.mAsset).then(typeId<Editor, System>(), [this](const GetAssetResponse& r) {
      mPreview = r.mAsset;
    }));
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
