#include "Precompile.h"
#include "ecs/system/physics/PhysicsSystem.h"

#include "ecs/component/PhysicsComponents.h"
#include "ecs/component/TransformComponent.h"

#include "out_ispc/unity.h"

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

  template<class AddView, class ToAdd>
  void _addToAllInView(SystemContext<CommandBuffer<ToAdd>, AddView>& context) {
    auto&& [ cmd, view ] = context.get();
    //TODO: add to all in chunk support would help here
    for(auto&& chunk : view.chunks()) {
      for(size_t i = 0; i < chunk.size(); ++i) {
        const auto entity = chunk.indexToEntity(i);
        cmd.addComponent<ToAdd>(entity);
      }
    }
  }

  struct SOAConstVec3s {
    const float* x = nullptr;
    const float* y = nullptr;
    const float* z = nullptr;
  };

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

  struct SOAConstQuats {
    const float* i = nullptr;
    const float* j = nullptr;
    const float* k = nullptr;
    const float* w = nullptr;
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

  // a b c
  //   d e
  //     f
  struct SOASymmetricMatrix {
    float* a = nullptr;
    float* b = nullptr;
    float* c = nullptr;
    float* d = nullptr;
    float* e = nullptr;
    float* f = nullptr;
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
  SOASymmetricMatrix _unwrapSymmetricMatrix(FromChunk& chunk) {
    return SOASymmetricMatrix {
      reinterpret_cast<float*>(chunk.tryGet<FloatComponent<TagT, A>>()->data()),
      reinterpret_cast<float*>(chunk.tryGet<FloatComponent<TagT, B>>()->data()),
      reinterpret_cast<float*>(chunk.tryGet<FloatComponent<TagT, C>>()->data()),
      reinterpret_cast<float*>(chunk.tryGet<FloatComponent<TagT, D>>()->data()),
      reinterpret_cast<float*>(chunk.tryGet<FloatComponent<TagT, E>>()->data()),
      reinterpret_cast<float*>(chunk.tryGet<FloatComponent<TagT, F>>()->data())
    };
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
  SOAConstVec3s _unwrapConstVec3(FromChunk& chunk) {
    return SOAConstVec3s {
      reinterpret_cast<const float*>(chunk.tryGet<const FloatComponent<TagT, X>>()->data()),
      reinterpret_cast<const float*>(chunk.tryGet<const FloatComponent<TagT, Y>>()->data()),
      reinterpret_cast<const float*>(chunk.tryGet<const FloatComponent<TagT, Z>>()->data())
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

  template<class TagT, class FromChunk>
  SOAConstQuats _unwrapConstQuat(FromChunk& chunk) {
    return {
      reinterpret_cast<const float*>(chunk.tryGet<const FloatComponent<TagT, I>>()->data()),
      reinterpret_cast<const float*>(chunk.tryGet<const FloatComponent<TagT, J>>()->data()),
      reinterpret_cast<const float*>(chunk.tryGet<const FloatComponent<TagT, K>>()->data()),
      reinterpret_cast<const float*>(chunk.tryGet<const FloatComponent<TagT, W>>()->data())
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
  //Write transform to physics object
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

  using PublishTransformDest = View<Write<TransformComponent>>;
  //Syncs all physics objects. Could optimize here for ones that didn't move/rotate
  using PublishTransformSource = View<
    Read<PhysicsObject>,
    Read<FloatComponent<Position, X>>,
    Read<FloatComponent<Position, Y>>,
    Read<FloatComponent<Position, Z>>,
    Read<FloatComponent<Rotation, I>>,
    Read<FloatComponent<Rotation, J>>,
    Read<FloatComponent<Rotation, K>>,
    Read<FloatComponent<Rotation, W>>
  >;
  //Write physics object to transform
  void _publishTransform(SystemContext<PublishTransformSource, PublishTransformDest>& context) {
    auto& dstView = context.get<PublishTransformDest>();
    for(auto&& chunk : context.get<PublishTransformSource>().chunks()) {
      const SOAConstQuats rot = _unwrapConstQuat<Rotation>(chunk);
      const SOAConstVec3s pos = _unwrapConstVec3<Position>(chunk);
      const PhysicsObject* objects = chunk.tryGet<const PhysicsObject>()->data();
      for(size_t i = 0; i < chunk.size(); ++i) {
        //TODO: could build multiple transforms at the same time with ispc, output is awkward since it wouldn't be SOA
        if(auto ownerIt = dstView.find(objects[i].mPhysicsOwner); ownerIt != dstView.end()) {
          Syx::Mat4& transform = (*ownerIt).get<TransformComponent>().mValue;
          //Pull out scale
          //TODO: this is a bummer, maybe should copy it out so it can be copied back more easily. Or not bake scale into the matrix
          Syx::Vec3 unused, scale;
          Syx::Mat3 unusedMat;
          transform.decompose(scale, unusedMat, unused);

          transform = Syx::Mat4::transform(scale, Syx::Quat(rot.i[i], rot.j[i], rot.k[i], rot.w[i]), Syx::Vec3(pos.x[i], pos.y[i], pos.z[i]));
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

  using ComputeInertiaView = View<
    Include<ComputeInertiaRequestComponent>,
    Read<FloatComponent<Rotation, I>>,
    Read<FloatComponent<Rotation, J>>,
    Read<FloatComponent<Rotation, K>>,
    Read<FloatComponent<Rotation, W>>,
    Read<FloatComponent<LocalInertia, X>>,
    Read<FloatComponent<LocalInertia, Y>>,
    Read<FloatComponent<LocalInertia, Z>>,
    Write<FloatComponent<Inertia, A>>,
    Write<FloatComponent<Inertia, B>>,
    Write<FloatComponent<Inertia, C>>,
    Write<FloatComponent<Inertia, D>>,
    Write<FloatComponent<Inertia, E>>,
    Write<FloatComponent<Inertia, F>>
  >;
  void _recomputeInertia(SystemContext<ComputeInertiaView>& context) {
    for(auto&& chunk : context.get<ComputeInertiaView>().chunks()) {
      SOASymmetricMatrix result = _unwrapSymmetricMatrix<Inertia>(chunk);
      SOAConstVec3s localInertia = _unwrapConstVec3<LocalInertia>(chunk);
      SOAConstQuats rot = _unwrapConstQuat<Rotation>(chunk);
      const ispc::UniformConstVec3 uInertia{ localInertia.x, localInertia.y, localInertia.z };
      const ispc::UniformConstQuat uRot{ rot.i, rot.j, rot.k, rot.w };
      ispc::UniformSymmetricMatrix uResult{ result.a, result.b, result.c, result.d, result.e, result.f };
      ispc::recomputeInertiaTensor(uRot, uInertia, uResult, static_cast<uint32_t>(chunk.size()));
    }
  }

  template<class Axis>
  using AccelerationView = View<Read<FloatComponent<GlobalAcceleration, Axis>>>;
  using DTView = View<Read<FloatComponent<GlobalDeltaTime, Value>>>;
  template<class Axis>
  using IntegrateAccelerationView = View<Write<FloatComponent<LinearVelocity, Axis>>>;
  template<class Axis>
  void _integrateGlobalVelocity(SystemContext<AccelerationView<Axis>, DTView, IntegrateAccelerationView<Axis>>& context) {
    for(auto&& dtEntity : context.get<DTView>()) {
      const float dt = dtEntity.get<const FloatComponent<GlobalDeltaTime, Value>>().mValue;
      for(auto&& accelEntity : context.get<AccelerationView<Axis>>()) {
        const float acceleration = accelEntity.get<const FloatComponent<GlobalAcceleration, Axis>>().mValue;
        for(auto&& chunk : context.get<IntegrateAccelerationView<Axis>>().chunks()) {
          float* velocity = _unwrapFloat<LinearVelocity, Axis>(chunk);
          ispc::integrateLinearVelocityGlobalAcceleration(velocity, acceleration, dt, static_cast<uint32_t>(chunk.size()));
        }
      }
    }
  }

  template<class Axis>
  using IntegratePositionView = View<Read<FloatComponent<LinearVelocity, Axis>>, Write<FloatComponent<Position, Axis>>>;
  template<class Axis>
  void _integratePosition(SystemContext<DTView, IntegratePositionView<Axis>>& context) {
    for(auto&& dtEntity : context.get<DTView>()) {
      const float dt = dtEntity.get<const FloatComponent<GlobalDeltaTime, Value>>().mValue;
      for(auto&& chunk : context.get<IntegratePositionView<Axis>>().chunks()) {
        float* position = _unwrapFloat<Position, Axis>(chunk);
        const float* velocity = _unwrapConstFloat<LinearVelocity, Axis>(chunk);
        ispc::integrateLinearPosition(position, velocity, dt, static_cast<uint32_t>(chunk.size()));
      }
    }
  }

  using IntegrateRotationView = View<
    Write<FloatComponent<Rotation, I>>,
    Write<FloatComponent<Rotation, J>>,
    Write<FloatComponent<Rotation, K>>,
    Write<FloatComponent<Rotation, W>>,
    Read<FloatComponent<AngularVelocity, X>>,
    Read<FloatComponent<AngularVelocity, Y>>,
    Read<FloatComponent<AngularVelocity, Z>>
  >;
  void _integrateRotation(SystemContext<DTView, IntegrateRotationView>& context) {
    for(auto&& dtEntity : context.get<DTView>()) {
      const float dt = dtEntity.get<const FloatComponent<GlobalDeltaTime, Value>>().mValue;
      for(auto&& chunk : context.get<IntegrateRotationView>().chunks()) {
        const SOAConstVec3s angVel = _unwrapConstVec3<AngularVelocity>(chunk);
        SOAQuats rot = _unwrapQuat<Rotation>(chunk);
        ispc::UniformQuat uRot{ rot.i, rot.j, rot.k, rot.w };
        ispc::UniformConstVec3 uAngVel{ angVel.x, angVel.y, angVel.z };
        ispc::integrateRotation(uRot, uAngVel, dt, static_cast<uint32_t>(chunk.size()));
      }
    }
  }

  using InitCmd = CommandBuffer<
    FloatComponent<GlobalAcceleration, Y>,
    FloatComponent<GlobalDeltaTime, Value>
  >;
  void _init(SystemContext<InitCmd>& context) {
    auto cmd = context.get<InitCmd>();
    std::get<1>(cmd.createAndGetEntityWithComponents<FloatComponent<GlobalAcceleration, Y>>())->mValue = -9.8f;
    //Default to fixed update, can update per-frame if variable rate is desired
    std::get<1>(cmd.createAndGetEntityWithComponents<FloatComponent<GlobalDeltaTime, Value>>())->mValue = 1.0f/60.0f;
  }
}

std::shared_ptr<Engine::System> PhysicsSystems::createInit() {
  return ecx::makeSystem("PhysicsInit", &Impl::_init);
}

std::vector<std::shared_ptr<Engine::System>> PhysicsSystems::createDefault() {
  using namespace Engine;
  std::vector<std::shared_ptr<Engine::System>> systems;

  //Default only gravity on y axis
  systems.push_back(ecx::makeSystem("IntegrateVelocity", &Impl::_integrateGlobalVelocity<Tags::Y>));

  systems.push_back(ecx::makeSystem("IntegratePositionX", &Impl::_integratePosition<Tags::X>));
  systems.push_back(ecx::makeSystem("IntegratePositionY", &Impl::_integratePosition<Tags::Y>));
  systems.push_back(ecx::makeSystem("IntegratePositionZ", &Impl::_integratePosition<Tags::Z>));
  systems.push_back(ecx::makeSystem("IntegrateRotation", &Impl::_integrateRotation));

  //For now, add to all every frame. Can ultimately account for sleeping entities and shapes that don't require recomputing inertia
  systems.push_back(ecx::makeSystem("FlagForInertiaUpdate", &Impl::_addToAllInView<View<Include<PhysicsObject>>, ComputeInertiaRequestComponent>));

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

  //Both for initialization and the common case when objects move
  systems.push_back(ecx::makeSystem("ComputeInertia", &Impl::_recomputeInertia));

  systems.push_back(ecx::makeSystem("ClearRequests", &Impl::_removeAllComponentsSystem<SyncTransformRequestComponent>));
  systems.push_back(ecx::makeSystem("ClearRequests", &Impl::_removeAllComponentsSystem<SyncModelRequestComponent>));
  systems.push_back(ecx::makeSystem("ClearRequests", &Impl::_removeAllComponentsSystem<SyncVelocityRequestComponent>));
  systems.push_back(ecx::makeSystem("ClearRequests", &Impl::_removeAllComponentsSystem<ComputeInertiaRequestComponent>));

  //Publish transform as late as possible
  systems.push_back(ecx::makeSystem("PublishTransform", &Impl::_publishTransform));

  return systems;
}
