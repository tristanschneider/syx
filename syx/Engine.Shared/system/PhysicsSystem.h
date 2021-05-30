#pragma once
#include "System.h"

namespace Syx {
  struct IPhysicsObject;
  struct ISpace;
  struct IMaterialHandle;
  struct Material;
  class Model;
  class PhysicsSystem;
  typedef size_t Handle;
  struct UpdateEvent;
};

struct ApplyForceEvent;
class ClearSpaceEvent;
class Model;
class EventBuffer;
class TransformEvent;
class SetComponentPropEvent;
class SetComponentPropsEvent;
class SetTimescaleEvent;
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

  PhysicsSystem(const SystemArgs& args);
  ~PhysicsSystem();

  void init() override;
  void queueTasks(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) override;
  void uninit() override;

private:
  struct SyxData {
    //TODO: do lifetime management through IPhysicsObject os that handle usage can be removed
    Syx::Handle mHandle = 0;
    //Transform from syx to model space
    Syx::Mat4 mSyxToModel = Syx::Mat4::identity();
    std::shared_ptr<Syx::IPhysicsObject> mObj;
  };

  void _processSyxEvents();
  void _updateObject(Handle obj, const SyxData& data, const Syx::UpdateEvent& e);
  void _transformEvent(const TransformEvent& e);
  void _updateTransform(Handle handle, const Syx::Mat4& mat);
  void _setComponentPropsEvent(const SetComponentPropsEvent& e);
  void _clearSpaceEvent(const ClearSpaceEvent& e);
  void _removeComponentEvent(const RemoveComponentEvent& e);
  void _setTimescaleEvent(const SetTimescaleEvent& e);
  void _onApplyForce(const ApplyForceEvent& e);

  Syx::Handle _createObject(Handle gameobject, bool hasRigidbody, bool hasCollider);

  SyxData& _getSyxData(Handle obj, bool hasRigidbody, bool hasCollider);
  SyxData* _tryGetSyxData(Handle gameHandle);
  Syx::IPhysicsObject* _tryGetValidPhysicsObject(Handle gameHandle);

  float _getExpectedDeltaTime() const;

  std::unique_ptr<Syx::PhysicsSystem> mSystem;
  std::unique_ptr<EventBuffer> mTransformUpdates;

  std::unordered_map<Handle, SyxData> mToSyx;
  std::unordered_map<Syx::Handle, Handle> mFromSyx;
  std::shared_ptr<Syx::ISpace> mDefaultSpace;
  std::shared_ptr<Syx::IMaterialHandle> mDefaultMaterial;
  std::shared_ptr<Syx::Model> mDefaultModel;
  float mTimescale;
};