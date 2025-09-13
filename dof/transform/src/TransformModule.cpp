#include <transform/TransformModule.h>

#include <IAppModule.h>
#include <RuntimeDatabase.h>
#include <TLSTaskImpl.h>
#include <transform/TransformRows.h>
#include <loader/ReflectionModule.h>
#include <loader/SceneAsset.h>
#include <Events.h>
#include <math/Geometric.h>

namespace Transform {
  //Recompute inverse transform for any elements that moved, then reset the flag
  struct UpdateTransform {
    struct Group {
      void init(TableID t) {
        table = t;
      }

      void init(RuntimeDatabaseTaskBuilder& task) {
        task.query(table, query);
      }

      QueryResult<
        const WorldTransformRow,
        WorldInverseTransformRow,
        TransformNeedsUpdateRow,
        TransformHasUpdatedRow
      > query;
      TableID table;
    };

    void init() {
    }

    void execute(Group& group) {
      for(size_t t = 0; t < group.query.size(); ++t) {
        auto [worlds, inverses, updates, notifications] = group.query.get(t);

        for(size_t i : *updates) {
          inverses->at(i) = worlds->at(i).inverse();
          notifications->getOrAdd(i);
        }
        updates->clear();
      }
    }
  };

  struct ClearNotifications {
    void init(RuntimeDatabaseTaskBuilder& task) {
      query = task;
    }

    void execute() {
      for(size_t t = 0; t < query.size(); ++t) {
        auto [updates] = query.get(t);
        updates->clear();
      }
    }

    QueryResult<TransformHasUpdatedRow> query;
  };

  //Flag any newly created or moved elements as needing a transform update
  struct FlagNewAndMoved {
    void init(RuntimeDatabaseTaskBuilder& task) {
      query = task;
    }

    void execute() {
      for(size_t t = 0; t < query.size(); ++t) {
        auto [events, updates] = query.get(t);
        for(auto it : *events) {
          if(it.second.isCreate() || it.second.isMove()) {
            updates->getOrAdd(it.first);
          }
        }
      }
    }

    QueryResult<
      const Events::EventsRow,
      TransformNeedsUpdateRow
    > query;
  };

  struct TransformLoader {
    using src_row = Loader::TransformRow;
    static constexpr std::string_view NAME = src_row::KEY;

    static Transform::PackedTransform toTransform(const Loader::Transform& t) {
      return Transform::PackedTransform::build(Transform::Parts{
        .rot = { std::cos(t.rot), std::sin(t.rot) },
        .scale = Geo::toVec2(t.scale),
        .translate = t.pos
      });
    }

    static void load(const IRow& src, RuntimeTable& dst, gnx::IndexRange range) {
      const Loader::TransformRow& s = static_cast<const Loader::TransformRow&>(src);
      Reflection::tryLoadRow<Transform::WorldTransformRow>(s, dst, range, &toTransform);
    }
  };

  class Impl : public IAppModule {
  public:
    void init(IAppBuilder& builder) final {
      Reflection::registerLoaders(builder,
        Reflection::createRowLoader(TransformLoader{})
      );
    }

    void update(IAppBuilder& builder) final {
      //Clear notifications since last frame, then write the new ones for the current frame.
      builder.submitTask(TLSTask::create<ClearNotifications>("ClearTransformNotifications"));
      for(TableID table : builder.queryTables<TransformNeedsUpdateRow>()) {
        updateInverse(builder, table);
      }
    }

    void postProcessEvents(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::create<FlagNewAndMoved>("TransformEvents"));
    }
  };

  StorageTableBuilder& addTransform25D(StorageTableBuilder& table) {
    return table.addRows<
      WorldTransformRow,
      WorldInverseTransformRow,
      TransformNeedsUpdateRow,
      TransformHasUpdatedRow
    >();
  }

  StorageTableBuilder& addPosXY(StorageTableBuilder& table) {
    return addTransform25D(table);
  }

  StorageTableBuilder& addTransform2D(StorageTableBuilder& table) {
    return addTransform25D(table);
  }

  StorageTableBuilder& addTransform2DNoScale(StorageTableBuilder& table) {
    return addTransform25D(table);
  }

  std::unique_ptr<IAppModule> createModule() {
    return std::make_unique<Impl>();
  }

  void updateInverse(IAppBuilder& builder, TableID table) {
    builder.submitTask(TLSTask::createWithArgs<UpdateTransform, UpdateTransform::Group>("UpdateTransform", table));
  }
}