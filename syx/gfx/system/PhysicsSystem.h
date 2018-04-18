#pragma once
#include "System.h"

namespace Syx {
  class PhysicsSystem;
  typedef size_t Handle;
  struct Material;
  struct UpdateEvent;
};

class Model;
class EventBuffer;
class TransformEvent;
class PhysicsCompUpdateEvent;
class SetComponentPropsEvent;
class Gameobject;
class Event;
class App;
struct PhysicsData;

class PhysicsSystem : public System {
public:
  RegisterSystemH(PhysicsSystem);
  PhysicsSystem(const SystemArgs& args);
  ~PhysicsSystem();

  void init() override;
  void queueTasks(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) override;
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
  void _setComponentPropsEvent(const SetComponentPropsEvent& e);
  Syx::Handle _createObject(Handle gameobject, bool hasRigidbody, bool hasCollider);

  void _updateFromData(Handle obj, const PhysicsData& data);

  std::unique_ptr<Syx::PhysicsSystem> mSystem;
  std::unique_ptr<EventBuffer> mTransformUpdates;

  std::unordered_map<Handle, SyxData> mToSyx;
  std::unordered_map<Syx::Handle, Handle> mFromSyx;
  Syx::Handle mDefaultSpace;
};