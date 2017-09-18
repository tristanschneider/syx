#pragma once

class GraphicsSystem;
class KeyboardInput;
class System;
class Space;
enum class SystemId : uint8_t;

class App {
public:
  App();
  ~App();

  void init();
  void update(float dt);
  void uninit();
  Space& getDefaultSpace();

  template<typename SystemType>
  SystemType* getSystem(SystemId id) {
    return static_cast<SystemType*>(mSystems[static_cast<int>(id)].get());
  }

  //Temporary until asset manager that wraps asset loading and such
  std::unordered_map<std::string, Handle> mAssets;

private:
  //Construct and register derived type of System, and store the constructed object in systemPtr
  template<typename SystemType, typename... ArgTypes>
  void _registerSystem(ArgTypes&&... args) {
    std::unique_ptr<System> systemPtr = std::make_unique<SystemType>(std::forward<ArgTypes>(args)...);
    systemPtr->mApp = this;
    mSystems[static_cast<int>(systemPtr->getId())] = std::move(systemPtr);
  }

  std::vector<std::unique_ptr<System>> mSystems;
  std::unique_ptr<Space> mDefaultSpace;
};