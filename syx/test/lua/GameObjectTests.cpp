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
#include "event/SpaceEvents.h"
#include "event/EventBuffer.h"
#include "LuaGameObject.h"
#include "lua/LuaVariant.h"

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
      info.fill();
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

    const Lua::Variant* _getScriptProps(Handle obj, const std::string& script, MockApp& app) {
      const LuaGameObject* gameObject = app.get().getSystem<LuaGameSystem>()->getObject(obj);
      Assert::IsNotNull(gameObject, L"Game object should exist", LINE_INFO());
      if(obj) {
        AssetInfo info(script);
        info.fill();
        const LuaComponent* comp = gameObject->getLuaComponent(info.mId);
        Assert::IsNotNull(comp, L"Game object should have a LuaComponent", LINE_INFO());
        return comp ? &comp->getPropVariant() : nullptr;
      }
      return nullptr;
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

    TEST_METHOD(GameObject_PublicBool_SavedToProps) {
      MockApp app;
      const Handle objHandle = _addObjectWithScript(app.get(), "script", "a = true;");
      //Once for init, once for update
      app.get().update(1.0f);
      app.get().update(1.0f);

      if(const Lua::Variant* props = _getScriptProps(objHandle, "script", app)) {
        const Lua::Variant* a = props->getChild(Lua::Key("a"));
        Assert::IsNotNull(a, L"Object should have the 'a' property", LINE_INFO());
        if(a) {
          Assert::IsTrue(a->getTypeId() == typeId<bool>(), L"Property should be bool", LINE_INFO());
          Assert::IsTrue(a->get<bool>(), L"Property should be true", LINE_INFO());
        }
      }
    }

    template<class T>
    const Lua::Variant* _getAssertProp(const Lua::Variant& props, const Lua::Key& key) {
      const Lua::Variant* result = props.getChild(key);
      Assert::IsNotNull(result, (L"Prop should have been found: " + Util::toWide(key.toString())).c_str(), LINE_INFO());
      if(result) {
        const bool typesMatch = result->getTypeId() == typeId<T>();
        Assert::IsTrue(typesMatch, (L"Property should have expected type: " + Util::toWide(key.toString())).c_str());
        return typesMatch ? result : nullptr;
      }
      return nullptr;
    }

    TEST_METHOD(GameObject_PublicProps_AllAreSaved) {
      MockApp app;
      const Handle objHandle = _addObjectWithScript(app.get(), "script", R"(
          --Does nothing
          a = nil;
          b = 2;
          c = 3.5;
          d = "str";
          e = true;
          f = Vec3.unitX();
          --Equivalent to null
          g = {};
          h = { 1, 2, 3 };
          i = {
            ia = 1,
            ib = "two",
            ic = false
          };
          j = {
            ja = 2,
            jb = {
              jbs = "asdf"
            }
          };
        )");
      app.get().update(1.0f);
      app.get().update(1.0f);

      if(const Lua::Variant* props = _getScriptProps(objHandle, "script", app)) {
        if(const Lua::Variant* prop = _getAssertProp<double>(*props, Lua::Key("b"))) {
          Assert::AreEqual(prop->get<double>(), 2.0, 0.1, L"b should have expected value", LINE_INFO());
        }
        if(const Lua::Variant* prop = _getAssertProp<double>(*props, Lua::Key("c"))) {
          Assert::AreEqual(prop->get<double>(), 3.5, L"c should have expected value", LINE_INFO());
        }
        if(const Lua::Variant* prop = _getAssertProp<std::string>(*props, Lua::Key("d"))) {
          Assert::IsTrue(prop->get<std::string>() == "str", L"d should have expected value", LINE_INFO());
        }
        if(const Lua::Variant* prop = _getAssertProp<bool>(*props, Lua::Key("e"))) {
          Assert::IsTrue(prop->get<bool>(), L"e should have expected value", LINE_INFO());
        }
        if(const Lua::Variant* prop = _getAssertProp<Syx::Vec3>(*props, Lua::Key("f"))) {
          Assert::IsTrue(prop->get<Syx::Vec3>() == Syx::Vec3::UnitX, L"f should turn into a vec3 via the __typeNode function in lua", LINE_INFO());
        }
        if(const Lua::Variant* prop = _getAssertProp<void>(*props, Lua::Key("h"))) {
          for(int i = 0; i < 3; ++i) {
            if(const Lua::Variant* index = _getAssertProp<double>(*prop, Lua::Key(i + 1))) {
              Assert::AreEqual(index->get<double>(), static_cast<double>(i) + 1.0, 0.1, L"Index value should match", LINE_INFO());
            }
          }
        }
        if(const Lua::Variant* prop = _getAssertProp<void>(*props, Lua::Key("i"))) {
          if(const Lua::Variant* ia = _getAssertProp<double>(*prop, Lua::Key("ia"))) {
            Assert::AreEqual(ia->get<double>(), 1.0, 0.1, L"ia value should match");
          }
          if(const Lua::Variant* ib = _getAssertProp<std::string>(*prop, Lua::Key("ib"))) {
            Assert::AreEqual(ib->get<std::string>().c_str(), "two", L"ib should have matching string variant", LINE_INFO());
          }
          if(const Lua::Variant* ic = _getAssertProp<bool>(*prop, Lua::Key("ic"))) {
            Assert::IsFalse(ic->get<bool>(), L"ic value should match", LINE_INFO());
          }
        }
        if(const Lua::Variant* prop = _getAssertProp<void>(*props, Lua::Key("j"))) {
          if(const Lua::Variant* ja = _getAssertProp<double>(*prop, Lua::Key("ja"))) {
            Assert::AreEqual(ja->get<double>(), 2.0, 0.1, L"ja value should match", LINE_INFO());
          }
          if(const Lua::Variant* jb = _getAssertProp<void>(*prop, Lua::Key("jb"))) {
            if(const Lua::Variant* jbs = _getAssertProp<std::string>(*jb, Lua::Key("jbs"))) {
              Assert::AreEqual(jbs->get<std::string>().c_str(), "asdf", "jbs value should match", LINE_INFO());
            }
          }
        }
      }
    }

    TEST_METHOD(GameObject_SetTransformMat_TransformIsUpdated) {
      MockApp app;
      const Handle objHandle = _addObjectWithScript(app.get(), "script", R"(
        function initialize(self)
          self.transform.matrix = { 1, 0, 0, 0,
                                    0, 1, 0, 0,
                                    0, 0, 1, 5,
                                    0, 0, 0, 1 };
        end
        )");
      //Make timescale nonzero so scripts update
      app.mApp->getMessageQueue().get().push(SetTimescaleEvent(0, 1.0f));
      //Initialize script component
      app.get().update(1.0f);
      //Call initialize function
      app.get().update(1.0f);
      //Apply events sent by initialization
      app.get().update(1.0f);
      const LuaGameObject* obj = app.mApp->getSystem<LuaGameSystem>()->getObject(objHandle);
      Assert::IsNotNull(obj, L"Gameobject should exist", LINE_INFO());
      Assert::IsTrue(obj && obj->getComponent<Transform>()->get() == Syx::Mat4(1, 0, 0, 0,
                                                                               0, 1, 0, 0,
                                                                               0, 0, 1, 5,
                                                                               0, 0, 0, 1), L"Transform matrix should have been set by lua", LINE_INFO());
    }

    TEST_METHOD(GameObject_UseTransformMethod_TransformIsUpdated) {
      MockApp app;
      const Handle objHandle = _addObjectWithScript(app.get(), "script", R"(
        function initialize(self)
          self.transform:setTranslate(Vec3.new3(0, 5, 0));
        end
      )");
      app.mApp->getMessageQueue().get().push(SetTimescaleEvent(0, 1.0f));
      for(int i = 0; i < 3; ++i) {
        app.get().update(1.0f);
      }

      const LuaGameObject* obj = app.mApp->getSystem<LuaGameSystem>()->getObject(objHandle);
      Assert::IsNotNull(obj, L"Gameobject should exist", LINE_INFO());
      Assert::IsTrue(obj && obj->getComponent<Transform>()->get().getTranslate() == Syx::Vec3(0, 5, 0), L"Translate should have been updated by script", LINE_INFO());
    }

    TEST_METHOD(GameObject_SetTransformTwice_BothChangesPreserved) {
      MockApp app;
      const Handle objHandle = _addObjectWithScript(app.get(), "script", R"(
        function initialize(self)
          self.transform:setTranslate(Vec3.new3(0, 5, 0));
          self.transform:setScale(Vec3.new3(1, 2, 3));
        end
      )");
      app.mApp->getMessageQueue().get().push(SetTimescaleEvent(0, 1.0f));
      for(int i = 0; i < 3; ++i) {
        app.get().update(1.0f);
      }

      const LuaGameObject* obj = app.mApp->getSystem<LuaGameSystem>()->getObject(objHandle);
      Assert::IsNotNull(obj, L"Gameobject should exist", LINE_INFO());
      if(obj) {
        const Transform* transform = obj->getComponent<Transform>();
        Assert::IsTrue(transform->get().getTranslate() == Syx::Vec3(0, 5, 0), L"Translate should have been updated by script", LINE_INFO());
        Assert::IsTrue(transform->get().getScale() == Syx::Vec3(1, 2, 3), L"Scale should have been updated by script", LINE_INFO());
      }
    }

    TEST_METHOD(GameObject_AddComponent_HasComponent) {
    }
  };
}