#pragma once
#include "asset/Asset.h"

namespace Syx {
  struct IMaterialHandle;
};

class PhysicsSystem;

class PhysicsMaterial : public Asset {
public:
  friend class PhysicsSystem;
  using Asset::Asset;

  const Syx::IMaterialHandle& getSyxHandle() const {
    return *mSyxHandle;
  }

private:
  std::shared_ptr<Syx::IMaterialHandle> mSyxHandle;
};

class PhysicsModel : public Asset {
public:
  friend class PhysicsSystem;
  using Asset::Asset;

  Handle getSyxHandle() const;

private:
  Handle mSyxHandle;
};