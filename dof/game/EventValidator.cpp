#include "Precompile.h"
#include "EventValidator.h"

#include "IAppModule.h"
#include "AppBuilder.h"
#include "TLSTaskImpl.h"
#include "Events.h"
#include "RowTags.h"
#include "TableName.h"

namespace EventValidator {
  struct EventTracker {
    TableID lastKnownTable{};
    std::string_view tableName{};
  };

  struct EventValidatorGroup {
    using TrackingMap = std::unordered_map<ElementRef, EventTracker>;
    using TrackingIterator = typename TrackingMap::iterator;

    void init(std::string_view name) {
      moduleName = name;
    }

    TrackingMap trackedElements;
    std::string_view moduleName;
  };

  struct ElementLookup {
    ElementRef element;
    TableID table{};
    bool existsInTable{};
    EventValidatorGroup::TrackingIterator tracked;
    std::string_view tableName{ "?" };
  };

  constexpr bool enableLogs = false;

  void logBegin() {
    if constexpr(enableLogs) {
      printf("Tick\n");
    }
  }

  void logEnd() {
    if constexpr(enableLogs) {
    }
  }

  void logProcess() {
    if constexpr(enableLogs) {
      printf("Process\n");
    }
  }

  void logCreate([[maybe_unused]] const ElementLookup& lookup) {
    if constexpr(enableLogs) {
      printf("Create %p,%d in %s\n", lookup.element.uncheckedGet(), (int)lookup.element.getExpectedVersion(), lookup.tableName.data());
    }
  }

  void logMove([[maybe_unused]] const ElementLookup& lookup) {
    if constexpr(enableLogs) {
      printf("Move %p,%d to %s\n", lookup.element.uncheckedGet(), lookup.element.getExpectedVersion(), lookup.tableName.data());
    }
  }

  void logDestroy([[maybe_unused]] const ElementLookup& lookup) {
    if constexpr(enableLogs) {
      printf("Destroy %p,%d from %s\n", lookup.element.uncheckedGet(), lookup.element.getExpectedVersion(), lookup.tableName.data());
    }
  }

  struct EventValidatorBase {
    void init(RuntimeDatabaseTaskBuilder& task) {
      query = task;
      res = task.getResolver(tableName);
      ids = task.getIDResolver()->getRefResolver();
    }

    ElementLookup find(EventValidatorGroup& group, size_t tableIndex, size_t elementIndex) {
      auto [events, stables, names] = query.get(tableIndex);
      if(elementIndex >= stables->size()) {
        return {};
      }

      ElementLookup result;
      const ElementRef& e = stables->at(elementIndex);
      const UnpackedDatabaseElementID id = ids.unpack(e);
      result.existsInTable = static_cast<bool>(e);
      if(result.existsInTable) {
        assert(e.getMapping()->getTableIndex() == query[tableIndex].getTableIndex());
        assert(e.getMapping()->getElementIndex() == elementIndex);
      }
      result.table = TableID{ id };
      result.existsInTable = true;
      result.tableName = names->at(id.getElementIndex()).name;
      result.tracked = group.trackedElements.find(e);
      result.element = e;
      return result;
    }

    std::string_view getTableName(const TableID& table) {
      return res->tryGetOrSwapRow(tableName, table) ? std::string_view{ tableName->at().name } : std::string_view{ "?" };
    }

    void validateNewElement(EventValidatorGroup& group, size_t tableIndex, size_t elementIndex, bool doLogs) {
      auto [events, stables, names] = query.get(tableIndex);
      assert(elementIndex < stables->size());
      const ElementRef& newElement = stables->at(elementIndex);

      //New element created, should exist as they are emitted upon creation of the element
      const ElementLookup lookup = find(group, tableIndex, elementIndex);
      assert(lookup.existsInTable);
      assert(lookup.tracked == group.trackedElements.end() && "New element should be new");

      if(doLogs) {
        logCreate(lookup);
      }

      group.trackedElements[newElement] = EventTracker{
        .lastKnownTable = lookup.table,
        .tableName = lookup.tableName
      };
    }

