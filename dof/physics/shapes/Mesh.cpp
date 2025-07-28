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
#include <PhysicsTableBuilder.h>
#include <TableName.h>

#include <ConstraintSolver.h>
#include <Narrowphase.h>

namespace Shapes {
  //This is equivalent to Relation::HasChildrenRow but allows children of mesh asset entries to exist unrelated to composite meshes.
  struct CompositeMesh {
    std::vector<ElementRef> parts;
  };
  struct CompositeMeshRow : Row<CompositeMesh> {};
  struct CompositeMeshTagRow : TagRow {};

  struct TriangleMesh {
    TriangleMesh() = default;
    TriangleMesh(const pt::PackedTransform& t, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c)
      : points{ t.transformPoint(a), t.transformPoint(b), t.transformPoint(c) }
      , aabb{ Geo::AABB::build({ points[0], points[1], points[2] }) }
    {
    }

    ShapeRegistry::Mesh toMesh() const {
      //Points are in world space, so transforms are identity
      return ShapeRegistry::Mesh{
        .points = points,
        .aabb = aabb,
      };
    }

    std::vector<glm::vec2> points;
    Geo::AABB aabb;
  };
  struct TriangleMeshRow : Row<TriangleMesh> {};

  struct MeshStorage : ChainedRuntimeStorage {
    using ChainedRuntimeStorage::ChainedRuntimeStorage;
    struct Rows {
      MeshAssetRow mesh;
      //Reference to elements in the compositeAssets table for composite meshes
      CompositeMeshRow composite;
    };

    std::vector<Rows> rows;
    //Pieces of the full model broken down into a bunch of smaller ones.
    //These are assets referenced by MeshReferenceRow
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

    static void fillModelFromPoints(MeshAsset& mesh, ConvexHull::Context& ctx) {
      ConvexHull::compute(mesh.points, ctx);
      mesh.convexHull.resize(ctx.result.size());
      std::transform(ctx.result.begin(), ctx.result.end(), mesh.convexHull.begin(), ConvexHull::GetPoints{ mesh.points });
      mesh.aabb = computeAABB(mesh);
    }

