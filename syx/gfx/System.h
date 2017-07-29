#pragma once

class System {
public:
  virtual void init() = 0;
  virtual void update(float dt) = 0;
  virtual void uninit() = 0;
};