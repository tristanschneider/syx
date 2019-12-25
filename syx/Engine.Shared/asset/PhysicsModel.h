#pragma once
#include "asset/Asset.h"

class PhysicsSystem;

class PhysicsModel : public Asset {
public:
  friend class PhysicsSystem;
  using Asset::Asset;

  Handle getSyxHandle() const;

private:
  Handle mSyxHandle;
};