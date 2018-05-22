#pragma once
#include "event/Event.h"

//Built in scene ids. Further ids can be registered
enum class BuiltInSceneId : Handle {
  Editor,
  Game,
  Count
};

class RegisterSceneEvent : public Event {
public:
  RegisterSceneEvent(Handle sceneId);
  Handle mSceneId;
};

class ClearSceneEvent : public Event {
public:
  ClearSceneEvent(Handle sceneId);
  Handle mSceneId;
};
