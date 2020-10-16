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
      return Lua::createGameContext(*app->getSystem<LuaGameSystem>());
    }

    void _clearSpace(Handle space, MockApp& app) {
      app->getMessageQueue().get().push(ClearSpaceEvent(space));
      app->update(1.f);
    }

    std::string _saveSpace(Handle space, FilePath file, MockApp& app) {
      app->getMessageQueue()->push(SaveSpaceEvent(space, file));
      app->update(1.f);
      auto& fs = static_cast<FileSystem::TestFileSystem&>(app->getFileSystem());
      auto found = fs.mFiles.find(file.cstr());
      Assert::IsTrue(found != fs.mFiles.end(), L"File for saved scene should exist after saving", LINE_INFO());
      return found != fs.mFiles.end() ? found->second : "";
    }

    void _loadSpace(Handle targetSpace, FilePath toLoad, MockApp& app) {
      app->getMessageQueue()->push(LoadSpaceEvent(targetSpace, toLoad));
      app->update(1.f);
    }

    TEST_METHOD(Scene_SaveLoadEmptyScene_FileExists) {
      MockApp app;
      _saveSpace(0, "space", app);
    }

    TEST_METHOD(Scene_ClearScene_ObjectsRemoved) {
      MockApp app;
      auto context = _createGameContext(app);
      const Handle createdObject = context->addGameObject().getRuntimeID();

      _clearSpace(0, app);

      context->clearCache();
      Assert::IsNull(context->getGameObject(createdObject), L"Created object should have been destroyed when the scene was cleared", LINE_INFO());
    }

    TEST_METHOD(Scene_AccessBuiltInComponentsOnNewObject_ComponentsExist) {
      MockApp app;
      auto context = _createGameContext(app);
      IGameObject& createdObject = context->addGameObject();
      Assert::IsTrue(createdObject.getComponent({ Component::typeId<NameComponent>(), 0 }) != nullptr, L"Name should exist");
      Assert::IsTrue(createdObject.getComponent({ Component::typeId<SpaceComponent>(), 0 }) != nullptr, L"Space should exist");
      Assert::IsTrue(createdObject.getComponent({ Component::typeId<Transform>(), 0 }) != nullptr, L"Transform should exist");
    }

    TEST_METHOD(Scene_SaveLoadSingleObjectName_IsSame) {
      MockApp app;
      auto context = _createGameContext(app);
      IGameObject& createdObject = context->addGameObject();
      const Handle createdObjectHandle = createdObject.getRuntimeID();
      IComponent& name = *createdObject.getComponent({ Component::typeId<NameComponent>(), 0 });
      name.set([&name] {
         NameComponent temp = name.get<NameComponent>();
         temp.setName("test");
         return temp;
      }());
      //TODO save here, load, and then get the name again to see if it matches

      _saveSpace(0, "test", app);
      _clearSpace(0, app);

      context->clearCache();
      Assert::IsNull(context->getGameObject(createdObjectHandle), L"Created object should have been destroyed when the scene was cleared", LINE_INFO());
    }

    TEST_METHOD(Scene_SaveLoadSingleObject_IsSame) {

    }

    TEST_METHOD(Scene_LoadSceneWithAsset_AssetIsLoaded) {

    }
  };
}