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
        return result != table->rows.end() ? result->second.row : nullptr;
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

    void resize(size_t count) override {
      instance.resize(count);
    }

    void resizeWithIDs(size_t, const StableElementID*) {
      assert(false && "Caller should ensure this is a stable table when using this");
    }

    void swapRemove(const UnpackedDatabaseElementID& id) override {
      instance.modifier.swapRemove(instance.table, id);
    }

    void insert(const UnpackedDatabaseElementID& location, size_t count) override {
      instance.modifier.insert(instance.table, location, count);
    }

    TableModifierInstance instance;
  };

  struct STM : ITableModifier {
    STM(StableTableModifierInstance i)
      : instance{ i } {
    }

    size_t addElements(size_t count) override {
      return instance.addElements(count, nullptr);
    }

    void resize(size_t count) override {
      instance.resize(count, nullptr);
    }

    void resizeWithIDs(size_t count, const StableElementID* reservedIDs) {
      instance.resize(count, reservedIDs);
    }

    void swapRemove(const UnpackedDatabaseElementID& id) override {
      instance.modifier.swapRemove(instance.table, id, *instance.stableMappings);
    }

    void insert(const UnpackedDatabaseElementID& location, size_t count) override {
      instance.modifier.insert(instance.table, location, count, *instance.stableMappings);
    }

    StableTableModifierInstance instance;
  };
}

namespace IDResolverImpl {
  //TODO: is this worth it or should the description be more accessible?
  struct Impl : IIDResolver {
    Impl(RuntimeDatabase& db)
      : description{ db.getDescription() }
      , mappings{ db.getMappings() } {
      auto allTables = db.query<>();
      auto stableTables = db.query<StableIDRow>();
      stableIDs.resize(allTables.matchingTableIDs.size());
      for(size_t i = 0; i < stableTables.size(); ++i) {
        stableIDs[stableTables.matchingTableIDs[i].getTableIndex()] = &stableTables.get<0>(i);
      }
    }

    UnpackedDatabaseElementID uncheckedUnpack(const StableElementID& id) const final {
      return id.toUnpacked(description);
    }

    std::optional<StableElementID> tryResolveStableID(const StableElementID& id) const final {
      //TODO: is this actually faster than doing the map lookup always?
      const UnpackedDatabaseElementID unpacked = id.toUnpacked(description);
      const size_t table = unpacked.getTableIndex();
      const size_t index = unpacked.getElementIndex();
      if(table < stableIDs.size()) {
        if(const StableIDRow* row = stableIDs[table]) {
          if(index < row->size() && row->at(index) == id.mStableID) {
            return id;
          }
        }
      }

      auto it = mappings.findKey(id.mStableID);
      return it.second ? std::make_optional(StableElementID{ it.second->unstableIndex, it.first }) : std::nullopt;
    }

    ElementRef tryResolveRef(const StableElementID& id) const final {
      auto it = mappings.findKey(id.mStableID);
      return { it.second };
    }

    std::optional<ResolvedIDs> tryResolveAndUnpack(const StableElementID& id) const final {
      if(auto resolved = tryResolveStableID(id)) {
        return ResolvedIDs{ *resolved, uncheckedUnpack(*resolved) };
      }
      return {};
    }

    StableElementID createKey() final {
      return StableElementID::fromStableID(mappings.createKey());
    }

    ElementRefResolver getRefResolver() const {
      return { description };
    }

    DatabaseDescription description;
    std::vector<const StableIDRow*> stableIDs;
    StableElementMappings& mappings;
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
          return table->stableModifier.addElements(count, nullptr);
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

void RuntimeDatabaseTaskBuilder::log(const std::vector<UnpackedDatabaseElementID>& tableIds, const TypeIDT& id, bool isConst) {
  for(const UnpackedDatabaseElementID& table : tableIds) {
    if(isConst) {
      logRead(table, id);
    }
    else {
      logWrite(table, id);
    }
  }
}

void RuntimeDatabaseTaskBuilder::log(const QueryAliasBase& alias, const std::vector<UnpackedDatabaseElementID>& tableIds) {
  log(tableIds, alias.type, alias.isConst);
}

void RuntimeDatabaseTaskBuilder::logRead(const UnpackedDatabaseElementID& table, TypeIDT t) {
  builtTask.data.reads.push_back({ t, table });
}

void RuntimeDatabaseTaskBuilder::logWrite(const UnpackedDatabaseElementID& table, TypeIDT t) {
  builtTask.data.writes.push_back({ t, table });
}

void RuntimeDatabaseTaskBuilder::logTableModifier(const UnpackedDatabaseElementID& id) {
  builtTask.data.tableModifiers.push_back(id);
}

std::shared_ptr<ITableModifier> RuntimeDatabaseTaskBuilder::getModifierForTable(const UnpackedDatabaseElementID& table) {
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

std::vector<std::shared_ptr<ITableModifier>> RuntimeDatabaseTaskBuilder::getModifiersForTables(const std::vector<UnpackedDatabaseElementID>& tables) {
  std::vector<std::shared_ptr<ITableModifier>> result(tables.size());
  for(size_t i = 0; i < result.size(); ++i) {
    result[i] = getModifierForTable(tables[i]);
  }
  return result;
}

std::shared_ptr<IAnyTableModifier> RuntimeDatabaseTaskBuilder::getAnyModifier() {
  QueryResult<> q = db.query();
  for(auto&& t : q.matchingTableIDs) {
    logTableModifier(t);
  }
  return std::make_unique<AnyTableModifier::ATM>(db);
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
