#include "Precompile.h"
#include "CppUnitTest.h"

#include "AppRegistration.h"
#include "ecs/ECS.h"
#include "ecs/component/EditorComponents.h"
#include "ecs/component/FileSystemComponent.h"
#include "ecs/component/SpaceComponents.h"
#include "ecs/component/UriActivationComponent.h"
#include "ecs/system/editor/EditorSystem.h"
#include "test/TestAppContext.h"
#include "test/TestFileSystem.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace SystemTests {
  using namespace Engine;
  TEST_CLASS(EditorSystemTests) {
    struct TestRegistry {
      TestRegistry() {
        //Set up default components needed for the system similar to how App.cpp does
        auto global = mRegistry.getSingleton();
        auto fs = std::make_unique<FileSystem::TestFileSystem>();
        mFileSystem = fs.get();
        mRegistry.addComponent<FileSystemComponent>(global, std::move(fs));

        EditorSystem::init()->tick(mRegistry);
      }

      EntityRegistry* operator->() {
        return &mRegistry;
      }

      EntityRegistry& operator*() {
        return mRegistry;
      }

      EntityRegistry mRegistry;
      FileSystem::TestFileSystem* mFileSystem = nullptr;
    };

    TEST_METHOD(EditorSystem_UriActivation_ClearsAndLoadsSpace) {
      TestRegistry reg;
      reg.mFileSystem->addDirectory("test/dir/t.txt");
      auto message = reg->createEntity();
      reg->addComponent<UriActivationComponent>(message, UriActivationComponent{ "loadScene=test/dir/t.txt" });

      EditorSystem::createUriListener()->tick(*reg);

      auto clearSpace = reg->begin<ClearSpaceComponent>();
      auto loadSpace = reg->begin<LoadSpaceComponent>();
      Assert::IsFalse(clearSpace == reg->end<ClearSpaceComponent>());
      Assert::IsFalse(loadSpace == reg->end<LoadSpaceComponent>());
      Assert::IsTrue(reg->isValid(clearSpace->mSpace));
      Assert::AreEqual(std::string("test/dir/t.txt"), std::string(loadSpace->mToLoad.cstr()));
      Assert::IsTrue(reg->isValid(loadSpace->mSpace));
    }

    TEST_METHOD(EditorSystem_UriActivationFileDoesntExist_NothingHappens) {
      TestRegistry reg;
      auto message = reg->createEntity();
      reg->addComponent<UriActivationComponent>(message, UriActivationComponent{ "loadScene=test/dir/t.txt" });

      EditorSystem::createUriListener()->tick(*reg);

      auto clearSpace = reg->begin<ClearSpaceComponent>();
      auto loadSpace = reg->begin<LoadSpaceComponent>();
      Assert::IsTrue(clearSpace == reg->end<ClearSpaceComponent>());
      Assert::IsTrue(loadSpace == reg->end<LoadSpaceComponent>());
    }

    TEST_METHOD(DefaultAppRegistration_Init_HasEditorComponents) {
      TestAppContext context;
      TestRegistry reg;
      context->initialize(*reg);

      Assert::IsFalse(reg->begin<EditorContextComponent>() == reg->end<EditorContextComponent>());
      Assert::IsFalse(reg->begin<EditorSavedSceneComponent>() == reg->end<EditorSavedSceneComponent>());
      Assert::IsFalse(reg->begin<EditorSceneReferenceComponent>() == reg->end<EditorSceneReferenceComponent>());
    }
  };
}