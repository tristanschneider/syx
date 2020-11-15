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
#include "test/TestAppRegistration.h"
#include "test/TestFileSystem.h"
#include "threading/AsyncHandle.h"

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

    std::shared_ptr<IAsyncHandle<bool>> _loadSpace(Handle targetSpace, FilePath toLoad, MockApp& app) {
      LoadSpaceEvent loadSpace(targetSpace, toLoad);
      auto result = Async::createAsyncHandle<bool>();
      loadSpace.then(LuaRegistration::TEST_CALLBACK_ID, [result, alreadyComplete(std::make_shared<bool>(false))](bool success) {
        Async::setComplete(*result, success);
        Assert::IsFalse(*alreadyComplete, L"Task completion should only trigger once");
        *alreadyComplete = true;
      });

      app->getMessageQueue()->push(std::move(loadSpace));

      return result;
    }

    void _reloadSpace(Handle space, MockApp& app) {
      const FilePath path = "test";
      _saveSpace(space, path, app);
      _clearSpace(space, app);

      auto loadTask = _loadSpace(space, path, app);

      app.waitUntil([&loadTask] {
        return loadTask->getStatus() == AsyncStatus::Complete;
      });
      Assert::IsTrue(*loadTask->getResult(), L"Loading the scene should have worked");
    }

    const LuaGameObject* _getObject(const UniqueID& id, MockApp& app) {
      const auto& objs = app->getSystem<LuaGameSystem>()->getObjects();
      const auto it = std::find_if(objs.begin(), objs.end(), [&id](const auto& pair) {
        return pair.second->getUniqueID() == id;
      });
      return it != objs.end() ? it->second.get() : nullptr;
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
      const UniqueID createdUniqueID = createdObject.getUniqueID();
      IComponent& name = *createdObject.getComponent({ Component::typeId<NameComponent>(), 0 });
      name.set([&name] {
         NameComponent temp = name.get<NameComponent>();
         temp.setName("test");
         return temp;
      }());

      //This particular order is required to ensure that the claimed id is released before the scene is loaded. It likely indicates a problem that I'll need to address when continuing with the design of UniqueID
      context->clearCache();
      _reloadSpace(0, app);

      const LuaGameObject* reloadedObject = _getObject(createdUniqueID, app);
      Assert::IsNotNull(reloadedObject, L"Object should have been reloaded with the same unique id", LINE_INFO());

      const NameComponent* reloadedName = reloadedObject->getComponent<NameComponent>();
      Assert::IsNotNull(reloadedName, L"Name component should have been reloaded", LINE_INFO());
      Assert::IsTrue(reloadedName->getName() == "test", L"Name should retain saved value", LINE_INFO());
    }

    TEST_METHOD(Scene_LoadSceneWithAsset_AssetIsLoaded) {

    }
  };
}