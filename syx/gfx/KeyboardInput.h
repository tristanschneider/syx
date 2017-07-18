#pragma once

enum KeyState {
  KeyStateUp,
  KeyStateDown,
  KeyStateOnUp,
  KeyStateOnDown
};

class KeyboardInput {
public:
  KeyState getKeyState(int key);
private:
};