#include "Precompile.h"
#include "ecs/system/SandboxSystem.h"

#include "ecs/component/AssetComponent.h"
#include "ecs/component/PhysicsComponents.h"
#include "ecs/component/GraphicsComponents.h"
#include "ecs/component/RawInputComponent.h"
#include "ecs/component/TransformComponent.h"

namespace {
  using namespace Engine;
  struct Sandbox : public Engine::System {
    static constexpr size_t REFRESH_KEY = 1;

    using Common = ecx::SystemCommon<Engine::Entity>;
    struct SandboxGlobals {
      Engine::Entity globalEntity;
      Engine::Entity sphereModel;
      Engine::Entity defaultTexture;
      std::vector<Engine::Entity> objects;
      bool assetsLoaded = false;
    };

    struct Args {
      EntityRegistry& registry;
      ecx::ThreadLocalContext& context;
    };

    static SandboxGlobals& tryInitGlobals(Args& args) {
      if(auto globals = Common::getOrCreateView<Write<SandboxGlobals>>(args.registry, args.context).tryGetFirst()) {
        return globals->get<SandboxGlobals>();
      }

      auto&& [entity, globals, mappings] = args.registry.createAndGetEntityWithComponents<SandboxGlobals, InputMappingComponent>(*Common::getEntityGenerator(args.context));
      mappings.get().mKeyMappings.push_back(KeyMapping{ Key::KeyR, KeyState::Triggered, REFRESH_KEY });
      globals.get().globalEntity = entity;
      return globals.get();
    }

    static bool tryLoadAssets(SandboxGlobals& globals, Args& args) {
      if(globals.assetsLoaded) {
        return true;
      }
      struct ModelInfo {
        const char* filename;
        Engine::Entity* storage;
      };
      auto toLoad = {
        ModelInfo{ "models/sphere.obj", &globals.sphereModel },
        ModelInfo{ "textures/test.bmp", &globals.defaultTexture }
      };

      globals.assetsLoaded = true;
      for(const ModelInfo& info : toLoad) {
        //If it hasn't been requested yet, request it
        if(!*info.storage) {
          auto&& [entity, request] = args.registry.createAndGetEntityWithComponents<AssetLoadRequestComponent>(*Common::getEntityGenerator(args.context));
          *info.storage = entity;
          request.get().mPath = info.filename;
        }
        //If it succeeded, continue
        else if(args.registry.tryGetComponent<AssetComponent>(*info.storage)) {
          continue;
        }
        //If it failed, requeue
        else if(args.registry.tryGetComponent<AssetLoadFailedComponent>(*info.storage)) {
          printf("Failed to load sphere\n");
          args.registry.destroyEntity(*info.storage, *Common::getEntityGenerator(args.context));
          *info.storage = {};
        }
        //Only the continue case above should keep the success state
        globals.assetsLoaded = false;
      }
      return globals.assetsLoaded;
    }

    static bool tryInitScene(SandboxGlobals& globals, Args& args) {
      if(!globals.objects.empty()) {
        return true;
      }

      auto gen = Common::getEntityGenerator(args.context);
      for(size_t i = 0; i < 10; ++ i) {
        auto&& [entity,
          transform,
          graphicsModel,
          texture,
          req,
          physicsModel] = args.registry.createAndGetEntityWithComponents<
          TransformComponent,
          GraphicsModelRefComponent,
          TextureRefComponent,
          CreatePhysicsObjectRequestComponent,
          PhysicsModelComponent
        >(*gen);
        globals.objects.push_back(entity);
        transform.get().mValue.setTranslate(Syx::Vec3(float(i), float(i), -5.0f));
        graphicsModel.get().mModel = globals.sphereModel;
        texture.get().mTexture = globals.defaultTexture;
        physicsModel.get().mModel = PhysicsModelComponent::Sphere{ 1.0f };

        if(i == 0) {
          transform.get().mValue.setTranslate(Syx::Vec3(0.0f, -10.0f, 0.0f));
          const float radius = 2.0f;
          transform.get().mValue.setScale(Syx::Vec3(radius));
          physicsModel.get().mModel = PhysicsModelComponent::Sphere{ radius };
          physicsModel.get().mDensity = 0.0f;
        }
      }

      return true;
    }

    static void resetObjects(SandboxGlobals& globals, Args& args) {
      auto gen = Common::getEntityGenerator(args.context);
      //Destroy all the entities, they will be recreated next frame by tryInitScene
      for(Entity e : globals.objects) {
        //TODO: the physics objects will get stuck this way
        args.registry.destroyEntity(e, *gen);
      }
      globals.objects.clear();
    }

    static void tickScene(SandboxGlobals& globals, Args& args) {
      if(InputMappingComponent* input = args.registry.tryGetComponent<InputMappingComponent>(globals.globalEntity)) {
        for(const MappedKeyEvent& e : input->mKeyEvents) {
          switch(e.mActionIndex) {
          case REFRESH_KEY: resetObjects(globals, args); break;
          default: break;
          }
        }

        input->clearEvents();
      }
    }

    void tick(EntityRegistry& registry, ecx::ThreadLocalContext& localContext) const override {
      Args args{ registry, localContext };
      SandboxGlobals& globals = tryInitGlobals(args);
      if(tryLoadAssets(globals, args)) {
        if(tryInitScene(globals, args)) {
          tickScene(globals, args);
        }
      }
    }

    ecx::SystemInfo<Engine::Entity> getInfo() const override {
      ecx::SystemInfo<Engine::Entity> result;
      result.mIsBlocking = true;
      result.mName = "Sandbox";
      return result;
    }
  };
}

std::shared_ptr<Engine::System> SandboxSystem::create() {
  return std::make_shared<Sandbox>();
}