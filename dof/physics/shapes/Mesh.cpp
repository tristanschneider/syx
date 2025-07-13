#include <Precompile.h>
#include <shapes/Mesh.h>

#include <AppBuilder.h>
#include <ConvexHull.h>
#include <loader/MeshAsset.h>
#include <Events.h>
#include <TLSTaskImpl.h>
#include <shapes/ShapeRegistry.h>
#include <IAppModule.h>
#include <generics/Functional.h>
#include <loader/ReflectionModule.h>
#include <loader/SceneAsset.h>
#include <RelationModule.h>

namespace Shapes {
  //This is equivalent to Relation::HasChildrenRow but allows children of mesh asset entries to exist unrelated to composite meshes.
  struct CompositeMesh {
    std::vector<ElementRef> parts;
  };
  struct CompositeMeshRow : Row<CompositeMesh> {};
  struct CompositeMeshTagRow : TagRow {};

  struct MeshStorage : ChainedRuntimeStorage {
    using ChainedRuntimeStorage::ChainedRuntimeStorage;
    struct Rows {
      MeshAssetRow mesh;
      //Reference to elements in th ecompositeAssets table for composite meshes
      CompositeMeshRow composite;
    };

    std::vector<Rows> rows;
    Table<
      CompositeMeshTagRow,
      StableIDRow,
      Relation::HasParentRow,
      MeshAssetRow
    > compositeAssets;
  };

  struct ImportMeshTask {
    void init(RuntimeDatabaseTaskBuilder& task) {
      query = task;
      compositeMeshTable = task.queryTables<CompositeMeshTagRow>().getTableID(0);
    }

    void init(AppTaskArgs& args) {
      relations = args;
    }

    static Geo::AABB computeAABB(const MeshAsset& mesh) {
      Geo::AABB result;
      if(mesh.convexHull.size()) {
        result.buildInit();
        for(const glm::vec2& p : mesh.convexHull) {
          result.buildAdd(p);
        }
      }
      return result;
    }

    static MeshAsset createMesh(const Loader::MeshAsset& mesh) {
      MeshAsset result;
      result.points.resize(mesh.verts.size());
      //Copy triangle primitives as-is into verts
      std::transform(mesh.verts.begin(), mesh.verts.end(), result.points.begin(), [](const Loader::MeshVertex& v) {
        return glm::vec2{ v.pos.x, v.pos.y };
      });
      //Compute convex hull and store in convexHull
      ConvexHull::Context ctx;
      ConvexHull::compute(result.points, ctx);
      result.convexHull.resize(ctx.result.size());
      std::transform(ctx.result.begin(), ctx.result.end(), result.convexHull.begin(), ConvexHull::GetPoints{ result.points });
      //Mass properties will be computed based on convex hull by MassModule

      result.aabb = computeAABB(result);
      return result;
    }

    static void createCompositeMesh(
      TableID childTable,
      const ElementRef& parentMesh,
      const MeshAsset& mesh,
      CompositeMesh& parentComposite,
      Relation::ChildrenEntry& meshChildren,
      Relation::RelationWriter& writer) {
      std::vector<size_t> triIndices;
      triIndices.reserve(mesh.points.size() / 3);

      //Find all "valid" triangles as ones that have some amount of area with counter-clockwise winding.
      //Points are assumed to be in triplets
      for(size_t i = 0; i + 2 < mesh.points.size(); i += 3) {
        const glm::vec2& a = mesh.points[i];
        const glm::vec2& b = mesh.points[i + 1];
        const glm::vec2& c = mesh.points[i + 2];
        const float ccwArea = Geo::cross(b - a, c - a);
        if(ccwArea > 0.01f) {
          triIndices.push_back(i);
        }
      }

      if(triIndices.empty()) {
        return;
      }

      //Add child elements
      Relation::RelationWriter::NewChildren result = writer.addChildren(parentMesh, meshChildren, childTable, triIndices.size());
      //Fill in child meshes
      MeshAssetRow* childMeshes = result.table->tryGet<MeshAssetRow>();
      parentComposite.parts.resize(triIndices.size());
      for(size_t i = 0; i < triIndices.size(); ++i) {
        MeshAsset& childMesh = childMeshes->at(i + result.startIndex);
        const size_t ti = triIndices[i];
        childMesh.points.insert(childMesh.points.end(), {
          mesh.points[ti],
          mesh.points[ti + 1],
          mesh.points[ti + 2]
        });
        //Triangle is already its own convex hull
        childMesh.convexHull = childMesh.points;
        childMesh.aabb = computeAABB(childMesh);
        //Store references to the chldren on the parent
        parentComposite.parts[i] = result.childRefs[i];
      }
    }

    void execute() {
      for(size_t t = 0; t < query.size(); ++t) {
        auto [stables, events, baseMeshes, children, compositeMeshes, physicsMeshes] = query.get(t);
        for(auto it : events) {
          if(it.second.isCreate()) {
            const size_t i = it.first;
            physicsMeshes->at(i) = createMesh(baseMeshes->at(i));
            //Always create composite mesh data for now. Could be done on-demand in the future if needed.
            createCompositeMesh(
              compositeMeshTable,
              stables->at(i),
              physicsMeshes->at(i),
              compositeMeshes->at(i),
              children->at(i),
              relations
            );
          }
        }
      }
    }

    QueryResult<
      const StableIDRow,
      const Events::EventsRow,
      const Loader::MeshAssetRow,
      Relation::HasChildrenRow,
      CompositeMeshRow,
      MeshAssetRow
    > query;
    Relation::RelationWriter relations;
    TableID compositeMeshTable;
  };

  class MeshModule : public IAppModule {
  public:
    void postProcessEvents(IAppBuilder& builder) override {
      builder.submitTask(TLSTask::create<ImportMeshTask>("import mesh"));
    }

