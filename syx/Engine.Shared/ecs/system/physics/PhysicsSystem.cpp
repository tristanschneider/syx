#include "Precompile.h"
#include "ecs/system/physics/PhysicsSystem.h"

#include "ecs/component/PhysicsComponents.h"
#include "ecs/component/TransformComponent.h"

#include "out_ispc/Inertia.h"

namespace Impl {
  using namespace Tags;
  using namespace Engine;
  using FactoryCommands = CommandBuffer<
    CreatePhysicsObjectRequestComponent,
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
    FloatComponent<Mass, Value>,
    FloatComponent<Density, Value>
  >;

  using CreateView = View<Include<CreatePhysicsObjectRequestComponent>>;
  void _createPhysicsObjects(SystemContext<CreateView, FactoryCommands>& context) {
    auto cmd = context.get<FactoryCommands>();
    for(auto&& chunk : context.get<CreateView>().chunks()) {
      for(size_t i = 0; i < chunk.size(); ++i) {
        const Entity owner = chunk.indexToEntity(i);
        auto tuple = cmd.createAndGetEntityWithComponents<
          PhysicsObject,
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
          FloatComponent<Mass, Value>,
          FloatComponent<Density, Value>
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
  template<class RemoveView, class ToRemove>
  void _removeFromAllInView(SystemContext<CommandBuffer<ToRemove>, RemoveView>& context) {
    auto&& [ cmd, view ] = context.get();
    //TODO: remove from all in chunk support would help here
    for(auto&& chunk : view.chunks()) {
      for(size_t i = 0; i < chunk.size(); ++i) {
        const auto entity = chunk.indexToEntity(i);
        cmd.removeComponent<ToRemove>(entity);
      }
    }
  }

  template<class ToRemove>
  void _removeAllComponentsSystem(SystemContext<CommandBuffer<ToRemove>, View<Include<ToRemove>>>& context) {
    _removeFromAllInView(context);
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

  template<class TagT, class TagV, class FromChunk>
  float* _unwrapFloat(FromChunk& chunk) {
    return reinterpret_cast<float*>(chunk.tryGet<FloatComponent<TagT, TagV>>()->data());
  }

  template<class TagT, class TagV, class FromChunk>
  const float* _unwrapConstFloat(FromChunk& chunk) {
    return reinterpret_cast<const float*>(chunk.tryGet<const FloatComponent<TagT, TagV>>()->data());
  }

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
          //Scale isn't used from the transform, it's only applied as appropriate from the PhysicsModelComponent
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

  using SyncModelSource = View<Read<PhysicsModelComponent>>;
  using SyncModelDest = View<Include<SyncModelRequestComponent>, Read<PhysicsObject>, Write<FloatComponent<Density, Value>>>;
  using SyncModelCmd = CommandBuffer<FloatComponent<SphereModel, Radius>>;
  void visitModel(const PhysicsModelComponent::Sphere& model, const Entity& physicsObject, SyncModelCmd& cmd) {
    cmd.addComponent<FloatComponent<SphereModel, Radius>>(physicsObject).mValue = model.mRadius;
  }

  //Add appropriate model to physics object assuming whatever it previously was has already been cleared
  void _syncModel(SystemContext<SyncModelSource, SyncModelDest, SyncModelCmd>& context) {
    auto& src = context.get<SyncModelSource>();
    auto cmd = context.get<SyncModelCmd>();
    for(auto&& chunk : context.get<SyncModelDest>().chunks()) {
      const auto* objects = chunk.tryGet<const PhysicsObject>();
      float* densities = _unwrapFloat<Density, Value>(chunk);
      for(size_t i = 0; i < chunk.size(); ++i) {
        if(auto it = src.find(objects->at(i).mPhysicsOwner); it != src.end()) {
          const PhysicsModelComponent& sourceModel = (*it).get<const PhysicsModelComponent>();
          std::visit([&](auto&& model) { visitModel(model, chunk.indexToEntity(i), cmd); }, sourceModel.mModel);
          densities[i] = sourceModel.mDensity;
        }
      }
    }
  }

  using SphereMassView = View<Include<SyncModelRequestComponent>,
    Read<FloatComponent<SphereModel, Radius>>,
    Write<FloatComponent<Mass, Value>>,
    Write<FloatComponent<LocalInertia, X>>,
    Write<FloatComponent<LocalInertia, Y>>,
    Write<FloatComponent<LocalInertia, Z>>
  >;
  void _computeSphereMass(SystemContext<SphereMassView>& context) {
    for(auto&& chunk : context.get<SphereMassView>().chunks()) {
      SOAVec3s inertia = _unwrapVec3<LocalInertia>(chunk);
      const float* radius = _unwrapConstFloat<SphereModel, Radius>(chunk);
      float* resultMass = _unwrapFloat<Mass, Value>(chunk);
      ispc::UniformVec3 uInertia{ inertia.x, inertia.y, inertia.z };
      ispc::computeSphereMass(radius, resultMass, uInertia, static_cast<uint32_t>(chunk.size()));
    }
  }

  //Invert the result of the mass calculation from the previous system. Done as a separate step since this doesn't depend on model type
  using InvertMassView = View<Include<SyncModelRequestComponent>,
    Read<FloatComponent<Density, Value>>,
    Write<FloatComponent<Mass, Value>>,
    Write<FloatComponent<LocalInertia, X>>,
    Write<FloatComponent<LocalInertia, Y>>,
    Write<FloatComponent<LocalInertia, Z>>
  >;
  void _invertMass(SystemContext<InvertMassView>& context) {
    for(auto&& chunk : context.get<InvertMassView>().chunks()) {
      SOAVec3s inertia = _unwrapVec3<LocalInertia>(chunk);
      const float* density = _unwrapConstFloat<Density, Value>(chunk);
      float* mass = _unwrapFloat<Mass, Value>(chunk);
      const uint32_t count = static_cast<uint32_t>(chunk.size());
      ispc::invertMass(mass, density, count);
      ispc::invertMass(inertia.x, density, count);
      ispc::invertMass(inertia.y, density, count);
      ispc::invertMass(inertia.z, density, count);
    }
  }
}

std::vector<std::shared_ptr<Engine::System>> PhysicsSystems::createDefault() {
  using namespace Engine;
  std::vector<std::shared_ptr<Engine::System>> systems;
  systems.push_back(ecx::makeSystem("CreatePhysicsObjects", &Impl::_createPhysicsObjects));
  systems.push_back(ecx::makeSystem("DestroyPhysicsObjects", &Impl::_destroyPhysicsObjects));
  systems.push_back(ecx::makeSystem("SyncTransform", &Impl::_syncTransform));
  systems.push_back(ecx::makeSystem("ClearOldModel", &Impl::_removeFromAllInView<
    View<Include<SyncModelRequestComponent>, Include<FloatComponent<Tags::SphereModel, Tags::Radius>>>,
    FloatComponent<Tags::SphereModel, Tags::Radius>
  >));
  systems.push_back(ecx::makeSystem("SyncModel", &Impl::_syncModel));
  systems.push_back(ecx::makeSystem("SphereMass", &Impl::_computeSphereMass));
  systems.push_back(ecx::makeSystem("InvertMass", &Impl::_invertMass));
  systems.push_back(ecx::makeSystem("ClearRequests", &Impl::_removeAllComponentsSystem<SyncTransformRequestComponent>));
  systems.push_back(ecx::makeSystem("ClearRequests", &Impl::_removeAllComponentsSystem<SyncModelRequestComponent>));
  systems.push_back(ecx::makeSystem("ClearRequests", &Impl::_removeAllComponentsSystem<SyncVelocityRequestComponent>));
  return systems;
}
