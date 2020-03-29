#include "Precompile.h"
#include "editor/SceneBrowser.h"

#include "Camera.h"
#include "editor/Editor.h"
#include "editor/event/EditorEvents.h"
#include <event/EventHandler.h>
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
  const Syx::Vec3 PICK_COLOR(1, 0, 0);
}

SceneBrowser::SceneBrowser(MessageQueueProvider& msg, GameObjectHandleProvider& handleGen, KeyboardInput& input, EventHandler&)
  : mMsg(&msg)
  , mHandleGen(&handleGen)
  , mInput(&input)
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
    msg.get().push(RenderCommandEvent(RenderCommand::outline(h, PICK_COLOR, 5)));
  }

  KeyState lmb = mInput->getKeyState(Key::LeftMouse);
  Syx::Vec2 mousePos = mInput->getMousePos();
  if(lmb == KeyState::Down && mMouseDownPos != INVALID_MOUSE && mousePos != mMouseDownPos) {
    Syx::Vec2 min = mMouseDownPos;
    Syx::Vec2 max = mInput->getMousePos();
    for(int i = 0; i < 2; ++i) {
      if(min[i] > max[i])
        std::swap(min[i], max[i]);
    }
    Syx::Vec3 color = PICK_COLOR;
    color.w = 0.3f;
    msg.get().push(RenderCommandEvent(RenderCommand::quad2d(min, max, color, RenderCommand::Space::ScreenPixel)));
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
    const Syx::Vec2 mouseDownPos = mMouseDownPos;
    const Syx::Vec2 mouseUpPos = mInput->getMousePos();
    //Get the camera at the mouse, then pick with that camera
    mMsg->getMessageQueue().get().push(GetCameraRequest(mouseDownPos, GetCameraRequest::CoordSpace::Pixel).then(typeId<Editor>(), [this, space, mouseDownPos, mouseUpPos](const GetCameraResponse& getCam) {
      if(getCam.mCamera.isValid()) {
        mMsg->getMessageQueue().get().push(ScreenPickRequest(PICK_ID, getCam.mCamera.getOps().mOwner, space, mouseDownPos, mouseUpPos).then(typeId<Editor>(), [this](const ScreenPickResponse& res) {
          _clearForNewSelection();
          for(Handle obj : res.mObjects) {
            mSelected.insert(obj);
          }
          mMsg->getMessageQueue().get().push(SetSelectionEvent(std::vector<Handle>(res.mObjects)));
        }));
      }
    }));
    mMouseDownPos = INVALID_MOUSE;
  }
}

void SceneBrowser::_clearForNewSelection() {
  if(!mInput->getKeyDown(Key::Control) && !mInput->getKeyDown(Key::Shift)) {
    mSelected.clear();
  }
}