    static void assertValidEvent(const Events::ElementEvent& e) {
      assert(e.isCreate() || e.isMove() || e.isDestroy());
    }

    QueryResult<
      const Events::EventsRow,
      const StableIDRow,
      const TableName::TableNameRow
    > query;
    CachedRow<const TableName::TableNameRow> tableName;
    std::shared_ptr<ITableResolver> res;
    ElementRefResolver ids;
  };

  struct PreValidator : EventValidatorBase {
    void execute(EventValidatorGroup& group) {
      logBegin();
      for(size_t t = 0; t < query.size(); ++t) {
        auto [events, stables, names] = query.get(t);
        for(auto event : *events) {
          const Events::ElementEvent& e = event.second;
          assertValidEvent(e);
          if(e.isCreate()) {
            validateNewElement(group, t, event.first, true);
          }
          if(e.isDestroy() || e.isMove()) {
            //Move or destroy. The element should still exist because it hasn't happened yet
            const ElementLookup lookup = find(group, t, event.first);
            assert(lookup.existsInTable);
            assert(lookup.tracked != group.trackedElements.end() && "Element must exist to be moved or destroyed");
            if(lookup.tracked != group.trackedElements.end()) {
              assert(lookup.table == lookup.tracked->second.lastKnownTable && "Table should match last event");
            }

            if(lookup.tracked != group.trackedElements.end()) {
              if(e.isDestroy()) {
                logDestroy(lookup);
                group.trackedElements.erase(lookup.tracked);
              }
              else {
                const TableID newTable = event.second.getTableID();
                lookup.tracked->second.lastKnownTable = newTable;
                lookup.tracked->second.tableName = getTableName(newTable);
                logMove(lookup);
              }
            }
          }
        }
      }
      logEnd();
    }
  };

  struct PostValidator : EventValidatorBase {
    void execute(EventValidatorGroup& group) {
      for(size_t t = 0; t < query.size(); ++t) {
        auto [events, stables, names] = query.get(t);
        for(auto event : *events) {
          assertValidEvent(event.second);

          if(event.second.isCreate()) {
            validateNewElement(group, t, event.first, false);
          }

          if(event.second.isMove() || event.second.isDestroy()) {
            //Move or destroy. The element should now either be at the new location or destroyed.
            const ElementLookup lookup = find(group, t, event.first);
            assert(lookup.tracked != group.trackedElements.end() && "Element must exist to be moved or destroyed");

            if(lookup.tracked != group.trackedElements.end()) {
              if(event.second.isDestroy()) {
                assert(!lookup.existsInTable && "Element should have been destroyed");
                group.trackedElements.erase(lookup.tracked);
              }
              else {
                const TableID newTable = event.second.getTableID();
                lookup.tracked->second.lastKnownTable = newTable;
                lookup.tracked->second.tableName = getTableName(newTable);
                assert(lookup.table == newTable && "Element should have arrived in the table it indicated to move to");
              }
            }
          }
        }
      }
    }
  };

  struct ProcessValidator {
    void init(RuntimeDatabaseTaskBuilder& task) {
      events = task;
    }

    void execute() {
      logProcess();
    }

    //Artificial dependency to ensure the scheduler doesn't skip this past other events
    QueryResult<const Events::EventsRow> events;
  };

  struct EventValidatorModule : IAppModule {
    EventValidatorModule(std::string_view name)
      : moduleName{ name } {
    }

    void preProcessEvents(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::createWithArgs<PreValidator, EventValidatorGroup>("prevalidate", moduleName));
    }

    void processEvents(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::create<ProcessValidator>("validate"));
    }

    void postProcessEvents(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::createWithArgs<PostValidator, EventValidatorGroup>("postvalidate", moduleName));
    }

    std::string_view moduleName;
  };

  std::unique_ptr<IAppModule> createModule(std::string_view name) {
    return std::make_unique<EventValidatorModule>(name);
  }
}