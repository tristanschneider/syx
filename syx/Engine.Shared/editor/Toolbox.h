#pragma once

enum class PlayState : uint8_t;

class EventHandler;
struct EventListener;
class InputStore;
class MessageQueueProvider;

class Toolbox {
public:
  Toolbox(MessageQueueProvider& msg, EventHandler& handler);

  //Updates during play state
  void update(const InputStore& input);
  //Updates during edit state
  void editorUpdate(const InputStore& input);

private:
  void _updateGui();
  void _updateInput(const InputStore& input);
  void _open();
  void _save();
  void _saveAs();
  void _play();
  void _pause();
  void _stop();
  void _step();
  void _requestPlayStateChange(PlayState newState) const;

  std::vector<std::shared_ptr<EventListener>> mListeners;
  MessageQueueProvider& mMsg;
  PlayState mCurrentPlayState;
  //State to return to after step if it was initiated by Toolbox
  PlayState mPostStepState;
};