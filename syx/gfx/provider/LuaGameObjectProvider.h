#pragma once

class LuaGameObject;

class LuaGameObjectProvider {
public:
  virtual const LuaGameObject* getObject(Handle handle) const = 0;
};