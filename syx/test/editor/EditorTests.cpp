#include "Precompile.h"
#include "CppUnitTest.h"

#include "App.h"
#include "editor/Editor.h"
#include "event/BaseComponentEvents.h"
#include "event/SpaceEvents.h"
#include "event/EventBuffer.h"
#include "LuaGameObject.h"
#include "lua/LuaGameContext.h"
#include "lua/LuaState.h"
#include "lua/LuaVariant.h"
#include "system/AssetRepo.h"
#include "system/LuaGameSystem.h"
#include "SyxVec2.h"
#include "test/MockApp.h"
#include "test/TestAppPlatform.h"
#include "test/TestAppRegistration.h"
#include "test/TestFileSystem.h"
#include "test/TestGUIHook.h"
#include "test/TestListenerSystem.h"
#include "threading/AsyncHandle.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace EditorTests {
  TEST_CLASS(BaseEditorTests) {
    struct MockEditorApp : public MockApp {
      MockEditorApp()
        : MockApp(std::make_unique<TestAppPlatform>(), TestRegistration::createEditorRegistration()) {
      }

      void processEditorInput() {
        //The first update will process whatever the desired input was, the second will process whatever event the input sent
        mApp->update(0.f);
        mApp->update(0.f);
      }

      void pressButtonAndProcessInput(const std::string& label) {
        {
          auto gui = Create::createAndRegisterTestGuiHook();
          auto press = gui->addScopedButtonPress(label);
          mApp->update(0.f);
        }
        mApp->update(0.f);
      }
    };

    std::unique_ptr<ILuaGameContext> _createGameContext(MockApp& app) {
      return Lua::createGameContext(*app->getSystem<LuaGameSystem>());
    }

    const LuaGameObject* _getObject(const UniqueID& id, MockApp& app) {
      const auto& objs = app->getSystem<LuaGameSystem>()->getObjects();
      const auto it = std::find_if(objs.begin(), objs.end(), [&id](const auto& pair) {
        return pair.second->getUniqueID() == id;
      });
      return it != objs.end() ? it->second.get() : nullptr;
    }

    TEST_METHOD(Editor_Update_NothingHappens) {
      MockEditorApp app;
      Assert::IsNotNull(app->getSystem<Editor>(), L"Editor should exist", LINE_INFO());

      app->update(0.f);
    }

    TEST_METHOD(EmptyScene_PressAddObject_ObjectIsAdded) {
      MockEditorApp app;
      const size_t prevObjects = app->getSystem<LuaGameSystem>()->getObjects().size();

      app.pressButtonAndProcessInput("New Object");

      Assert::IsTrue(app->getSystem<TestListenerSystem>()->hasEventOfType(Event::typeId<AddGameObjectEvent>()), L"Pressing new object should have sent an AddGameObjectEvent");
      const size_t currentObjects = app->getSystem<LuaGameSystem>()->getObjects().size();
      Assert::IsTrue(currentObjects > prevObjects, L"New object should have been created");
    }
  };
}