#include "Precompile.h"
#include "KeyboardInput.h"
#include <Windows.h>

KeyState KeyboardInput::getKeyState(int key) {
  //Fake implementation
  return GetAsyncKeyState(key) != 0 ? KeyStateDown : KeyStateUp;
}