    //Add MeshAssetRow to all tables with Loader::MeshAssetRow
    void createDependentDatabase(RuntimeDatabaseArgs& args) override {
      std::vector<RuntimeTableRowBuilder*> tables;
      for(auto&& t : args.tables) {
        if(t.contains<Loader::MeshAssetRow>()) {
          tables.push_back(&t);
        }
      }
      MeshStorage* storage = RuntimeStorage::addToChain<MeshStorage>(args);
      storage->rows.resize(tables.size());
      for(size_t i = 0; i < tables.size(); ++i) {
        DBReflect::details::reflectRow(storage->rows[i].mesh, *tables[i]);
        DBReflect::details::reflectRow(storage->rows[i].composite, *tables[i]);
      }
      //Add table for all the child meshes of a composite mesh
      args.tables.emplace_back();
      DBReflect::details::reflectTable(args.tables.back(), storage->compositeAssets);
    }

    //For any objects that load with MatMeshRefRow, copy that mesh reference into MeshReferenceRow if it exists to use for collision.
    struct MatMeshLoader {
      using src_row = Loader::MatMeshRefRow;
      static constexpr std::string_view NAME = src_row::KEY;

      static void load(const IRow& src, RuntimeTable& dst, gnx::IndexRange range) {
        using namespace gnx::func;
        using namespace Reflection;

        const Loader::MatMeshRefRow& s = static_cast<const Loader::MatMeshRefRow&>(src);
        tryLoadRow<MeshReferenceRow>(s, dst, range, GetMember<&Loader::MatMeshRef::mesh>{});
      }
    };

    void init(IAppBuilder& builder) override {
      Reflection::registerLoaders(builder, Reflection::createRowLoader(MatMeshLoader{}));
    }
  };

  std::unique_ptr<IAppModule> createMeshModule() {
    return std::make_unique<MeshModule>();
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
      //TODO: avoid recomputing by storing in table with the objects and making transformResolver deal with this
      auto [mToW, wToM] = transform.toPackedWithInverse();
      if(asset) {
        return { ShapeRegistry::Mesh{
          .points = asset->convexHull,
          .aabb = asset->aabb,
          .modelToWorld = mToW,
          .worldToModel = wToM
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

  struct UpdateBoundaries {
    struct Args {
      const MeshTransform& transform;
      ShapeRegistry::BroadphaseBounds& bounds;
    };
    struct Group {
      void init(const Args& args) {
        alias = args.transform;
        bounds = &args.bounds;
      }

      void init(RuntimeDatabaseTaskBuilder& task) {
        task.logDependency({ bounds->requiredDependency });
        query = task.queryAlias(bounds->table,
          QueryAlias<MeshReferenceRow>::create().read(),
          alias.centerX.read(),
          alias.centerY.read(),
          alias.rotX.read(),
          alias.rotY.read(),
          alias.scaleX.read(),
          alias.scaleY.read()
        );
        resolver = task.getResolver<const Shapes::MeshAssetRow>();
        ids = task.getRefResolver();
      }

      MeshTransform alias;
      ShapeRegistry::BroadphaseBounds* bounds{};
      QueryResult<
        const MeshReferenceRow,
        const Row<float>,
        const Row<float>,
        const Row<float>,
        const Row<float>,
        const Row<float>,
        const Row<float>
      > query;
      std::shared_ptr<ITableResolver> resolver;
      ElementRefResolver ids;
    };

    void init() {}

    void execute(Group& g) {
      CachedRow<const MeshAssetRow> assets;
      assert(g.query.size() <= 1);
      for(size_t t = 0; t < g.query.size(); ++t) {
        auto&& [shape, px, py, rx, ry, sx, sy] = g.query.get(t);
        g.bounds->minX.resize(px->size());
        g.bounds->minY.resize(px->size());
        g.bounds->maxX.resize(px->size());
        g.bounds->maxY.resize(px->size());

        for(size_t i = 0; i < px->size(); ++i) {
          //Resolve mesh
          if(const MeshAsset* mesh = g.resolver->tryGetOrSwapRowElement(assets, g.ids.unpack(shape->at(i).meshAsset.asset))) {
            //Resolve transform
            pt::Parts parts{
              .rot = glm::vec2{ rx->at(i), ry->at(i) },
              .scale = glm::vec2{ sx->at(i), sy->at(i) },
              .translate = glm::vec3{ px->at(i), py->at(i), 0.f }
            };
            const pt::PackedTransform transform{ pt::PackedTransform::build(parts) };
            //Transform aabb to world
            auto points = mesh->aabb.points();
            Geo::AABB worldBB;
            worldBB.buildInit();
            for(glm::vec2& point : points) {
              worldBB.buildAdd(transform.transformPoint(point));
            }

            //Write world bounds
            g.bounds->minX.at(i) = worldBB.min.x;
            g.bounds->minY.at(i) = worldBB.min.y;
            g.bounds->maxX.at(i) = worldBB.max.x;
            g.bounds->maxY.at(i) = worldBB.max.y;
          }
        }
      }
    }

    std::optional<MeshClassifier> classifier;
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

    void writeBoundaries(IAppBuilder& builder, ShapeRegistry::BroadphaseBounds& bounds) const final {
      builder.submitTask(TLSTask::createWithArgs<UpdateBoundaries, UpdateBoundaries::Group>("UpdateMeshBounds", UpdateBoundaries::Args{
        .transform = transform,
        .bounds = bounds
      }));
    }

    const MeshTransform transform;
  };

  std::unique_ptr<ShapeRegistry::IShapeImpl> createMesh(const MeshTransform& transform) {
    return std::make_unique<MeshImpl>(transform);
  }
}