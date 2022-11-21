#include "Precompile.h"
#include "ecs/system/physics/PhysicsSystem.h"

#include "ecs/component/PhysicsComponents.h"
#include "ecs/component/TransformComponent.h"

namespace Impl {
  using namespace Tags;
  using namespace Engine;
  using FactoryCommands = CommandBuffer<
    CreatePhysicsObjectRequestComponent,
    ComputeMassRequestComponent,
    ComputeInertiaRequestComponent,
    PhysicsOwner,
    PhysicsObject,
    SyncTransformRequestComponent,
    SyncModelRequestComponent,
    SyncVelocityRequestComponent,
    FloatComponent<Position, X>,
    FloatComponent<Position, Y>,
    FloatComponent<Position, Z>,
    FloatComponent<Rotation, I>,
    FloatComponent<Rotation, J>,
    FloatComponent<Rotation, K>,
    FloatComponent<Rotation, W>,
    FloatComponent<LinearVelocity, X>,
    FloatComponent<LinearVelocity, Y>,
    FloatComponent<LinearVelocity, Z>,
    FloatComponent<AngularVelocity, X>,
    FloatComponent<AngularVelocity, Y>,
    FloatComponent<AngularVelocity, Z>,
    FloatComponent<LocalInertia, X>,
    FloatComponent<LocalInertia, Y>,
    FloatComponent<LocalInertia, Z>,
    FloatComponent<Inertia, A>,
    FloatComponent<Inertia, B>,
    FloatComponent<Inertia, C>,
    FloatComponent<Inertia, D>,
    FloatComponent<Inertia, E>,
    FloatComponent<Inertia, F>,
    FloatComponent<Mass, Value>
  >;

  using CreateView = View<Include<CreatePhysicsObjectRequestComponent>>;
  void _createPhysicsObjects(SystemContext<CreateView, FactoryCommands>& context) {
    auto cmd = context.get<FactoryCommands>();
    for(auto&& chunk : context.get<CreateView>().chunks()) {
      for(size_t i = 0; i < chunk.size(); ++i) {
        const Entity owner = chunk.indexToEntity(i);
        auto tuple = cmd.createAndGetEntityWithComponents<
          PhysicsObject,
          ComputeMassRequestComponent,
          ComputeInertiaRequestComponent,
          SyncTransformRequestComponent,
          SyncModelRequestComponent,
          SyncVelocityRequestComponent,
          FloatComponent<Position, X>,
          FloatComponent<Position, Y>,
          FloatComponent<Position, Z>,
          FloatComponent<Rotation, I>,
          FloatComponent<Rotation, J>,
          FloatComponent<Rotation, K>,
          FloatComponent<Rotation, W>,
          FloatComponent<LinearVelocity, X>,
          FloatComponent<LinearVelocity, Y>,
          FloatComponent<LinearVelocity, Z>,
          FloatComponent<AngularVelocity, X>,
          FloatComponent<AngularVelocity, Y>,
          FloatComponent<AngularVelocity, Z>,
          FloatComponent<LocalInertia, X>,
          FloatComponent<LocalInertia, Y>,
          FloatComponent<LocalInertia, Z>,
          FloatComponent<Inertia, A>,
          FloatComponent<Inertia, B>,
          FloatComponent<Inertia, C>,
          FloatComponent<Inertia, D>,
          FloatComponent<Inertia, E>,
          FloatComponent<Inertia, F>,
          FloatComponent<Mass, Value>
        >();

        //Link the new object to its owner
        const Entity physicsObject = std::get<0>(tuple);
        std::get<1>(tuple)->mPhysicsOwner = owner;

        //Link the owner to the new object
        cmd.addComponent<PhysicsOwner>(owner).mPhysicsObject = physicsObject;
        cmd.removeComponent<CreatePhysicsObjectRequestComponent>(owner);
      }
    }
  }

  using DestructorCmd = CommandBuffer<ecx::EntityDestroyTag, PhysicsOwner, DestroyPhysicsObjectRequestComponent>;
  using DestructOwnerView = View<Include<DestroyPhysicsObjectRequestComponent>, Read<PhysicsOwner>>;
  //Given a view of owner entities with destroy requests, destroy the linked physics entities and remove the owner and request components
  void _destroyPhysicsObjects(SystemContext<DestructorCmd, DestructOwnerView>& context) {
    auto cmd = context.get<DestructorCmd>();
    for(auto&& chunk : context.get<DestructOwnerView>().chunks()) {
      const auto* owners = chunk.tryGet<const PhysicsOwner>();
      for(size_t i = 0; i < owners->size(); ++i) {
        cmd.destroyEntity(owners->at(i).mPhysicsObject);
        cmd.removeComponent<PhysicsOwner>(chunk.indexToEntity(i));
        cmd.removeComponent<DestroyPhysicsObjectRequestComponent>(chunk.indexToEntity(i));
      }
    }
  }

