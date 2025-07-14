#pragma once

#include <Mass.h>
#include <SparseRow.h>
#include <Table.h>

class IAppModule;
struct PhysicsAliases;

//Computes the value in MassRow as needed based on the ShapeRegistry shape.
//Mass is updated whenever an element is added and when PhysicsEvents::RecomputeMassRow is flagged
namespace MassModule {
  struct MassRow : Row<Mass::OriginMass> {};
  struct IsImmobile : TagRow{};

  std::unique_ptr<IAppModule> createModule(const PhysicsAliases& aliases);
}