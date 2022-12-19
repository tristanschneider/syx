#include "Precompile.h"
#include "ecs/system/SandboxSystem.h"

#include "ecs/component/AssetComponent.h"
#include "ecs/component/PhysicsComponents.h"
#include "ecs/component/GameobjectComponent.h"
#include "ecs/component/GraphicsComponents.h"
#include "ecs/component/RawInputComponent.h"
#include "ecs/component/TransformComponent.h"

namespace {
  using namespace Engine;
  struct Sandbox : public Engine::System {
    static constexpr size_t REFRESH_KEY = 1;
    static constexpr size_t UP_KEY = 2;
    static constexpr size_t DOWN_KEY = 3;
    static constexpr size_t LEFT_KEY = 4;
    static constexpr size_t RIGHT_KEY = 5;
    static constexpr size_t CAMERA_DRAG_KEY = 6;
    static constexpr size_t CAMERA_MOVE_AMOUNT = 7;

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
      auto entityGen = Common::getEntityGenerator(args.context);
      if(!entityGen) {
        //Shouldn't happen outside of tests
        static SandboxGlobals none;
        return none;
      }

      auto&& [entity, globals, mappings] = args.registry.createAndGetEntityWithComponents<SandboxGlobals, InputMappingComponent>(*entityGen);
      mappings.get().mKeyMappings.push_back(KeyMapping{ Key::KeyR, KeyState::Triggered, REFRESH_KEY });
      mappings.get().mKeyMappings.push_back(KeyMapping{ Key::KeyW, KeyState::Down, UP_KEY });
      mappings.get().mKeyMappings.push_back(KeyMapping{ Key::KeyS, KeyState::Down, DOWN_KEY });
      mappings.get().mKeyMappings.push_back(KeyMapping{ Key::KeyA, KeyState::Down, LEFT_KEY });
      mappings.get().mKeyMappings.push_back(KeyMapping{ Key::KeyD, KeyState::Down, RIGHT_KEY });
      mappings.get().mKeyMappings.push_back(KeyMapping{ Key::RightMouse, KeyState::Down, CAMERA_DRAG_KEY });
      mappings.get().mMappings2D.push_back(Direction2DMapping{ &RawInputComponent::mMouseDelta, CAMERA_MOVE_AMOUNT });
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

      auto gen = Common::getEntityGenerator(args.context);
      if(!gen) {
        return false;
      }

      globals.assetsLoaded = true;
      for(const ModelInfo& info : toLoad) {
        //If it hasn't been requested yet, request it
        if(!*info.storage) {
          auto&& [entity, request] = args.registry.createAndGetEntityWithComponents<AssetLoadRequestComponent>(*gen);
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
          args.registry.destroyEntity(*info.storage, *gen);
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
      if(!gen) {
        return false;
      }
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
      if(!gen) {
        return;
      }
      //Destroy all the entities, they will be recreated next frame by tryInitScene
      for(Entity e : globals.objects) {
        args.registry.addComponent<DestroyGameobjectComponent>(e);
      }
      globals.objects.clear();
    }

    static void tickScene(SandboxGlobals& globals, Args& args) {
      if(InputMappingComponent* input = args.registry.tryGetComponent<InputMappingComponent>(globals.globalEntity)) {
        Syx::Vec2 move;
        Syx::Vec2 rotateInput;
        bool doRotate = false;
        for(const MappedKeyEvent& e : input->mKeyEvents) {
          switch(e.mActionIndex) {
            case REFRESH_KEY: resetObjects(globals, args); break;
            case UP_KEY: move.y = 1; break;
            case DOWN_KEY: move.y = -1; break;
            case LEFT_KEY: move.x = -1; break;
            case RIGHT_KEY: move.x = 1; break;
            case CAMERA_DRAG_KEY: doRotate = true; break;
            default: break;
          }
        }
        for(const Mapped2DEvent& e : input->mEvents2D) {
          switch(e.mAction) {
            case CAMERA_MOVE_AMOUNT: rotateInput = e.mValue; break;
            default: break;
          }
        }

        if(auto viewport = View<Read<ViewportComponent>, Include<DefaultViewportComponent>>(args.registry).tryGetFirst()) {
          View<Write<TransformComponent>, Read<CameraComponent>> cameraView(args.registry);
          //Move camera based on input
          if(auto cameraEntity = cameraView.find(viewport->get<const ViewportComponent>().mCamera); cameraEntity != cameraView.end()) {
            TransformComponent& cameraTransform = (*cameraEntity).get<TransformComponent>();
            Syx::Mat3 rotate;
            Syx::Vec3 translate;
            cameraTransform.mValue.decompose(rotate, translate);
            //Translation along camera's right and forward vectors
            const float speed = 0.1f;
            translate += rotate.getCol(0)*(move.x*speed);
            translate -= rotate.getCol(2)*(move.y*speed);

            if(doRotate) {
              const float rotateSpeed = 0.1f;
              rotateInput *= rotateSpeed;
              rotate = Syx::Mat3::axisAngle(Syx::Vec3(0, 1, 0), -rotateInput.x) * rotate;
              rotate = Syx::Mat3::axisAngle(rotate.getCol(0), -rotateInput.y) * rotate;
            }

            cameraTransform.mValue = Syx::Mat4::transform(rotate, translate);
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