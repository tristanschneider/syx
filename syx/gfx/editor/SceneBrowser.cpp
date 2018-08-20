#include "Precompile.h"
#include "editor/SceneBrowser.h"

#include "editor/event/EditorEvents.h"
#include "event/eventBuffer.h"
#include "imgui/imgui.h"
#include "ImGuiImpl.h"
#include "LuaGameObject.h"
#include "provider/MessageQueueProvider.h"

SceneBrowser::SceneBrowser(MessageQueueProvider* msg)
  : mMsg(msg) {
}

void SceneBrowser::editorUpdate(const HandleMap<std::unique_ptr<LuaGameObject>>& objects) {
  if(!ImGuiImpl::enabled()) {
    return;
  }
  ImGui::Begin("Objects");
  ImGui::BeginChild("ScrollView", ImVec2(50, 200));
  std::string name;
  for(const auto& it : objects) {
    const LuaGameObject& obj = *it.second;
    name = obj.getName().getName();
    //Make handle a hidden part of the id
    name += "##";
    name += std::to_string(obj.getHandle());
    if(ImGui::Selectable(name.c_str(), mSelected == obj.getHandle())) {
      mSelected = obj.getHandle();
      mMsg->getMessageQueue().get().push(PickObjectEvent(mSelected));
    }
  }
  ImGui::EndChild();
  ImGui::End();
}