#include "Precompile.h"
#include "EventValidator.h"

#include "IAppModule.h"
#include "AppBuilder.h"
#include "TLSTaskImpl.h"
#include "DBEvents.h"
#include "RowTags.h"

namespace EventValidator {
  struct EventTracker {
    TableID lastKnownTable{};
    std::string_view tableName{};
  };

  struct EventValidatorGroup {
    using TrackingMap = std::unordered_map<ElementRef, EventTracker>;
    using TrackingIterator = typename TrackingMap::iterator;

    EventValidatorGroup(RuntimeDatabaseTaskBuilder&, std::string_view name)
      : moduleName{ name }
    {}

    TrackingMap trackedElements;
    std::string_view moduleName;
  };

  struct ElementLookup {
    TableID table{};
    bool existsInTable{};
    EventValidatorGroup::TrackingIterator tracked;
    std::string_view tableName{ "?" };
  };

  constexpr bool enableLogs = true;

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

  void logCreate([[maybe_unused]] const ElementLookup& lookup, const ElementRef& e) {
    if constexpr(enableLogs) {
      printf("Create %p,%d in %s\n", e.uncheckedGet(), (int)e.getExpectedVersion(), lookup.tableName.data());
    }
  }

  void logMove([[maybe_unused]] const ElementLookup& lookup, const ElementRef& e) {
    if constexpr(enableLogs) {
      printf("Move %p,%d to %s\n", e.uncheckedGet(), e.getExpectedVersion(), lookup.tableName.data());
    }
  }

  void logDestroy([[maybe_unused]] const ElementLookup& lookup, const ElementRef& e) {
    if constexpr(enableLogs) {
      printf("Destroy %p,%d from %s\n", e.uncheckedGet(), e.getExpectedVersion(), lookup.tableName.data());
    }
  }

  struct EventValidatorBase {
    EventValidatorBase(RuntimeDatabaseTaskBuilder& task)
      : events{ Events::getPublishedEvents(task) }
      , res{ task.getResolver(stable, tableName) }
      , ids{ task.getIDResolver()->getRefResolver() }
    {
    }

    ElementLookup find(const ElementRef& e, EventValidatorGroup& group) {
      ElementLookup result;
      const UnpackedDatabaseElementID id = ids.unpack(e);
      result.table = TableID{ id };
      if(const ElementRef* found = res->tryGetOrSwapRowElement(stable, id)) {
        result.existsInTable = *found == e;
        if(const Tags::TableName* name = res->tryGetOrSwapRowElement(tableName, id)) {
          result.tableName = name->name;
        }
      }
      result.tracked = group.trackedElements.find(e);
      return result;
    }

    std::string_view getTableName(const TableID& table) {
      return res->tryGetOrSwapRow(tableName, table) ? std::string_view{ tableName->at().name } : std::string_view{ "?" };
    }

    void validateNewElement(const DBEvents::MoveCommand& cmd, EventValidatorGroup& group, bool doLogs) {
      auto from = std::get_if<ElementRef>(&cmd.source);
      auto to = std::get_if<ElementRef>(&cmd.destination);
      assert(to);
      if(!to) {
        return;
      }

      //New element created, should exist as they are emitted upon creation of the element
      const ElementLookup lookup = find(*to, group);
      assert(lookup.existsInTable);
      assert(!from && "Event should only refer to a single element");
      assert(std::holds_alternative<std::monostate>(cmd.source) && "When destination exists it should be a new event");
      assert(lookup.tracked == group.trackedElements.end() && "New element should be new");

      if(doLogs) {
        logCreate(lookup, *to);
      }

      group.trackedElements[*to] = EventTracker{
        .lastKnownTable = lookup.table,
        .tableName = lookup.tableName
      };
    }

    const DBEvents& events;
    CachedRow<const StableIDRow> stable;
    CachedRow<const Tags::TableNameRow> tableName;
    std::shared_ptr<ITableResolver> res;
    ElementRefResolver ids;
  };

