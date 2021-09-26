#pragma once

#include "event/Event.h"

class LuaGameSystemObserver;

//TODO: this is a hack to get the editor hooked into the LuaGameSystem tick
//it would be better to find a way to remove the dependency entirely, perhaps through the editor maintaining its own game context
struct AddGameObserver : public TypedEvent<AddGameObserver> {
  AddGameObserver(std::shared_ptr<LuaGameSystemObserver> observer)
    : mObserver(observer) {
  }

  std::shared_ptr<LuaGameSystemObserver> mObserver;
};