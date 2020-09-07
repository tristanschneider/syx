#include "Precompile.h"
#include "CppUnitTest.h"

#include "App.h"
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
#include "test/TestFileSystem.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace LuaTests {
  TEST_CLASS(SceneSaveLoadTests) {
    std::unique_ptr<ILuaGameContext> _createGameContext(MockApp& app) {
      auto state = std::make_unique<Lua::State>();
      LuaGameSystem* gameSystem = app->getSystem<LuaGameSystem>();
      gameSystem->_openAllLibs(state->get());
      return Lua::createGameContext(*gameSystem, std::move(state));
    }

    void _clearSpace(Handle space, MockApp& app) {
      app->getMessageQueue().get().push(ClearSpaceEvent(space));
      app->update(1.f);
    }

    std::string _saveSpace(Handle space, FilePath file, MockApp& app) {
      app->getMessageQueue().get().push(SaveSpaceEvent(space, file));
      app->update(1.f);
      auto& fs = static_cast<FileSystem::TestFileSystem&>(app->getFileSystem());
      auto found = fs.mFiles.find(file.cstr());
      Assert::IsTrue(found != fs.mFiles.end(), L"File for saved scene should exist after saving", LINE_INFO());
      return found != fs.mFiles.end() ? found->second : "";
    }

    TEST_METHOD(Scene_SaveLoadEmptyScene_FileExists) {
      MockApp app;
      _saveSpace(0, "space", app);
    }

    TEST_METHOD(Scene_ClearScene_ObjectsRemoved) {
      MockApp app;
      auto context = _createGameContext(app);
      const Handle createdObject = context->addGameObject().getHandle();

      _clearSpace(0, app);

      context->clearCache();
      Assert::IsNull(context->getGameObject(createdObject), L"Created object should have been destroyed when the scene was cleared", LINE_INFO());
    }
  };
}