#include "Precompile.h"
#include "editor/SceneBrowser.h"

#include "imgui/imgui.h"
#include "ImGuiImpl.h"
#include "LuaGameObject.h"

SceneBrowser::SceneBrowser(MessageQueueProvider* msg)
  : mMsg(msg) {
}

void SceneBrowser::editorUpdate(const HandleMap<std::unique_ptr<LuaGameObject>>& objects) {
  if(!ImGuiImpl::enabled()) {
    return;
  }
  ImGui::Begin("Objects");
  for(const auto& it : objects) {
    const LuaGameObject& obj = *it.second;
    ImGui::Text(obj.getName().getName().data());
  }
  ImGui::End();
}