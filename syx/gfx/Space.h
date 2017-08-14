#pragma once
#include "MappedBuffer.h"
#include "Gameobject.h"

class Space {
public:
  ~Space();

  void init();
  void update(float dt);
  void uninit();

  MappedBuffer<Gameobject> mObjects;
};