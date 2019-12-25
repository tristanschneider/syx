#include "Precompile.h"
#include "Toolbox.h"

#include "editor/event/EditorEvents.h"
#include <event/EventBuffer.h>
#include <event/EventHandler.h>
#include <imgui/imgui.h>
#include "ImGuiImpl.h"
#include "provider/MessageQueueProvider.h"
#include "system/KeyboardInput.h"

Toolbox::Toolbox(MessageQueueProvider& msg, EventHandler& handler)
  : mMsg(msg)
  , mCurrentPlayState(PlayState::Stopped)
  , mPostStepState(PlayState::Invalid) {

  handler.registerEventHandler<SetPlayStateEvent>([this](const SetPlayStateEvent& e) {
    mCurrentPlayState = e.mState;
    if(mCurrentPlayState == PlayState::Stepping && mPostStepState != PlayState::Invalid) {
      _requestPlayStateChange(mPostStepState);
      mPostStepState = PlayState::Invalid;
    }
  });
}

void Toolbox::editorUpdate(const KeyboardInput& input) {
  if(!ImGuiImpl::enabled())
    return;
  _updateGui();
  _updateInput(input);
}

void Toolbox::_updateGui() {
  ImGui::Begin("Toolbox", nullptr, ImGuiWindowFlags_MenuBar);
  if(ImGui::BeginMenuBar()) {
      if(ImGui::BeginMenu("File")) {
          if(ImGui::MenuItem("Open", "Ctrl+O")) {
            _open();
          }
          if(ImGui::MenuItem("Save", "Ctrl+S")) {
            _save();
          }
          if(ImGui::MenuItem("Save As", "Ctrl+Shift+S")) {
            _saveAs();
          }
          ImGui::EndMenu();
      }

      ImGui::EndMenuBar();
  }

  if(mCurrentPlayState == PlayState::Stopped || mCurrentPlayState == PlayState::Paused) {
    if(ImGui::Button("Play"))
      _play();
  }
  if(mCurrentPlayState == PlayState::Stepping || mCurrentPlayState == PlayState::Playing) {
    if(ImGui::Button("Pause"))
      _pause();
  }
  if(mCurrentPlayState == PlayState::Paused) {
    ImGui::SameLine();
    if(ImGui::Button("Step"))
      _step();
  }
  if(mCurrentPlayState == PlayState::Playing || mCurrentPlayState == PlayState::Stepping || mCurrentPlayState == PlayState::Paused) {
    ImGui::SameLine();
    if(ImGui::Button("Stop"))
      _stop();
  }

  ImGui::End();
}

void Toolbox::_updateInput(const KeyboardInput& input) {
  const bool ctrl = input.getKeyDown(Key::LeftCtrl) || input.getKeyDown(Key::RightCtrl);
  const bool shift = input.getKeyDown(Key::Shift);
  if(ctrl && shift && input.getKeyTriggered(Key::KeyS))
    _saveAs();
  else if(ctrl && input.getKeyTriggered(Key::KeyS))
    _save();
  else if(ctrl && input.getKeyTriggered(Key::KeyO))
    _open();
  else if(shift && input.getKeyTriggered(Key::F5))
    _stop();
  else if(input.getKeyTriggered(Key::F5))
    _play();
  else if(input.getKeyTriggered(Key::F6)) {
    if(mCurrentPlayState == PlayState::Playing)
      _pause();
    else if(mCurrentPlayState == PlayState::Paused)
      _step();
  }
}

void Toolbox::_open() {

}

void Toolbox::_save() {

}

void Toolbox::_saveAs() {

}

void Toolbox::_play() {
  _requestPlayStateChange(PlayState::Playing);
}

void Toolbox::_pause() {
  _requestPlayStateChange(PlayState::Paused);
}

void Toolbox::_stop() {
  _requestPlayStateChange(PlayState::Stopped);
}

void Toolbox::_step() {
  mPostStepState = mCurrentPlayState;
  _requestPlayStateChange(PlayState::Stepping);
}

void Toolbox::_requestPlayStateChange(PlayState newState) const {
  mMsg.getMessageQueue().get().push(SetPlayStateEvent(newState));
}
