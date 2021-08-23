#pragma once
#include "event/InputEvents.h"
#include "System.h"
#include "SyxVec2.h"

//TODO: platform should forward events to the game, this forces inefficient polling
class KeyboardInputImpl {
public:
  virtual ~KeyboardInputImpl() = default;
  virtual KeyState getKeyState(Key key) const = 0;
  virtual void update() = 0;
  virtual Syx::Vec2 getMousePos() const = 0;
  virtual Syx::Vec2 getMouseDelta() const = 0;
  virtual float getWheelDelta() const = 0;
};

class KeyboardInput : public System {
public:
  KeyboardInput(const SystemArgs& args);
  ~KeyboardInput();

  void init() override;
  void update(float, IWorkerPool&, std::shared_ptr<Task> frameTask) override;

  KeyState getKeyState(const std::string& key) const;
  KeyState getKeyState(Key key) const;
  bool getKeyDown(Key key) const;
  bool getKeyDownOrTriggered(Key key) const;
  bool getKeyUp(Key key) const;
  bool getKeyTriggered(Key key) const;
  bool getKeyReleased(Key key) const;
  KeyState getAsciiState(char c) const;
  //Get mouse information in pixels
  Syx::Vec2 getMousePos() const;
  Syx::Vec2 getMouseDelta() const;
  float getWheelDelta() const;

private:
  KeyState _shiftAnd(Key key) const;
  KeyState _noShift(Key key) const;
  KeyState _or(KeyState a, KeyState b) const;

  KeyboardInputImpl* mPlatform = nullptr;
};
