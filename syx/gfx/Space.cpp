#include "Precompile.h"
#include "Space.h"
#include "Gameobject.h"

Space::~Space() {
}

void Space::init() {
  for(Gameobject& g : mObjects.getBuffer())
    g.init();
}

void Space::update(float dt) {
  for(Gameobject& g : mObjects.getBuffer())
    g.update(dt);
}

void Space::uninit() {
  for(Gameobject& g : mObjects.getBuffer())
    g.uninit();
}
