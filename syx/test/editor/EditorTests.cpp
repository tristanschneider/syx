#include "Precompile.h"
#include "CppUnitTest.h"

#include "App.h"
#include "editor/Editor.h"
#include "system/AssetRepo.h"
#include "system/LuaGameSystem.h"
#include "SyxVec2.h"
#include "event/BaseComponentEvents.h"
#include "event/SpaceEvents.h"
#include "event/EventBuffer.h"
#include "LuaGameObject.h"
#include "lua/LuaGameContext.h"
#include "lua/LuaState.h"
#include "lua/LuaVariant.h"
#include "test/MockApp.h"
#include "test/TestAppPlatform.h"
#include "test/TestAppRegistration.h"
#include "test/TestFileSystem.h"
#include "threading/AsyncHandle.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace EditorTests {
  TEST_CLASS(BaseEditorTests) {
    struct MockEditorApp : public MockApp {
      MockEditorApp()
        : MockApp(std::make_unique<TestAppPlatform>(), TestRegistration::createEditorRegistration()) {
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
  };
}