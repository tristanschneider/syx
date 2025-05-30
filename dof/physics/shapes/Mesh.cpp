#include <Precompile.h>
#include <shapes/Mesh.h>

#include <AppBuilder.h>
#include <loader/MeshAsset.h>
#include <Events.h>
#include <TLSTaskImpl.h>
#include <shapes/ShapeRegistry.h>

namespace Shapes {
  struct MeshStorage : ChainedRuntimeStorage {
    using ChainedRuntimeStorage::ChainedRuntimeStorage;

    std::vector<MeshAssetRow> meshAssets;
  };

  struct ImportMeshTask {
    void init(RuntimeDatabaseTaskBuilder& task) {
      query = task;
    }

    static MeshAsset createMesh(const Loader::MeshAsset& mesh) {
      MeshAsset result;
      result.points.reserve(mesh.verts.size());
      std::transform(mesh.verts.begin(), mesh.verts.end(), result.points.begin(), [](const Loader::MeshVertex& v) {
        return glm::vec2{ v.pos.x, v.pos.y };
      });
      //Not center of mass, just a reference point
      if(result.points.size()) {
        result.aabb.buildInit();
        for(const glm::vec2& p : result.points) {
          result.aabb.buildAdd(p);
        }
      }
      return result;
    }

    void execute() {
      for(size_t t = 0; t < query.size(); ++t) {
        auto [events, baseMeshes, physicsMeshes] = query.get(t);
        for(auto it : events) {
          if(it.second.isCreate()) {
            const size_t i = it.first;
            physicsMeshes->at(i) = createMesh(baseMeshes->at(i));
          }
        }
      }
    }

    QueryResult<
      const Events::EventsRow,
      const Loader::MeshAssetRow,
      MeshAssetRow
    > query;
  };

  void MeshModule::postProcessEvents(IAppBuilder& builder) {
    builder.submitTask(TLSTask::create<ImportMeshTask>("import mesh"));
  }

  //Add MeshAssetRow to all tables with Loader::MeshAssetRow
  void MeshModule::createDependentDatabase(RuntimeDatabaseArgs& args) {
    std::vector<RuntimeTableRowBuilder*> tables;
    for(auto&& t : args.tables) {
      if(t.contains<Loader::MeshAssetRow>()) {
        tables.push_back(&t);
      }
    }
    MeshStorage* storage = RuntimeStorage::addToChain<MeshStorage>(args);
    storage->meshAssets.resize(tables.size());
    for(size_t i = 0; i < tables.size(); ++i) {
      DBReflect::details::reflectRow(storage->meshAssets[i], *tables[i]);
    }
  }

  pt::FullTransformAlias getTransformAlias(const MeshTransform& t) {
    pt::FullTransformAlias result;
    result.posX = t.centerX;
    result.posY = t.centerY;
    result.rotX = t.rotX;
    result.rotY = t.rotY;
    result.scaleX = t.scaleX;
    result.scaleY = t.scaleY;
    return result;
  }

  class MeshClassifier : public ShapeRegistry::IShapeClassifier {
  public:
    MeshClassifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& res, const MeshTransform& t)
      : transformResolver{ task, getTransformAlias(t) }
      , tableResolver{ res }
      , ids{ task.getIDResolver()->getRefResolver() }
    {
      task.getResolver(meshRef, meshAsset);
    }

    ShapeRegistry::BodyType classifyShape(const UnpackedDatabaseElementID& id) final {
      const MeshReference* ref = tableResolver.tryGetOrSwapRowElement(meshRef, id);
      const Shapes::MeshAsset* asset = ref ? tableResolver.tryGetOrSwapRowElement(meshAsset, ids.tryUnpack(ref->meshAsset.asset)) : nullptr;
      const pt::FullTransform transform = transformResolver.resolve(id);
      if(asset) {
        return { ShapeRegistry::Mesh{
          .points = asset->points,
          .aabb = asset->aabb,
          .transform = transform,
        }};
      }
      return {};
    }

    pt::FullTransformResolver transformResolver;
    ITableResolver& tableResolver;
    CachedRow<const MeshReferenceRow> meshRef;
    CachedRow<const MeshAssetRow> meshAsset;
    ElementRefResolver ids;
  };

  class MeshImpl : public ShapeRegistry::IShapeImpl {
  public:
    MeshImpl(const MeshTransform& t)
      : transform{ t }
    {
    }

    std::vector<TableID> queryTables(IAppBuilder& builder) const final {
      return builder.queryTables<MeshReferenceRow>().getMatchingTableIDs();
    }

    std::shared_ptr<ShapeRegistry::IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& resolver) const final {
      return std::make_shared<MeshClassifier>(task, resolver, transform);
    }

    void writeBoundaries(IAppBuilder&, ShapeRegistry::BroadphaseBounds&) const final {
      //TODO:
    }

    const MeshTransform transform;
  };

  std::unique_ptr<ShapeRegistry::IShapeImpl> createMesh(const MeshTransform& transform) {
    return std::make_unique<MeshImpl>(transform);
  }
}