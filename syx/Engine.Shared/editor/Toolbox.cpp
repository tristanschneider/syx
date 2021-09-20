#include "Precompile.h"
#include "Toolbox.h"

#include "editor/event/EditorEvents.h"
#include <event/EventBuffer.h>
#include <event/EventHandler.h>
#include "event/InputEvents.h"
#include <imgui/imgui.h>
#include "ImGuiImpl.h"
#include "input/InputStore.h"
#include "provider/MessageQueueProvider.h"

Toolbox::Toolbox(MessageQueueProvider& msg, EventHandler& handler)
  : mMsg(msg)
  , mCurrentPlayState(PlayState::Stopped)
  , mPostStepState(PlayState::Invalid) {

  mListeners.push_back(handler.registerEventListener([this](const SetPlayStateEvent& e) {
    mCurrentPlayState = e.mState;
    if(mCurrentPlayState == PlayState::Stepping && mPostStepState != PlayState::Invalid) {
      _requestPlayStateChange(mPostStepState);
      mPostStepState = PlayState::Invalid;
    }
  }));
}

void Toolbox::update(const InputStore& input) {
  _updateInput(input);
}

void Toolbox::editorUpdate(const InputStore& input) {
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

void Toolbox::_updateInput(const InputStore& input) {
  const bool ctrl = input.getKeyDownOrTriggered(Key::LeftCtrl) || input.getKeyDownOrTriggered(Key::RightCtrl);
  const bool shift = input.getKeyDownOrTriggered(Key::Shift);
  if(mCurrentPlayState == PlayState::Playing) {
    if(shift && input.getKeyTriggered(Key::F5))
      _stop();
    else if(input.getKeyTriggered(Key::F6)) {
      _pause();
    }
  }
  else {
    if(ctrl && shift && input.getKeyTriggered(Key::KeyS))
      _saveAs();
    else if(ctrl && input.getKeyTriggered(Key::KeyS))
      _save();
    else if(ctrl && input.getKeyTriggered(Key::KeyO))
      _open();
    else if(input.getKeyTriggered(Key::F5))
      _play();
    else if(input.getKeyTriggered(Key::F6) && mCurrentPlayState == PlayState::Paused)
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
