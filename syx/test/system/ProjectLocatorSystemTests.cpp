#include "Precompile.h"
#include "CppUnitTest.h"

#include "AppRegistration.h"
#include "ecs/ECS.h"
#include "ecs/component/AppPlatformComponents.h"
#include "ecs/component/FileSystemComponent.h"
#include "ecs/component/MessageComponent.h"
#include "ecs/component/ProjectLocatorComponent.h"
#include "ecs/component/UriActivationComponent.h"
#include "ecs/system/ProjectLocatorSystem.h"
#include "test/TestFileSystem.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace SystemTests {
  using namespace Engine;
  TEST_CLASS(ProjectLocatorSystemTests) {
    struct TestEntityRegistry : public Engine::EntityRegistry {
      using Base = Engine::EntityRegistry;

      auto createEntity() {
        return Base::createEntity(*getDefaultEntityGenerator());
      }

      void destroyEntity(Engine::Entity entity) {
        Base::destroyEntity(entity, *getDefaultEntityGenerator());
      }

      template<class... Args>
      auto createEntityWithComponents() {
        return Base::createEntityWithComponents<Args...>(*getDefaultEntityGenerator());
      }

      template<class... Args>
      auto createAndGetEntityWithComponents() {
        return Base::createAndGetEntityWithComponents<Args...>(*getDefaultEntityGenerator());
      }
    };
    struct TestRegistry {
      TestRegistry() {
        //Set up default components needed for the system similar to how App.cpp does
        auto global = mRegistry.getSingleton();
        auto fs = std::make_unique<FileSystem::TestFileSystem>();
        mFileSystem = fs.get();
        mRegistry.addComponent<FileSystemComponent>(global, std::move(fs));
        auto pl = std::make_unique<ProjectLocator>();
        mProjectLocator = pl.get();
        mRegistry.addComponent<ProjectLocatorComponent>(global, std::move(pl));
      }

      TestEntityRegistry* operator->() {
        return &mRegistry;
      }

      TestEntityRegistry& operator*() {
        return mRegistry;
      }

      TestEntityRegistry mRegistry;
      ProjectLocator* mProjectLocator = nullptr;
      FileSystem::TestFileSystem* mFileSystem = nullptr;
    };

    TEST_METHOD(ProjectLocatorSystem_ProjectRootUri_ProjectLocatorUpdated) {
      TestRegistry reg;
      reg.mFileSystem->addDirectory("test/dir/t.txt");
      auto message = reg->createEntity();
      reg->addComponent<UriActivationComponent>(message, UriActivationComponent{ "projectRoot=test/dir/" });

      ProjectLocatorSystem::createUriListener()->tick(*reg);

      Assert::AreEqual(std::string("test/dir/p"), std::string(reg.mProjectLocator->transform("p", PathSpace::Project, PathSpace::Full).cstr()));
      Assert::AreEqual(size_t(1), reg->size<SetWorkingDirectoryComponent>());
    }

    TEST_METHOD(ProjectLocatorSystem_UnrelatedUri_NothingHappens) {
      TestRegistry reg;
      reg.mFileSystem->addDirectory("test/dir/t.txt");
      auto message = reg->createEntity();
      reg->addComponent<UriActivationComponent>(message, UriActivationComponent{ "bogus=test/dir/" });

      ProjectLocatorSystem::createUriListener()->tick(*reg);

      Assert::AreEqual(std::string("/p"), std::string(reg.mProjectLocator->transform("p", PathSpace::Project, PathSpace::Full).cstr()));
      Assert::AreEqual(size_t(0), reg->size<SetWorkingDirectoryComponent>());
    }

    TEST_METHOD(ProjectLocatorSystem_ProjectRootUriDoesntExist_NothingHappens) {
      TestRegistry reg;
      auto message = reg->createEntity();
      reg->addComponent<UriActivationComponent>(message, UriActivationComponent{ "projectRoot=test/dir/" });

      ProjectLocatorSystem::createUriListener()->tick(*reg);

      Assert::AreEqual(std::string("/p"), std::string(reg.mProjectLocator->transform("p", PathSpace::Project, PathSpace::Full).cstr()));
      Assert::AreEqual(size_t(0), reg->size<SetWorkingDirectoryComponent>());
    }

    TEST_METHOD(ProjectLocatorSystem_MultipleProjectRootUri_LastOneWins) {
      TestRegistry reg;
      reg.mFileSystem->addDirectory("test/dir/t.txt");
      reg.mFileSystem->addDirectory("test/dir2/t.txt");
      auto message = reg->createEntity();
      reg->addComponent<UriActivationComponent>(message, UriActivationComponent{ "projectRoot=test/dir/" });
      message = reg->createEntity();
      reg->addComponent<UriActivationComponent>(message, UriActivationComponent{ "projectRoot=test/dir2/" });

      ProjectLocatorSystem::createUriListener()->tick(*reg);

      Assert::AreEqual(std::string("test/dir2/p"), std::string(reg.mProjectLocator->transform("p", PathSpace::Project, PathSpace::Full).cstr()));
      Assert::AreEqual(size_t(2), reg->size<SetWorkingDirectoryComponent>());
    }

    TEST_METHOD(ProjectLocatorSystem_DefaultApp_MessageConsumed) {
      auto app = Registration::createDefaultApp();
      Engine::AppContext context(std::make_shared<Scheduler>(ecx::SchedulerConfig{}));
      app->registerAppContext(context);
      TestRegistry reg;
      auto message = reg->createEntity();
      reg->addComponent<UriActivationComponent>(message, UriActivationComponent{ "projectRoot=test/dir/" });
      reg->addComponent<MessageComponent>(message);
      context.addTickToAllPhases();

      context.update(*reg);

      Assert::AreEqual(size_t(0), reg->size<UriActivationComponent>());
    }
  };
}