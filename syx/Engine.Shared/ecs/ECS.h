#pragma once

#include "EntityFactory.h"
#include "LinearEntityRegistry.h"
#include "LinearView.h"
#include "System.h"

namespace Engine {
  using Entity = ecx::LinearEntity;
  using EntityRegistry = ecx::EntityRegistry<Entity>;
  template<class... Args>
  using View = ecx::View<Entity, Args...>;
  template<class... Args>
  using EntityModifier = ecx::EntityModifier<Entity, Args...>;
  using EntityFactory = ecx::EntityFactory<Entity>;
  using System = ecx::ISystem<Entity>;
}