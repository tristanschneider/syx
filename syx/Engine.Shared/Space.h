#pragma once
#include "component/SpaceComponent.h"

struct lua_State;

class Space {
public:
  Space(Handle id);

  float getTimescale() const;
  void setTimescale(float timescale);

  int push(lua_State* l);

private:
  SpaceComponent mInstance;
  float mTimescale;
};