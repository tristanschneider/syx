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

  template<class T> using Include = ecx::Include<T>;
  template<class T> using Exclude = ecx::Exclude<T>;
  template<class T> using OptionalRead = ecx::OptionalRead<T>;
  template<class T> using OptionalWrite = ecx::OptionalWrite<T>;
  template<class T> using Read = ecx::Read<T>;
  template<class T> using Write = ecx::Write<T>;

  template<class... Args>
  using EntityModifier = ecx::EntityModifier<Entity, Args...>;
  using EntityFactory = ecx::EntityFactory<Entity>;
  using System = ecx::ISystem<Entity>;
  template<class... Args>
  using SystemContext = ecx::SystemContext<Entity, Args...>;
}