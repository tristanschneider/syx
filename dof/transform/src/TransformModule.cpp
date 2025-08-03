#include <transform/TransformModule.h>

#include <IAppModule.h>
#include <RuntimeDatabase.h>
#include <TLSTaskImpl.h>
#include <transform/TransformRows.h>
#include <Events.h>

namespace Transform {
  //Recompute inverse transform for any elements that moved, then reset the flag
  struct UpdateTransform {
    void init(RuntimeDatabaseTaskBuilder& task) {
      query = task;
    }

    void execute() {
      for(size_t t = 0; t < query.size(); ++t) {
        auto [worlds, inverses, updates] = query.get(t);
        for(size_t i : *updates) {
          inverses->at(i) = worlds->at(i).inverse();
        }
        updates->clear();
      }
    }

    QueryResult<
      const WorldTransformRow,
      WorldInverseTransformRow,
      TransformNeedsUpdateRow
    > query;
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

  class Impl : public IAppModule {
  public:
    void update(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::create<UpdateTransform>("UpdateTransform"));
    }

    void postProcessEvents(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::create<FlagNewAndMoved>("TransformEvents");
    }
  };

  StorageTableBuilder& addTransform25D(StorageTableBuilder& table) {
    table.addRows<
      WorldTransformRow,
      WorldInverseTransformRow,
      TransformNeedsUpdateRow
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
}