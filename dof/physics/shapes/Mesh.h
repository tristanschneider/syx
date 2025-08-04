#pragma once

#include "RuntimeDatabase.h"
#include <StableElementID.h>
#include <glm/vec2.hpp>
#include <loader/AssetHandle.h>
#include <math/Geometric.h>

namespace ShapeRegistry {
  struct IShapeImpl;
}

class IAppBuilder;
class IAppModule;

namespace Shapes {
  //Point this at the loaded Loader::MeshAsset asset handle
  struct MeshReference {
    Loader::AssetHandle meshAsset;
  };
  //Same as MeshReference, but interprets the mesh as a composite mesh for each triangle.
  //Such meshes are not allowed to move (static), so are intended for terrain.
  struct StaticTriangleMeshReference : MeshReference {};
  //Add this to tables that want mesh collision. The table must also have Relation::HasChildrenRow as the composite uses children.
  struct MeshReferenceRow : Row<MeshReference> {};
  struct StaticTriangleMeshReferenceRow : Row<StaticTriangleMeshReference> {};

  //Populated by the physics module when it sees the mesh asset load
  struct MeshAsset {
    std::vector<glm::vec2> points;
    std::vector<glm::vec2> convexHull;
    Geo::AABB aabb;
  };
  struct MeshAssetRow : Row<MeshAsset> {};

  std::unique_ptr<IAppModule> createMeshModule();

  std::unique_ptr<ShapeRegistry::IShapeImpl> createMesh();
  std::unique_ptr<ShapeRegistry::IShapeImpl> createStaticTriangleMesh();
}