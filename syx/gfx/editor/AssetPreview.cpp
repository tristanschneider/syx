#include "Precompile.h"
#include "AssetPreview.h"

#include "asset/Asset.h"
#include "editor/event/EditorEvents.h"
#include <event/EventHandler.h>
#include "provider/MessageQueueProvider.h"
#include "system/AssetRepo.h"
#include "ImGuiImpl.h"
#include <imgui/imgui.h>

AssetPreview::AssetPreview(MessageQueueProvider& msg, EventHandler& handler, AssetRepo& assets)
  : mMsg(msg)
  , mAssets(assets) {

  handler.registerEventHandler<PreviewAssetEvent>([this](const PreviewAssetEvent& e) {
    mPreview = !e.mAsset.isEmpty() ? mAssets.getAsset(e.mAsset) : nullptr;
    //TODO: request asset thumbnail if necessary
  });

  //TODO: register for a response event to generate assets for previews
}

void AssetPreview::editorUpdate() {
  if(!ImGuiImpl::enabled())
    return;

  ImGui::Begin("Preview");
  if(mPreview && mPreview->isReady()) {
    auto lock = mPreview->getLock().getReader();
    ImGui::Text(mPreview->getInfo().mUri.c_str());
    //TODO: show asset here
  }
  else {
    ImGui::Text("None");
  }

  ImGui::End();
}
