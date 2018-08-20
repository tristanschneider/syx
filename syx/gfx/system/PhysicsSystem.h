#pragma once
#include "System.h"

namespace Syx {
  class PhysicsSystem;
  typedef size_t Handle;
  struct Material;
  struct UpdateEvent;
};

class ClearSpaceEvent;
class Model;
class EventBuffer;
class TransformEvent;
class PhysicsCompUpdateEvent;
class SetComponentPropEvent;
class SetComponentPropsEvent;
class RemoveComponentEvent;
class Gameobject;
class Event;
class App;
struct PhysicsData;

class PhysicsSystem : public System {
public:
  static const std::string CUBE_MODEL_NAME;
  static const std::string SPHERE_MODEL_NAME;
  static const std::string CAPSULE_MODEL_NAME;
  static const std::string DEFAULT_MATERIAL_NAME;

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
  void _updateTransform(Handle handle, const Syx::Mat4& mat);
  void _setComponentPropsEvent(const SetComponentPropsEvent& e);
  void _clearSpaceEvent(const ClearSpaceEvent& e);
  void _removeComponentEvent(const RemoveComponentEvent& e);

  Syx::Handle _createObject(Handle gameobject, bool hasRigidbody, bool hasCollider);

  SyxData& _getSyxData(Handle obj, bool hasRigidbody, bool hasCollider);
  void _updateFromData(Handle obj, const PhysicsData& data);

  std::unique_ptr<Syx::PhysicsSystem> mSystem;
  std::unique_ptr<EventBuffer> mTransformUpdates;

  std::unordered_map<Handle, SyxData> mToSyx;
  std::unordered_map<Syx::Handle, Handle> mFromSyx;
  Syx::Handle mDefaultSpace;
};