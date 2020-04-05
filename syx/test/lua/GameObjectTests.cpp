#include "Precompile.h"
#include "CppUnitTest.h"

#include "App.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#include "AppPlatform.h"
#include "AppRegistration.h"
#include "file/FilePath.h"
#include "file/DirectoryWatcher.h"
#include "system/AssetRepo.h"
#include "system/KeyboardInput.h"
#include "system/LuaGameSystem.h"
#include "SyxVec2.h"
#include "asset/LuaScript.h"
#include "event/BaseComponentEvents.h"
#include "event/EventBuffer.h"
#include "LuaGameObject.h"

class TestKeyboardInputImpl : public KeyboardInputImpl {
public:
  virtual KeyState getKeyState(Key key) const override;
  virtual void update() override;
  virtual Syx::Vec2 getMousePos() const override;
  virtual Syx::Vec2 getMouseDelta() const override;
  virtual float getWheelDelta() const override;
};

KeyState TestKeyboardInputImpl::getKeyState(Key) const {
  return KeyState::Up;
}

void TestKeyboardInputImpl::update() {
}

Syx::Vec2 TestKeyboardInputImpl::getMousePos() const {
  return Syx::Vec2(0, 0);
}

Syx::Vec2 TestKeyboardInputImpl::getMouseDelta() const {
  return Syx::Vec2(0, 0);
}

float TestKeyboardInputImpl::getWheelDelta() const {
  return 0.0f;
}

class TestAppPlatform : public AppPlatform {
public:
  TestAppPlatform();
  virtual std::string getExePath() override;
  virtual void setWorkingDirectory(const char* working) override;
  virtual std::unique_ptr<DirectoryWatcher> createDirectoryWatcher(FilePath root) override;
  virtual KeyboardInputImpl& getKeyboardInput() override;
private:
  std::unique_ptr<KeyboardInputImpl> mKeyboardInput;
};

TestAppPlatform::TestAppPlatform()
  : mKeyboardInput(std::make_unique<TestKeyboardInputImpl>()) {
}

std::string TestAppPlatform::getExePath() {
  return {};
}

void TestAppPlatform::setWorkingDirectory(const char*) {
}

std::unique_ptr<DirectoryWatcher> TestAppPlatform::createDirectoryWatcher(FilePath) {
  return std::make_unique<DirectoryWatcher>();
}

KeyboardInputImpl& TestAppPlatform::getKeyboardInput() {
  return *mKeyboardInput;
}

namespace LuaTests {
  class LuaRegistration : public AppRegistration {
    virtual void registerSystems(const SystemArgs& args, ISystemRegistry& registry) override {
      registry.registerSystem(std::make_unique<AssetRepo>(args, Registry::createAssetLoaderRegistry()));
      registry.registerSystem(std::make_unique<LuaGameSystem>(args));
    }
  };

  struct MockApp {
    MockApp()
      : mApp(std::make_unique<App>(std::make_unique<TestAppPlatform>(), std::make_unique<LuaRegistration>())) {
      mApp->init();
    }

    ~MockApp() {
      mApp->uninit();
    }

    App& get() {
      return *mApp;
    }

    std::unique_ptr<App> mApp;
  };

  TEST_CLASS(GameObjectTests) {
  public:
    AssetInfo _addScript(App& app, std::string scriptName, std::string script) {
      AssetInfo info(std::move(scriptName));
      auto scriptAsset = AssetRepo::createAsset<LuaScript>(AssetInfo(info));
      scriptAsset->set(std::move(script));
      app.getSystem<AssetRepo>()->addAsset(std::move(scriptAsset));
      return info;
    }

    Handle _addObjectWithScript(App& app, std::string scriptName, std::string script) {
      //Add script to asset repo
      const AssetInfo info = _addScript(app, std::move(scriptName), std::move(script));
      //Create empty object
      LuaGameObject& newObj = app.getSystem<LuaGameSystem>()->addGameObject();
      //Configure local version of object
      LuaComponent newComp(newObj.getHandle());
      newComp.setScript(info.mId);
      //Send messages to replicate local object
      newComp.addSync(app.getMessageQueue());
      return newObj.getHandle();
    }

    TEST_METHOD(GameObject_EmptyGame_DoesntCrash) {
      MockApp().get().update(1.0f);
    }

    TEST_METHOD(GameObject_AddScript_HasScriptComponent) {
      MockApp app;
      const Handle objHandle = _addObjectWithScript(app.get(), "script", {});
      app.get().update(1.0f);

      const LuaGameObject* obj = app.get().getSystem<LuaGameSystem>()->getObject(objHandle);
      Assert::IsNotNull(obj, L"Object should have been added in _addObjectWithScript", LINE_INFO());
      if(obj) {
        Assert::IsNotNull(obj->getComponent("script"), L"Script should have been added to game object", LINE_INFO());
      }
    }
  };
}