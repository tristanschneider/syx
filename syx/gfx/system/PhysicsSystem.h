#pragma once
#include "System.h"

namespace Syx {
  class PhysicsSystem;
  typedef size_t Handle;
  struct Material;
  struct UpdateEvent;
};

class Model;
class EventListener;
class TransformEvent;
class PhysicsCompUpdateEvent;
class Gameobject;
class Event;
class App;

class PhysicsSystem : public System {
public:
  RegisterSystemH(PhysicsSystem);
  PhysicsSystem(App& app);
  ~PhysicsSystem();

  void init() override;
  void update(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) override;
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

  void _processSyxEvents();
  void _updateObject(Handle obj, const SyxData& data, const Syx::UpdateEvent& e);
  void _compUpdateEvent(const PhysicsCompUpdateEvent& e);
  void _transformEvent(const TransformEvent& e);
  Syx::Handle _createObject(Handle gameobject, bool hasRigidbody, bool hasCollider);

  std::unique_ptr<Syx::PhysicsSystem> mSystem;
  std::unique_ptr<EventListener> mTransformUpdates;

  std::unordered_map<Handle, SyxData> mToSyx;
  std::unordered_map<Syx::Handle, Handle> mFromSyx;
  Syx::Handle mDefaultSpace;
};