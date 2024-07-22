#include "Precompile.h"
#include "shapes/Rectangle.h"
#include "AppBuilder.h"
#include "Physics.h"
#include "Geometric.h"

namespace TableExt {
  inline glm::vec2 read(size_t i, const Row<float>& a, const Row<float>& b) {
    return { a.at(i), b.at(i) };
  }
};

namespace Shapes {
  struct IndividualRectShape : ShapeRegistry::IShapeImpl {
    std::vector<TableID> queryTables(IAppBuilder& builder) const final {
      return builder.queryTables<const RectangleRow>().matchingTableIDs;
    }

    struct Classifier : ShapeRegistry::IShapeClassifier {
      Classifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& res)
        : resolver{ res }
      {
        //Log the dependency with get, but use the shared resolver
        task.getResolver(row);
      }

      ShapeRegistry::BodyType classifyShape(const UnpackedDatabaseElementID& id) final {
        if(const ShapeRegistry::Rectangle* rect = resolver.tryGetOrSwapRowElement(row, id)) {
          return { { *rect } };
        }
        return {};
      }

      ITableResolver& resolver;
      CachedRow<const RectangleRow> row;
    };

    std::shared_ptr<ShapeRegistry::IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& resolver) const final {
      return std::make_shared<Classifier>(task, resolver);
    }

    void writeBoundaries(IAppBuilder& builder, ShapeRegistry::BroadphaseBounds& bounds) const final {
      auto task = builder.createTask();
      task.setName("write rect indiv bounds");
      task.logDependency({ bounds.requiredDependency });

      auto query = task.query<const RectangleRow>(bounds.table);
      task.setCallback([query, &bounds](AppTaskArgs&) mutable {
        auto [rects] = query.get(0);
        const size_t s = rects->size();
        bounds.minX.resize(s);
        bounds.minY.resize(s);
        bounds.maxX.resize(s);
        bounds.maxY.resize(s);
        for(size_t i = 0; i < rects->size(); ++i) {
          const ShapeRegistry::Rectangle& rect = rects->at(i);
          const glm::vec2 extents = glm::abs(rect.right*rect.halfWidth.x) + glm::abs(Geo::orthogonal(rect.right*rect.halfWidth.y));
          bounds.minX[i] = rect.center.x - extents.x;
          bounds.maxX[i] = rect.center.x + extents.x;
          bounds.minY[i] = rect.center.y - extents.y;
          bounds.maxY[i] = rect.center.y + extents.y;
        }
      });

      builder.submitTask(std::move(task));
    }

    PhysicsAliases aliases;
  };

  struct SharedRectShape : ShapeRegistry::IShapeImpl {
    SharedRectShape(const RectDefinition& rectDef)
      : rect{ rectDef }
    {}

    std::vector<TableID> queryTables(IAppBuilder& builder) const final {
      return builder.queryTables<const SharedRectangleRow>().matchingTableIDs;
    }

    struct Classifier : ShapeRegistry::IShapeClassifier {
      Classifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& res, const RectDefinition& rectDef)
        : resolver{ res }
        , rect{ rectDef }
      {
        //Log the dependency with get, but use the shared resolver
        task.getAliasResolver(
          rect.centerX, rect.centerY,
          rect.rotX, rect.rotY,
          rect.scaleX, rect.scaleY
        );
      }

      ShapeRegistry::BodyType classifyShape(const UnpackedDatabaseElementID& id) final {
        size_t myIndex = id.getElementIndex();
        if(resolver.tryGetOrSwapRowAlias(rect.centerX, centerX, id) &&
          resolver.tryGetOrSwapRowAlias(rect.centerY, centerY, id)
        ) {
          ShapeRegistry::Rectangle result;
          result.center = TableExt::read(myIndex, *centerX, *centerY);
          //Rotation is optional
          if(resolver.tryGetOrSwapRowAlias(rect.rotX, rotX, id) &&
            resolver.tryGetOrSwapRowAlias(rect.rotY, rotY, id)
          ) {
            result.right = TableExt::read(myIndex, *rotX, *rotY);
          }
          if(resolver.tryGetOrSwapRowAlias(rect.scaleX, scaleX, id) &&
            resolver.tryGetOrSwapRowAlias(rect.scaleY, scaleY, id)) {
            result.halfWidth = TableExt::read(myIndex, *scaleX, *scaleY) * 0.5f;
          }
          return { ShapeRegistry::Variant{ result } };
        }
        return {};
      }

      const RectDefinition& rect;
      ITableResolver& resolver;
      CachedRow<const Row<float>> centerX;
      CachedRow<const Row<float>> centerY;
      CachedRow<const Row<float>> rotX;
      CachedRow<const Row<float>> rotY;
      CachedRow<const Row<float>> scaleX;
      CachedRow<const Row<float>> scaleY;
    };

    std::shared_ptr<ShapeRegistry::IShapeClassifier> createShapeClassifier(RuntimeDatabaseTaskBuilder& task, ITableResolver& resolver) const final {
      return std::make_shared<Classifier>(task, resolver, rect);
    }

    struct WriteArgs {
      const std::vector<float>* pos{};
      const std::vector<float>* rotX{};
      const std::vector<float>* rotY{};
      const std::vector<float>* scaleX{};
      const std::vector<float>* scaleY{};
      std::vector<float>* resultMin{};
      std::vector<float>* resultMax{};
    };
    template<
      bool hasRot,
      bool hasScale,
      int axis
    >
    static void writeAxis(RuntimeDatabaseTaskBuilder& task, WriteArgs& args) {
      task.setCallback([args](AppTaskArgs&) {
        args.resultMin->resize(args.pos->size());
        args.resultMax->resize(args.pos->size());
        for(size_t i = 0; i < args.pos->size(); ++i) {
          [[maybe_unused]] glm::vec2 right{ args.rotX->at(i), args.rotY->at(i) };
          [[maybe_unused]] glm::vec2 up = Geo::orthogonal(right);
          [[maybe_unused]] glm::vec2 scale{ 0.5f, 0.5f };
          if constexpr(hasScale) {
            scale.x = args.scaleX->at(i);
            scale.y = args.scaleY->at(i);
          }
          float extent = 0.5f;
          if constexpr(hasRot && hasScale) {
            //Scale is scaling the direction vector, so take the scaled right and up vectors and project them onto axis
            extent = std::abs(right[axis]*scale.x*0.5f) + std::abs(up[axis]*scale.y*0.5f);
          }
          else if constexpr(hasRot && !hasScale) {
            extent = std::abs(right[axis]*0.5f) + std::abs(up[axis]*0.5f);
          }
          else if constexpr(!hasRot && hasScale) {
            //Scale can't be negative so no abs is needed
            extent = scale[axis];
          }
          const float basePos = args.pos->at(i);
          args.resultMin->at(i) = basePos - extent;
          args.resultMax->at(i) = basePos + extent;
        }
      });
    }

    template<int axis>
    static void writeAxis(RuntimeDatabaseTaskBuilder& task, WriteArgs& args) {
      if(args.rotX && args.scaleX) {
        writeAxis<true, true, axis>(task, args);
      }
      else if(args.rotX && !args.scaleX) {
        writeAxis<true, false, axis>(task, args);
      }
      else if(!args.rotX && args.scaleX) {
        writeAxis<false, true, axis>(task, args);
      }
      else if(!args.rotX && !args.scaleX) {
        writeAxis<false, false, axis>(task, args);
      }
    }

    static void writeAxis(IAppBuilder& builder,
      ShapeRegistry::BroadphaseBounds& bounds,
      const ConstFloatQueryAlias& pos,
      const ConstFloatQueryAlias& rotX,
      const ConstFloatQueryAlias& rotY,
      const ConstFloatQueryAlias& scaleX,
      const ConstFloatQueryAlias& scaleY,
      std::vector<float>& resultMin,
      std::vector<float>& resultMax,
      int axis
    ) {
      auto task = builder.createTask();
      task.logDependency({ bounds.requiredDependency });
      WriteArgs args;
      args.pos = &task.queryAlias(bounds.table, pos).get<0>(0).mElements;
      auto rotQ = task.queryAlias(bounds.table, rotX, rotY);
      if(rotQ.size()) {
        auto [rx, ry] = rotQ.get(0);
        args.rotX = &rx->mElements;
        args.rotY = &ry->mElements;
      }
      auto scaleQ = task.queryAlias(bounds.table, scaleX, scaleY);
      if(scaleQ.size()) {
        auto [sx, sy] = scaleQ.get(0);
        args.scaleX = &sx->mElements;
        args.scaleY = &sy->mElements;
      }
      args.resultMin = &resultMin;
      args.resultMax = &resultMax;

      if(axis == 0) {
        writeAxis<0>(task, args);
      }
      else {
        writeAxis<1>(task, args);
      }

      task.setName("write rect axis");
      builder.submitTask(std::move(task));
    }

    void writeBoundaries(IAppBuilder& builder, ShapeRegistry::BroadphaseBounds& bounds) const final {
      writeAxis(builder,
        bounds,
        rect.centerX,
        rect.rotX,
        rect.rotY,
        rect.scaleX,
        rect.scaleY,
        bounds.minX,
        bounds.maxX,
        0);
      writeAxis(builder,
        bounds,
        rect.centerY,
        rect.rotX,
        rect.rotY,
        rect.scaleX,
        rect.scaleY,
        bounds.minY,
        bounds.maxY,
        1);
    }

    RectDefinition rect;
  };

  std::unique_ptr<ShapeRegistry::IShapeImpl> createIndividualRectangle() {
    return std::make_unique<IndividualRectShape>();
  }

  std::unique_ptr<ShapeRegistry::IShapeImpl> createSharedRectangle(const RectDefinition& rect) {
    return std::make_unique<SharedRectShape>(rect);
  }
}
