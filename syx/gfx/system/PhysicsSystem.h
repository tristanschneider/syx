#pragma once
#include "System.h"

namespace Syx {
  class PhysicsSystem;
  typedef size_t Handle;
  struct Material;
};

struct Model;

class PhysicsSystem : public System {
public:
  PhysicsSystem();
  ~PhysicsSystem();

  SystemId getId() const override {
    return SystemId::Physics;
  }

  void init() override;
  void update(float dt) override;
  void uninit() override;

  Handle addModel(const Model& model, bool environment);
  void removeModel(Handle handle);

  Handle addMaterial(const Syx::Material& mat);
  void removeMaterial(Handle handle);

private:
  std::unique_ptr<Syx::PhysicsSystem> mSystem;
  Syx::Handle mDefaultSpace;
};