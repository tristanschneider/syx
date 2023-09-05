#include "AppBuilder.h"

namespace TableResolverImpl {
  struct TR : ITableResolver {
    TR(RuntimeDatabase& d)
      : db{ d } {
    }

    //TODO: optional access validation
    void* tryGetRow(const UnpackedDatabaseElementID id, IDT type) override {
      if(RuntimeTable* table = db.tryGet(id)) {
        auto result = table->rows.find(type);
        return result != table->rows.end() ? result->second : nullptr;
      }
      return nullptr;
    }

    RuntimeDatabase& db;
  };

  std::unique_ptr<ITableResolver> create(RuntimeDatabase& db) {
    return std::make_unique<TR>(db);
  }
}

namespace TableModifierImpl {
  struct TM : ITableModifier {
    TM(TableModifierInstance i)
      : instance{ i } {
    }

    size_t addElements(size_t count) override {
      return instance.addElements(count);
    }

    TableModifierInstance instance;
  };

  struct STM : ITableModifier {
    STM(StableTableModifierInstance i)
      : instance{ i } {
    }

    size_t addElements(size_t count) override {
      return instance.addElements(count);
    }

    StableTableModifierInstance instance;
  };
}

namespace AnyTableModifier {
  struct ATM : IAnyTableModifier {
    ATM(RuntimeDatabase& rdb)
      : db{ rdb } {
    }

    size_t addElements(const UnpackedDatabaseElementID& id, size_t count) override {
      if(RuntimeTable* table =  db.tryGet(id)) {
        if(table->modifier) {
          return table->modifier.addElements(count);
        }
        else if(table->stableModifier) {
          return table->stableModifier.addElements(count);
        }
      }
      return std::numeric_limits<size_t>::max();
    }

    RuntimeDatabase& db;
  };
}

RuntimeDatabaseTaskBuilder::RuntimeDatabaseTaskBuilder(RuntimeDatabase& rdb)
  : db{ rdb } {
}

AppTaskWithMetadata RuntimeDatabaseTaskBuilder::finalize()&& {
  return std::move(builtTask);
}

std::unique_ptr<ITableResolver> RuntimeDatabaseTaskBuilder::getResolver() {
  return TableResolverImpl::create(db);
}

void RuntimeDatabaseTaskBuilder::logRead(TypeIDT t) {
  builtTask.data.reads.push_back(t);
}

void RuntimeDatabaseTaskBuilder::logWrite(TypeIDT t) {
  builtTask.data.writes.push_back(t);
}

void RuntimeDatabaseTaskBuilder::logTableModifier(const UnpackedDatabaseElementID& id) {
  builtTask.data.tableModifiers.push_back(id);
}

std::unique_ptr<ITableModifier> RuntimeDatabaseTaskBuilder::getModifierForTable(const UnpackedDatabaseElementID& table) {
  logTableModifier(table);
  if(RuntimeTable* t = db.tryGet(table)) {
    if(t->modifier) {
      return std::make_unique<TableModifierImpl::TM>(t->modifier);
    }
    else if(t->stableModifier) {
      return std::make_unique<TableModifierImpl::STM>(t->stableModifier);
    }
  }
  return nullptr;
}

std::unique_ptr<IAnyTableModifier> RuntimeDatabaseTaskBuilder::getAnyModifier() {
  QueryResult<> q = db.query();
  for(auto&& t : q.matchingTableIDs) {
    logTableModifier(t);
  }
  return std::make_unique<AnyTableModifier::ATM>(db);
}
