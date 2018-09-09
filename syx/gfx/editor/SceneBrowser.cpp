#include "Precompile.h"
#include "editor/SceneBrowser.h"

#include "editor/event/EditorEvents.h"
#include "event/BaseComponentEvents.h"
#include "event/eventBuffer.h"
#include "graphics/RenderCommand.h"
#include "imgui/imgui.h"
#include "ImGuiImpl.h"
#include "LuaGameObject.h"
#include "provider/GameObjectHandleProvider.h"
#include "provider/MessageQueueProvider.h"
#include "system/KeyboardInput.h"

namespace {
  const Syx::Vec2 INVALID_MOUSE(-1);
  const size_t PICK_ID = std::hash<std::string>()("editor pick");
}

SceneBrowser::SceneBrowser(MessageQueueProvider* msg, GameObjectHandleProvider* handleGen, KeyboardInput* input)
  : mMsg(msg)
  , mHandleGen(handleGen)
  , mInput(input)
  , mMouseDownPos(INVALID_MOUSE) {
}

void SceneBrowser::editorUpdate(const HandleMap<std::unique_ptr<LuaGameObject>>& objects) {
  _updatePick();

  if(!ImGuiImpl::enabled()) {
    return;
  }
  ImGui::Begin("Objects");

  if(ImGui::Button("New Object")) {
    mSelected.clear();
    Handle newHandle = mHandleGen->newHandle();
    mSelected.insert(newHandle);
    auto msg = mMsg->getMessageQueue();
    msg.get().push(AddGameObjectEvent(newHandle));
    msg.get().push(SetSelectionEvent({ newHandle }));
  }

  if(ImGui::Button("Delete Object")) {
    auto msg = mMsg->getMessageQueue();
    for(Handle obj : mSelected) {
      const auto& it = objects.find(obj);
      if(it != objects.end()) {
        it->second->remove(msg.get());
      }
    }
    mSelected.clear();
    msg.get().push(SetSelectionEvent({}));
  }

  ImGui::BeginChild("ScrollView", ImVec2(0, 0), true);
  std::string name;
  for(const auto& it : objects) {
    const LuaGameObject& obj = *it.second;
    name = obj.getName().getName();
    //Make handle a hidden part of the id
    name += "##";
    name += std::to_string(obj.getHandle());
    if(ImGui::Selectable(name.c_str(), mSelected.find(obj.getHandle()) != mSelected.end())) {
      _clearForNewSelection();
      mSelected.insert(obj.getHandle());
      mMsg->getMessageQueue().get().push(SetSelectionEvent({ obj.getHandle() }));
    }
  }
  ImGui::EndChild();

  ImGui::End();
  _drawSelected();
}

void SceneBrowser::_drawSelected() {
  auto msg = mMsg->getMessageQueue();
  for(Handle h : mSelected) {
    msg.get().push(RenderCommandEvent(RenderCommand::outline(h, Syx::Vec3(1, 0, 0), 5)));
  }
}

void SceneBrowser::onPickResponse(const ScreenPickResponse& response) {
  if(response.mRequestId == PICK_ID) {
    _clearForNewSelection();
    for(Handle obj : response.mObjects) {
      mSelected.insert(obj);
    }
    mMsg->getMessageQueue().get().push(SetSelectionEvent(std::vector<Handle>(response.mObjects)));
  }
}

void SceneBrowser::_updatePick() {
  KeyState lmb = mInput->getKeyState(Key::LeftMouse);
  if(!ImGui::IsMouseHoveringAnyWindow() && lmb == KeyState::Triggered) {
    mMouseDownPos = mInput->getMousePos();
  }

  if((lmb == KeyState::Released || lmb == KeyState::Up) && mMouseDownPos != INVALID_MOUSE) {
    //TODO: set the space to something
    Handle space = 0;
    mMsg->getMessageQueue().get().push(ScreenPickRequest(PICK_ID, space, mMouseDownPos, mInput->getMousePos()));
    mMouseDownPos = INVALID_MOUSE;
  }
}

void SceneBrowser::_clearForNewSelection() {
  if(!mInput->getKeyDown(Key::Control) && !mInput->getKeyDown(Key::Shift)) {
    mSelected.clear();
  }
}
