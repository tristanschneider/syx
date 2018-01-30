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
  Guarded<HandleMap<Gameobject>> getObjects();

private:
  HandleMap<Gameobject> mObjects;
  std::mutex mObjectsMutex;
  HandleGen mObjectGen;
  App* mApp;
};