  //Remove these components from all entities that have them
  template<class... ToRemove>
  void _removeAllComponentsSystem(SystemContext<CommandBuffer<ToRemove...>, View<Include<ToRemove>...>>& context) {
    auto cmd = context.get<CommandBuffer<ToRemove...>>();
    //TODO: remove from all in chunk support would help here
    for(auto&& chunk : context.get<View<Include<ToRemove>...>>().chunks()) {
      for(size_t i = 0; i < chunk.size(); ++i) {
        const auto entity = chunk.indexToEntity(i);
        (cmd.removeComponent<ToRemove>(entity), ...);
      }
    }
  }

  struct SOAVec3s {
    void set(size_t index, float inX, float inY, float inZ) {
      x[index] = inX;
      y[index] = inY;
      z[index] = inZ;
    }

    float* x = nullptr;
    float* y = nullptr;
    float* z = nullptr;
  };

  struct SOAQuats {
    void set(size_t index, float inI, float inJ, float inK, float inW) {
      i[index] = inI;
      j[index] = inJ;
      k[index] = inK;
      w[index] = inW;
    }

    float* i = nullptr;
    float* j = nullptr;
    float* k = nullptr;
    float* w = nullptr;
  };

  template<class TagT, class FromChunk>
  SOAVec3s _unwrapVec3(FromChunk& chunk) {
    return SOAVec3s {
      reinterpret_cast<float*>(chunk.tryGet<FloatComponent<TagT, X>>()->data()),
      reinterpret_cast<float*>(chunk.tryGet<FloatComponent<TagT, Y>>()->data()),
      reinterpret_cast<float*>(chunk.tryGet<FloatComponent<TagT, Z>>()->data())
    };
  }

  template<class TagT, class FromChunk>
  SOAQuats _unwrapQuat(FromChunk& chunk) {
    return {
      reinterpret_cast<float*>(chunk.tryGet<FloatComponent<TagT, I>>()->data()),
      reinterpret_cast<float*>(chunk.tryGet<FloatComponent<TagT, J>>()->data()),
      reinterpret_cast<float*>(chunk.tryGet<FloatComponent<TagT, K>>()->data()),
      reinterpret_cast<float*>(chunk.tryGet<FloatComponent<TagT, W>>()->data())
    };
  }

  using SyncTransformSource = View<Read<TransformComponent>>;
  using SyncTransformDest = View<Include<SyncTransformRequestComponent>,
    Read<PhysicsObject>,
    Write<FloatComponent<Position, X>>,
    Write<FloatComponent<Position, Y>>,
    Write<FloatComponent<Position, Z>>,
    Write<FloatComponent<Rotation, I>>,
    Write<FloatComponent<Rotation, J>>,
    Write<FloatComponent<Rotation, K>>,
    Write<FloatComponent<Rotation, W>>>;
  void _syncTransform(SystemContext<SyncTransformSource, SyncTransformDest>& context) {
    auto& source = context.get<SyncTransformSource>();
    for(auto&& chunk : context.get<SyncTransformDest>().chunks()) {
      const auto* destObjs = chunk.tryGet<const PhysicsObject>();
      SOAVec3s destPos = _unwrapVec3<Position>(chunk);
      SOAQuats destRot = _unwrapQuat<Rotation>(chunk);
      for(size_t i = 0; i < chunk.size(); ++i) {
        //Find physics entity's paired owner and get their transform
        if(auto ownerIt = source.find(destObjs->at(i).mPhysicsOwner); ownerIt != source.end()) {
          const TransformComponent& sourceTransform = (*ownerIt).get<const TransformComponent>();
          //TODO: use scale
          Syx::Vec3 translate, scale;
          Syx::Mat3 rotate;
          sourceTransform.mValue.decompose(scale, rotate, translate);
          Syx::Quat qRotate = rotate.toQuat();

          //Write owner values to physics entity
          destPos.set(i, translate.x, translate.y, translate.z);
          destRot.set(i, qRotate.mV.x, qRotate.mV.y, qRotate.mV.z, qRotate.mV.w);
        }
      }
    }
  }
}

std::vector<std::shared_ptr<Engine::System>> PhysicsSystems::createDefault() {
  std::vector<std::shared_ptr<Engine::System>> systems;
  systems.push_back(ecx::makeSystem("CreatePhysicsObjects", &Impl::_createPhysicsObjects));
  systems.push_back(ecx::makeSystem("DestroyPhysicsObjects", &Impl::_destroyPhysicsObjects));
  systems.push_back(ecx::makeSystem("SyncTransform", &Impl::_syncTransform));
  systems.push_back(ecx::makeSystem("ClearRequests", &Impl::_removeAllComponentsSystem<SyncTransformRequestComponent>));
  return systems;
}
