#pragma once
#include "system/KeyboardInput.h"

class KeyboardInputWin32 : public KeyboardInput {
public:
  RegisterSystemH(KeyboardInput);
  using KeyboardInput::KeyboardInput;

  void init() override;
  void update(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) override;
  void uninit() override;

  void feedWheelDelta(float delta) override;

  KeyState getKeyState(const std::string& key) const override;
  KeyState getKeyState(Key key) const override;
  bool getKeyDown(Key key) const override;
  bool getKeyUp(Key key) const override;
  bool getKeyTriggered(Key key) const override;
  bool getKeyReleased(Key key) const override;
  KeyState getAsciiState(char c) const override;
  //Get mouse information in pixels
  Syx::Vec2 getMousePos() const override;
  Syx::Vec2 getMouseDelta() const override;
  float getWheelDelta() const override;

private:
  static const size_t sKeyCount = 256;
  static const size_t sAsciiCount = 128;

  KeyState _shiftAnd(Key key) const;
  KeyState _noShift(Key key) const;
  KeyState _or(KeyState a, KeyState b) const;

  uint8_t mPrevState[sKeyCount];
  uint8_t mCurState[sKeyCount];
  uint8_t mAsciiToKey[sAsciiCount];
  uint8_t mKeyToAscii[sAsciiCount];
  Syx::Vec2 mPrevMouse;
  Syx::Vec2 mCurMouse;
  float mPrevWheel;
  float mCurWheel;
};