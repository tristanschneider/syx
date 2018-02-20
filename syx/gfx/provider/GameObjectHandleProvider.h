#pragma once
class GameObjectHandleProvider {
public:
  //get a new unique handle for a game object. Threadsafe
  virtual Handle newHandle() = 0;
};