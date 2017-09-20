#pragma once
#include "System.h"

namespace Syx {
  class PhysicsSystem;
  typedef size_t Handle;
  struct Material;
  struct UpdateEvent;
};

struct Model;
struct EventListener;
struct TransformListener;
struct TransformEvent;
class PhysicsCompUpdateEvent;
class Gameobject;
class Event;

class PhysicsSystem : public System {
public:
  PhysicsSystem();
  ~PhysicsSystem();

  SystemId getId() const override {
    return SystemId::Physics;
  }

  void init() override;
  void update(float dt, IWorkerPool& pool, std::shared_ptr<TaskGroup> frameTask) override;
  void uninit() override;

  Handle addModel(const Model& model, bool environment);
  void removeModel(Handle handle);

  Handle addMaterial(const Syx::Material& mat);
  void removeMaterial(Handle handle);

private:
  struct SyxData {
    Syx::Handle mHandle;
    //Transform from syx to model space
    Syx::Mat4 mSyxToModel;
  };

  void _processGameEvents();
  void _processSyxEvents();
  void _updateObject(Gameobject& obj, const SyxData& data, const Syx::UpdateEvent& e);
  void _compUpdateEvent(const PhysicsCompUpdateEvent& e);
  void _transformEvent(const TransformEvent& e);
  Syx::Handle _createObject(Handle gameobject, bool hasRigidbody, bool hasCollider);

  std::unique_ptr<Syx::PhysicsSystem> mSystem;
  std::unique_ptr<EventListener> mEventListener;
  std::unique_ptr<TransformListener> mTransformListener;
  std::unique_ptr<std::vector<TransformEvent>> mTransformUpdates;

  std::unordered_map<Handle, SyxData> mToSyx;
  std::unordered_map<Syx::Handle, Handle> mFromSyx;
  Syx::Handle mDefaultSpace;
};