#pragma once
class GameObjectHandleProvider {
  //get a new unique handle for a game object. Threadsafe
  virtual Handle newHandle() = 0;
};