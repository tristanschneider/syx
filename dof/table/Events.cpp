#include "Precompile.h"
#include "Events.h"

#include "AppBuilder.h"
#include "IAppModule.h"
#include "Database.h"
#include "RuntimeDatabase.h"
#include "StableElementID.h"
#include "TLSTaskImpl.h"

namespace Events {
  struct EventsStorage : ChainedRuntimeStorage {
    using ChainedRuntimeStorage::ChainedRuntimeStorage;

    std::vector<Events::EventsRow> rows;
  };

  struct ProcessCommands {
    ProcessCommands(RuntimeDatabaseTaskBuilder& task)
      : db{ task.getDatabase() }
      , ids{ task.getRefResolver() }
      , query{ task }
    {
    }

    void execute(AppTaskArgs&) {
      for(size_t t = 0; t < query.size(); ++t) {
        auto [stable, events] = query.get(t);
        RuntimeTable* srcTable = db.tryGet(query.getTableID(t));
        //Moving the table contents while iterating over this sparse row would require more complex sparse row iterator support.
        //Instead, modifications are gathered into vectors and performed in a second pass.
        for(auto it : *events) {
          assert(it.first < stable->size());
          const ElementRef& element = stable->at(it.first);
          const ElementEvent& event = it.second;
          if(event.isCreate()) {
            //Nothing currently, may want to support this in the future
          }
          else if(event.isDestroy()) {
            toRemove.push_back(element);
          }
          else if(event.isMove()) {
            if(RuntimeTable* dstTable = db.tryGet(event.getTableID())) {
              toMove.push_back(std::make_pair(element, dstTable));
            }
          }
        }

        //Perform all removals for this table
        for(const ElementRef& e : toRemove) {
          if(auto id = ids.unpack(e)) {
            srcTable->swapRemove(id.getElementIndex());
          }
          else {
            assert(false && "Element should be valid if being deleted");
          }
        }
        toRemove.clear();

        //Perform all moves from this table
        for(auto&& [e, dst] : toMove) {
          if(auto id = ids.unpack(e)) {
            //TODO: should the move event change to indicate the table it came from?
            RuntimeTable::migrate(id.getElementIndex(), *srcTable, *dst, 1);
          }
          else {
            assert(false && "Element should be valid to move");
          }
        }
        toMove.clear();
      }
    }

    RuntimeDatabase& db;
    QueryResult<const StableIDRow, const EventsRow> query;
    ElementRefResolver ids;
    std::vector<ElementRef> toRemove;
    std::vector<std::pair<ElementRef, RuntimeTable*>> toMove;
  };

  struct ClearEvents {
    ClearEvents(RuntimeDatabaseTaskBuilder& task)
      : query{ task }
    {}

    void execute(AppTaskArgs&) {
      for(size_t t = 0; t < query.size(); ++t) {
        auto [events] = query.get(t);
        events->clear();
      }
    }

    QueryResult<EventsRow> query;
  };

  struct EventsModule : IAppModule {
    void createDependentDatabase(RuntimeDatabaseArgs& args) final {
      //Gather all tables with stable rows
      std::vector<RuntimeTableRowBuilder*> tables;
      for(RuntimeTableRowBuilder& table : args.tables) {
        if(table.contains<StableIDRow>()) {
          tables.push_back(&table);
        }
      }

      //Add event rows to those tables
      EventsStorage* storage = RuntimeStorage::addToChain<EventsStorage>(args);
      storage->rows.resize(tables.size());
      for(size_t i = 0; i < storage->rows.size(); ++i) {
        DBReflect::addRow(storage->rows[i], args.tables[i]);
      }
    }

    void processEvents(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::create<ProcessCommands>("process events"));
    }

    void clearEvents(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::create<ClearEvents>("clear events"));
    }
  };

  std::unique_ptr<IAppModule> createModule() {
    return std::make_unique<EventsModule>();
  }
}