  struct PreValidator : EventValidatorBase {
    using EventValidatorBase::EventValidatorBase;

    void execute(EventValidatorGroup& group, AppTaskArgs&) {
      logBegin();
      for(const auto& cmd : events.toBeMovedElements) {
        auto from = std::get_if<ElementRef>(&cmd.source);
        auto to = std::get_if<ElementRef>(&cmd.destination);
        assert(from || to && L"Event should always refer to an element");
        if(from) {
          //Move or destroy. The element should still exist because it hasn't happened yet
          const ElementLookup lookup = find(*from, group);
          assert(lookup.existsInTable);
          assert(lookup.tracked != group.trackedElements.end() && "Element must exist to be moved or destroyed");
          if(lookup.tracked != group.trackedElements.end()) {
            assert(lookup.table == lookup.tracked->second.lastKnownTable && "Table should match last event");
          }
          assert(!to && "Event should only refer to a single element");

          if(lookup.tracked != group.trackedElements.end()) {
            if(std::holds_alternative<std::monostate>(cmd.destination)) {
              logDestroy(lookup, *from);
              group.trackedElements.erase(lookup.tracked);
            }
            else {
              const TableID newTable = std::get<TableID>(cmd.destination);
              lookup.tracked->second.lastKnownTable = newTable;
              lookup.tracked->second.tableName = getTableName(newTable);
              logMove(lookup, *from);
            }
          }
        }
        else if(to) {
          validateNewElement(cmd, group, true);
        }
      }
      logEnd();
    }
  };

  struct PostValidator : EventValidatorBase {
    using EventValidatorBase::EventValidatorBase;

    void execute(EventValidatorGroup& group, AppTaskArgs&) {
      for(const auto& cmd : events.toBeMovedElements) {
        auto from = std::get_if<ElementRef>(&cmd.source);
        auto to = std::get_if<ElementRef>(&cmd.destination);
        assert(from || to && L"Event should always refer to an element");
        if(from) {
          //Move or destroy. The element should now either be at the new location or destroyed.
          const ElementLookup lookup = find(*from, group);
          assert(lookup.tracked != group.trackedElements.end() && "Element must exist to be moved or destroyed");

          assert(!to && "Event should only refer to a single element");

          if(lookup.tracked != group.trackedElements.end()) {
            size_t h = std::hash<ElementRef>{}(lookup.tracked->first);
            h;

            if(std::holds_alternative<std::monostate>(cmd.destination)) {
              assert(!lookup.existsInTable && "Element should have been destroyed");
              group.trackedElements.erase(lookup.tracked);
            }
            else {
              const TableID newTable = std::get<TableID>(cmd.destination);
              lookup.tracked->second.lastKnownTable = newTable;
              lookup.tracked->second.tableName = getTableName(newTable);
              assert(lookup.table == newTable && "Element should have arrived in the table it indicated to move to");
            }
          }
        }
        else if(to) {
          validateNewElement(cmd, group, false);
        }
      }
    }
  };

  struct ProcessValidator {
    ProcessValidator(RuntimeDatabaseTaskBuilder& task)
      : events{ Events::getPublishedEvents(task) } {
    }

    void execute(AppTaskArgs&) {
      logProcess();
    }

    const DBEvents& events;
  };

  struct EventValidatorModule : IAppModule {
    EventValidatorModule(std::string_view name)
      : moduleName{ name } {
    }

    void preProcessEvents(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::createWithArgs<PreValidator, EventValidatorGroup, DefaultTaskLocals>("prevalidate", moduleName));
    }

    void processEvents(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::create<ProcessValidator>("validate"));
    }

    void postProcessEvents(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::createWithArgs<PostValidator, EventValidatorGroup, DefaultTaskLocals>("postvalidate", moduleName));
    }

    std::string_view moduleName;
  };

  std::unique_ptr<IAppModule> createModule(std::string_view name) {
    return std::make_unique<EventValidatorModule>(name);
  }
}