#include "Precompile.h"
#include "AppBuilder.h"

namespace TableResolverImpl {
  struct TR : ITableResolver {
    TR(RuntimeDatabase& d)
      : db{ d } {
    }

    //TODO: optional access validation
    IRow* tryGetRow(const UnpackedDatabaseElementID id, IDT type) override {
      RuntimeTable* table = db.tryGet(TableID{ id });
      return table ? table->tryGet(type) : nullptr;
    }

    RuntimeDatabase& db;
  };

  std::unique_ptr<ITableResolver> create(RuntimeDatabase& db) {
    return std::make_unique<TR>(db);
  }
}

namespace TableModifierImpl {
  struct TM : ITableModifier {
    TM(RuntimeTable& table)
      : instance{ table } {
    }

    size_t addElements(size_t count) override {
      return instance.addElements(count);
    }

    void resize(size_t count) override {
      instance.resize(count);
    }

    void resizeWithIDs(size_t newSize, const ElementRef* ids) {
      instance.resize(newSize, ids);
    }

    void swapRemove(const UnpackedDatabaseElementID& id) override {
      instance.swapRemove(id.getElementIndex());
    }

    //void insert(const UnpackedDatabaseElementID& location, size_t count) override {
    //  instance.modifier.insert(instance.table, location, count);
    //}

    RuntimeTable instance;
  };
}

namespace IDResolverImpl {
  //TODO: is this worth it or should the description be more accessible?
  struct Impl : IIDResolver {
    Impl(RuntimeDatabase& db)
      : description{ db.getDescription() }
      , mappings{ db.getMappings() } {
      //Log dependency
      db.query<const StableIDRow>();
    }

    size_t getTotalIds() const final {
      return mappings.size();
    }

    ElementRef createKey() final {
      return mappings.createKey();
    }

    ElementRefResolver getRefResolver() const {
      return { description };
    }

    DatabaseDescription description;
    StableElementMappings& mappings;
  };
}

RuntimeDatabaseTaskBuilder::RuntimeDatabaseTaskBuilder(RuntimeDatabase& rdb)
  : db{ rdb } {
}

RuntimeDatabaseTaskBuilder::~RuntimeDatabaseTaskBuilder() {
  assert(submitted);
}

void RuntimeDatabaseTaskBuilder::discard() {
  submitted = true;
}

AppTaskWithMetadata RuntimeDatabaseTaskBuilder::finalize()&& {
  submitted = true;
  return std::move(builtTask);
}

std::shared_ptr<ITableResolver> RuntimeDatabaseTaskBuilder::getResolver() {
  return TableResolverImpl::create(db);
}

std::shared_ptr<IIDResolver> RuntimeDatabaseTaskBuilder::getIDResolver() {
  return std::make_unique<IDResolverImpl::Impl>(db);
}

void RuntimeDatabaseTaskBuilder::logDependency(std::initializer_list<QueryAliasBase> aliases) {
  auto tables = db.queryAliasTables(aliases);
  for(const QueryAliasBase& q : aliases) {
    log(q, tables.matchingTableIDs);
  }
}

void RuntimeDatabaseTaskBuilder::log(const std::vector<TableID>& tableIds, const TypeIDT& id, bool isConst) {
  for(const TableID& table : tableIds) {
    if(isConst) {
      logRead(table, id);
    }
    else {
      logWrite(table, id);
    }
  }
}

void RuntimeDatabaseTaskBuilder::log(const QueryAliasBase& alias, const std::vector<TableID>& tableIds) {
  log(tableIds, alias.type, alias.isConst);
}

void RuntimeDatabaseTaskBuilder::logRead(const TableID& table, TypeIDT t) {
  builtTask.data.reads.push_back({ t, table });
}

void RuntimeDatabaseTaskBuilder::logWrite(const TableID& table, TypeIDT t) {
  builtTask.data.writes.push_back({ t, table });
}

void RuntimeDatabaseTaskBuilder::logTableModifier(const TableID& id) {
  builtTask.data.tableModifiers.push_back(id);
}

std::shared_ptr<ITableModifier> RuntimeDatabaseTaskBuilder::getModifierForTable(const TableID& table) {
  logTableModifier(table);
  if(RuntimeTable* t = db.tryGet(table)) {
    return std::make_unique<TableModifierImpl::TM>(*t);
  }
  return nullptr;
}

std::vector<std::shared_ptr<ITableModifier>> RuntimeDatabaseTaskBuilder::getModifiersForTables(const std::vector<TableID>& tables) {
  std::vector<std::shared_ptr<ITableModifier>> result(tables.size());
  for(size_t i = 0; i < result.size(); ++i) {
    result[i] = getModifierForTable(tables[i]);
  }
  return result;
}

std::shared_ptr<AppTaskConfig> RuntimeDatabaseTaskBuilder::getConfig() {
  if(!builtTask.task.config) {
    builtTask.task.config = std::make_shared<AppTaskConfig>();
  }
  return builtTask.task.config;
}

RuntimeDatabaseTaskBuilder& RuntimeDatabaseTaskBuilder::setPinning(AppTaskPinning::Variant pinning) {
  builtTask.task.pinning = std::move(pinning);
  return *this;
}

RuntimeDatabaseTaskBuilder& RuntimeDatabaseTaskBuilder::setCallback(AppTaskCallback&& callback) {
  builtTask.task.callback = std::move(callback);
  return *this;
}

RuntimeDatabaseTaskBuilder& RuntimeDatabaseTaskBuilder::setName(std::string_view name) {
  builtTask.data.name = name;
  return *this;
}

RuntimeDatabase& RuntimeDatabaseTaskBuilder::getDatabase() {
  setPinning(AppTaskPinning::Synchronous{});
  return db;
}