    static MeshAsset createMesh(const Loader::MeshAsset& mesh) {
      MeshAsset result;
      result.points.resize(mesh.verts.size());
      //Copy triangle primitives as-is into verts
      std::transform(mesh.verts.begin(), mesh.verts.end(), result.points.begin(), [](const Loader::MeshVertex& v) {
        return glm::vec2{ v.pos.x, v.pos.y };
      });
      ConvexHull::Context ctx;
      fillModelFromPoints(result, ctx);
      //Mass properties will be computed based on convex hull by MassModule
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

      //Find all "valid" triangles as ones that have some amount of area.
      //Points are assumed to be in triplets
      for(size_t i = 0; i + 2 < mesh.points.size(); i += 3) {
        const glm::vec2& a = mesh.points[i];
        const glm::vec2& b = mesh.points[i + 1];
        const glm::vec2& c = mesh.points[i + 2];
        const float area = std::abs(Geo::cross(b - a, c - a));
        if(area > 0.01f) {
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
      ConvexHull::Context ctx;

      for(size_t i = 0; i < triIndices.size(); ++i) {
        MeshAsset& childMesh = childMeshes->at(i + result.startIndex);
        const size_t ti = triIndices[i];
        childMesh.points.insert(childMesh.points.end(), {
          mesh.points[ti],
          mesh.points[ti + 1],
          mesh.points[ti + 2]
        });
        fillModelFromPoints(childMesh, ctx);

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

  struct InitCompositeMeshTask {
    struct Group {
      void init(const pt::FullTransformAlias& a) {
        transformAlias = a;
      }

      void init(RuntimeDatabaseTaskBuilder& task) {
        transformResolver = pt::FullTransformResolver{ task, transformAlias };
      }

      pt::FullTransformAlias transformAlias;
      pt::FullTransformResolver transformResolver;
    };

    void init(RuntimeDatabaseTaskBuilder& task) {
      triangleMeshTable = task.queryTables<TriangleMeshRow>().getTableID(0);
      query = task;
      ids = task.getRefResolver();
      resolver = task.getResolver(meshAssets, compositeMeshAssets);
    }

    void init(AppTaskArgs& args) {
      relation = args;
    }

    struct ChildRows {
      ChildRows() = default;
      ChildRows(RuntimeTable& table)
        : triangleMeshes{ table.tryGet<TriangleMeshRow>() }
        , constraintMasks{ table.tryGet<ConstraintSolver::ConstraintMaskRow>() }
        , collisionMasks{ table.tryGet<Narrowphase::CollisionMaskRow>() }
      {}

      explicit operator bool() const {
        return triangleMeshes && constraintMasks && collisionMasks;
      }

      TriangleMeshRow* triangleMeshes{};
      ConstraintSolver::ConstraintMaskRow* constraintMasks{};
      Narrowphase::CollisionMaskRow* collisionMasks{};
    };

    void execute(Group& g) {
      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [events, stables, meshRefs, constraintMasks, collisionMasks, children] = query.get(t);
        for(auto event : events) {
          if(!event.second.isCreate()) {
            continue;
          }
          const size_t si = event.first;
          const UnpackedDatabaseElementID assetID = ids.unpack(meshRefs->at(si).meshAsset.asset);
          const CompositeMesh* composite = resolver->tryGetOrSwapRowElement(compositeMeshAssets, assetID);
          if(!composite || composite->parts.empty()) {
            continue;
          }

          Relation::RelationWriter::NewChildren newChildren = relation.addChildren(stables->at(si), children->at(si), triangleMeshTable, composite->parts.size());
          ChildRows newRows = newChildren.table ? ChildRows{ *newChildren.table } : ChildRows{};
          if(!newChildren.count || !newRows) {
            continue;
          }

          const ElementRef& parentID = stables->at(si);
          const pt::PackedTransform parentTransform = g.transformResolver.resolve(ids.unpack(parentID)).toPacked();
          const Narrowphase::CollisionMask parentCollisionMask = collisionMasks->at(si);
          const ConstraintSolver::ConstraintMask parentConstraintMask = constraintMasks->at(si);

          for(size_t c = 0; c < newChildren.count; ++c) {
            const MeshAsset* childMesh = resolver->tryGetOrSwapRowElement(meshAssets, ids.unpack(composite->parts[c]));
            //Skip meshes that fail to resolve. This shouldn't happen. If it does, the new child will be useless but also shouldn't cause problems.
            //If they resolved, they should have 3 points as constructed by ImportMeshTask.
            if(!childMesh || childMesh->points.size() != 3) {
              continue;
            }
            const size_t ci = c + newChildren.startIndex;
            //Copy points for this child mesh to world space TriangleMesh using parent transform
            newRows.triangleMeshes->at(ci) = TriangleMesh{ parentTransform, childMesh->convexHull[0], childMesh->convexHull[1], childMesh->convexHull[2] };
            //Copy over parent masks
            newRows.collisionMasks->at(ci) = parentCollisionMask;
            newRows.constraintMasks->at(ci) = parentConstraintMask;
          }
        }
      }
    }

    QueryResult<
      const Events::EventsRow,
      const StableIDRow,
      const StaticTriangleMeshReferenceRow,
      const ConstraintSolver::ConstraintMaskRow,
      const Narrowphase::CollisionMaskRow,
      Relation::HasChildrenRow
    > query;
    Relation::RelationWriter relation;
    TableID triangleMeshTable;
    CachedRow<const CompositeMeshRow> compositeMeshAssets;
    CachedRow<const MeshAssetRow> meshAssets;
    std::shared_ptr<ITableResolver> resolver;
    ElementRefResolver ids;
  };

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

  class MeshModule : public IAppModule {
  public:
    MeshModule(const MeshTransform& meshTransform)
      : transformAlias{ getTransformAlias(meshTransform) }
    {
    }

    void postProcessEvents(IAppBuilder& builder) override {
      builder.submitTask(TLSTask::create<ImportMeshTask>("import mesh"));
      builder.submitTask(TLSTask::createWithArgs<InitCompositeMeshTask, InitCompositeMeshTask::Group>("InitCompositeMesh", transformAlias));
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

      //Add table for instances of those child meshes.
      std::invoke([] {
        StorageTableBuilder table;
        //Add rows needed for collision, omit mass which means a static (infinite mass) mesh
        PhysicsTableBuilder::addRigidbody(table);
        PhysicsTableBuilder::addCollider(table);
        PhysicsTableBuilder::addImmobile(table);
        //TODO: these need thickness and Z position
        table.addRows<
          TriangleMeshRow,
          //So that pt::FullTransformResolver uses this.
          //It will always be identity since points are in world space
          pt::TransformRow
        >().setStable().setTableName({ "Triangle Instances" });
        return table;
      }).finalize(args);
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
        tryLoadRow<StaticTriangleMeshReferenceRow>(s, dst, range, GetMember<&Loader::MatMeshRef::mesh>{});
      }
    };

    void init(IAppBuilder& builder) override {
      Reflection::registerLoaders(builder,
        Reflection::createRowLoader(MatMeshLoader{})
      );
    }

    const pt::FullTransformAlias transformAlias;
  };

  std::unique_ptr<IAppModule> createMeshModule(const MeshTransform& meshTransform) {
    return std::make_unique<MeshModule>(meshTransform);
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
      //TODO: avoid recomputing by storing in table with the objects and making transformResolver deal with this
      const pt::TransformPair transform = transformResolver.resolvePair(id);
      if(asset) {
        return { ShapeRegistry::Mesh{
          .points = asset->convexHull,
          .aabb = asset->aabb,
          .modelToWorld = transform.modelToWorld,
          .worldToModel = transform.worldToModel
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

  class StaticTriangleMeshClassifier : public ShapeRegistry::IShapeClassifier {
  public:
    StaticTriangleMeshClassifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& res)
      : tableResolver{ res }
    {
      task.getResolver(meshRef);
    }

    ShapeRegistry::BodyType classifyShape(const UnpackedDatabaseElementID& id) final {
      if(const TriangleMesh* ref = tableResolver.tryGetOrSwapRowElement(meshRef, id)) {
        return { ShapeRegistry::Mesh{
          .points = ref->points,
          .aabb = ref->aabb
        }};
      }
      return {};
    }

    CachedRow<const TriangleMeshRow> meshRef;
    ITableResolver& tableResolver;
  };

  void resizeBounds(ShapeRegistry::BroadphaseBounds& bounds, size_t size) {
    bounds.minX.resize(size);
    bounds.minY.resize(size);
    bounds.maxX.resize(size);
    bounds.maxY.resize(size);
  }

  void writeWorldBounds(ShapeRegistry::BroadphaseBounds& bounds, size_t i, const Geo::AABB& bb) {
    bounds.minX.at(i) = bb.min.x;
    bounds.minY.at(i) = bb.min.y;
    bounds.maxX.at(i) = bb.max.x;
    bounds.maxY.at(i) = bb.max.y;
  }

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
        resizeBounds(*g.bounds, px->size());

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

            writeWorldBounds(*g.bounds, i, worldBB);
          }
        }
      }
    }

    std::optional<MeshClassifier> classifier;
  };

