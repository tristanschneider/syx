#pragma once

enum class PlayState : uint8_t;

class EventHandler;
class KeyboardInput;
class MessageQueueProvider;

class Toolbox {
public:
  Toolbox(MessageQueueProvider& msg, EventHandler& handler);

  void editorUpdate(const KeyboardInput& input);

private:
  void _updateGui();
  void _updateInput(const KeyboardInput& input);
  void _open();
  void _save();
  void _saveAs();
  void _play();
  void _pause();
  void _stop();
  void _step();
  void _requestPlayStateChange(PlayState newState) const;

  MessageQueueProvider& mMsg;
  PlayState mCurrentPlayState;
  //State to return to after step if it was initiated by Toolbox
  PlayState mPostStepState;
};