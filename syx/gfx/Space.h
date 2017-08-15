#pragma once
#include "MappedBuffer.h"
#include "Gameobject.h"

class Gameobject;
class App;

class Space {
public:
  Space(App& app);
  ~Space();

  void init();
  void update(float dt);
  void uninit();

  Gameobject* createObject();

  App& getApp();

  MappedBuffer<Gameobject> mObjects;

private:
  HandleGen mObjectGen;
  App* mApp;
};