  struct UpdateStaticTriangleMeshBoundaries {
    struct Args {
      ShapeRegistry::BroadphaseBounds& bounds;
    };
    struct Group {
      void init(const Args& args) {
        bounds = &args.bounds;
      }

      void init(RuntimeDatabaseTaskBuilder& task) {
        task.logDependency({ bounds->requiredDependency });
        childMeshQuery = task.query<
          const TriangleMeshRow
        >(bounds->table);
      }

      ShapeRegistry::BroadphaseBounds* bounds{};
      QueryResult<
        const TriangleMeshRow
      > childMeshQuery;
    };

    void init() {}

    void execute(Group& g) {
      //TODO: do nothing because they're static
      for(size_t t = 0; t < g.childMeshQuery.size(); ++t) {
        auto&& [mesh] = g.childMeshQuery.get(t);
        resizeBounds(*g.bounds, mesh->size());

        for(size_t i = 0; i < mesh->size(); ++i) {
          writeWorldBounds(*g.bounds, i, mesh->at(i).aabb);
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

  class StaticTriangleMeshImpl : public ShapeRegistry::IShapeImpl {
    std::vector<TableID> queryTables(IAppBuilder& builder) const final {
      return builder.queryTables<TriangleMeshRow>().getMatchingTableIDs();
    }

    std::shared_ptr<ShapeRegistry::IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& resolver) const final {
      return std::make_shared<StaticTriangleMeshClassifier>(task, resolver);
    }

    void writeBoundaries(IAppBuilder& builder, ShapeRegistry::BroadphaseBounds& bounds) const final {
      builder.submitTask(TLSTask::createWithArgs<UpdateStaticTriangleMeshBoundaries, UpdateStaticTriangleMeshBoundaries::Group>("UpdateMeshBounds", UpdateStaticTriangleMeshBoundaries::Args{
        .bounds = bounds
      }));
    }
  };

  std::unique_ptr<ShapeRegistry::IShapeImpl> createMesh(const MeshTransform& transform) {
    return std::make_unique<MeshImpl>(transform);
  }

  std::unique_ptr<ShapeRegistry::IShapeImpl> createStaticTriangleMesh() {
    return std::make_unique<StaticTriangleMeshImpl>();
  }
}