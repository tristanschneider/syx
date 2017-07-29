#pragma once

class GraphicsSystem;
class KeyboardInput;
class System;

class App {
public:
  enum class SystemId {
    Graphics,
    KeyboardInput,
    Count
  };

  App();
  ~App();

  void init();
  void update(float dt);
  void uninit();

  template<typename SystemType>
  SystemType& getSystem(SystemId id) {
    return static_cast<SystemType&>(*mSystems[static_cast<int>(id)]);
  }

private:
  //Construct and register derived type of System, and store the constructed object in systemPtr
  template<typename SystemType, typename... ArgTypes>
  void _registerSystem(std::unique_ptr<SystemType>& systemPtr, SystemId id, ArgTypes&&... args) {
    systemPtr = std::make_unique<SystemType>(std::forward<ArgTypes>(args)...);
    mSystems[static_cast<int>(id)] = static_cast<System*>(systemPtr.get());
  }

  std::unique_ptr<GraphicsSystem> mGraphics;
  std::unique_ptr<KeyboardInput> mKeyboardInput;
  std::vector<System*> mSystems;
};