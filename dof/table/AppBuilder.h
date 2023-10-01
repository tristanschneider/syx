#pragma once

#include "Database.h"
#include "TableOperations.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include "TypeId.h"
#include "RuntimeDatabase.h"

#include <variant>

class ITableModifier {
public:
  virtual ~ITableModifier() = default;
  virtual size_t addElements(size_t count) = 0;
  virtual void resize(size_t count) = 0;
  //Resize to count and use provided IDs for new elements created. Only has meaning in stable tables
  virtual void resizeWithIDs(size_t count, const StableElementID* reservedIDs);
  virtual void swapRemove(const UnpackedDatabaseElementID& id) = 0;
  //Insert this many before location
  virtual void insert(const UnpackedDatabaseElementID& location, size_t count) = 0;
};

class IIDResolver {
public :
  virtual ~IIDResolver() = default;
  //Unpack without ensuring it's valid
  virtual UnpackedDatabaseElementID uncheckedUnpack(const StableElementID& id) const = 0;
  virtual std::optional<StableElementID> tryResolveStableID(const StableElementID& id) const = 0;
  virtual StableElementID createKey();
};

class IAnyTableModifier {
public:
  virtual ~IAnyTableModifier() = default;
  virtual size_t addElements(const UnpackedDatabaseElementID& id, size_t count) = 0;
  //TODO: move, delete
};

class ITableResolver {
public:
  using IDT = DBTypeID;

  virtual ~ITableResolver() = default;

  template<class Row>
  Row* tryGetRow(const UnpackedDatabaseElementID& id) {
    return static_cast<Row*>(tryGetRow(id, IDT::get<std::decay_t<Row>>()));
  }

  template<class Row>
  Row* tryGetRowAlias(const QueryAlias<Row>& alias, const UnpackedDatabaseElementID& id) {
    return alias.cast(tryGetRow(id, alias.type));
  }

  template<class Row>
  bool tryGetOrSwapRow(CachedRow<Row>& row, const UnpackedDatabaseElementID& id) {
    if(!row.row || row.tableID != id.getTableIndex()) {
      row.row = tryGetRow<Row>(id);
      row.tableID = id.getTableIndex();
      return row.row != nullptr;
    }
    return row.row != nullptr;
  }

  template<class Row>
  bool tryGetOrSwapRowAlias(const QueryAlias<Row>& alias, CachedRow<Row>& row, const UnpackedDatabaseElementID& id) {
    if(!row.row || row.tableID != id.getTableIndex()) {
      row.row = tryGetRowAlias(alias, id);
      row.tableID = id.getTableIndex();
      return row.row != nullptr;
    }
    return row.row != nullptr;
  }

  template<class... Args>
  bool tryGetOrSwapAllRows(const UnpackedDatabaseElementID& id, Args&... rows) {
    return (tryGetOrSwapRow(rows, id) && ...);
  }

  template<class... Args>
  bool tryGetOrSwapAnyRows(const UnpackedDatabaseElementID& id, Args&... rows) {
    return (tryGetOrSwapRow(rows, id) | ...);
  }

private:
  virtual void* tryGetRow(const UnpackedDatabaseElementID id, IDT type) = 0;
};

struct AppTaskSize {
  size_t workItemCount{};
  size_t batchSize{};
};

namespace AppTaskPinning {
  struct None {};
  struct MainThread {};
  using Variant = std::variant<None, MainThread>;
};

//This is created at configuration time to be used to change configurations at runtime
struct AppTaskConfig {
  //This can be used at runtime for tasks to set the sizes of upcoming other tasks
  //It is set by the builder implementation, not the users of the builders that add tasks
  std::function<void(const AppTaskSize&)> setSize;
};

struct AppTaskArgs {
  size_t begin{};
  size_t end{};
  void* threadLocal{};
};
using AppTaskCallback = std::function<void(AppTaskArgs&)>;

//Information needed to execute the task
struct AppTask {
  AppTaskCallback callback;
  std::shared_ptr<AppTaskConfig> config;
  AppTaskPinning::Variant pinning;
};

struct AppTaskNode {
  AppTask task;
  std::vector<std::shared_ptr<AppTaskNode>> children;
};

struct TableAccess {
  using TypeIDT = DBTypeID;
  TypeIDT rowType;
  UnpackedDatabaseElementID tableID;
};

//Information about the data dependencies of the task
struct AppTaskMetadata {
  using TypeIDT = DBTypeID;

  //Reads and writes to data in rows, no addition or removal
  std::vector<TableAccess> reads;
  std::vector<TableAccess> writes;
  //Addition and removal to particular tables
  std::vector<UnpackedDatabaseElementID> tableModifiers;
  std::string_view name;
};

struct AppTaskWithMetadata {
  AppTask task;
  AppTaskMetadata data;
};

//This is used at configuration time to fetch all data dependences as well as set the callback for the work
//the work callback uses only the dependencies it previously fetched
class RuntimeDatabaseTaskBuilder {
public:
  using TypeIDT = DBTypeID;

  RuntimeDatabaseTaskBuilder(RuntimeDatabase& rdb);
  ~RuntimeDatabaseTaskBuilder();

  std::shared_ptr<IIDResolver> getIDResolver();

  template<class... Rows>
  std::shared_ptr<ITableResolver> getResolver() {
    //Resolvers don't require all rows to match at once so any tables with any of the rows must be logged
    (log<Rows>(), ...);
    return getResolver();
  }

  template<class... Aliases>
  std::shared_ptr<ITableResolver> getAliasResolver(const Aliases&... aliases) {
    (log(aliases), ...);
    return getResolver();
  }

  template<class... Rows>
  QueryResult<Rows...> query() {
    QueryResult<Rows...> result = db.query<Rows...>();
    (log<Rows>(result.matchingTableIDs), ...);
    return result;
  }

  template<class... Rows>
  QueryResult<Rows...> query(const UnpackedDatabaseElementID& table) {
    QueryResult<Rows...> result = db.query<Rows...>(table);
    if(result.size()) {
      const std::vector<UnpackedDatabaseElementID> t{ table };
      (log<Rows>(t), ...);
    }
    return result;
  }

  template<class... Aliases>
  auto queryAlias(const Aliases&... aliases) {
    QueryResult<typename Aliases::RowT...> result = db.queryAlias(aliases...);
    (log(aliases, result.matchingTableIDs), ...);
    return result;
  }

  template<class... Aliases>
  auto queryAlias(const UnpackedDatabaseElementID& table, const Aliases&... aliases) {
    QueryResult<typename Aliases::RowT...> result = db.queryAlias(table, aliases...);
    if(result.size()) {
      const std::vector<UnpackedDatabaseElementID> t{ table };
      (log(aliases, t), ...);
    }
    return result;
  }

  std::shared_ptr<ITableModifier> getModifierForTable(const UnpackedDatabaseElementID& table);
  std::vector<std::shared_ptr<ITableModifier>> getModifiersForTables(const std::vector<UnpackedDatabaseElementID>& tables);
  std::shared_ptr<IAnyTableModifier> getAnyModifier();
  std::shared_ptr<AppTaskConfig> getConfig();

  RuntimeDatabaseTaskBuilder& setPinning(AppTaskPinning::Variant pinning);
  RuntimeDatabaseTaskBuilder& setCallback(AppTaskCallback&& callback);
  RuntimeDatabaseTaskBuilder& setName(std::string_view name);

  AppTaskWithMetadata finalize()&&;
  void discard();

private:
  template<class T>
  void log() {
    log<T>(db.query<std::decay_t<T>>().matchingTableIDs);
  }

  template<class T>
  void log(const std::vector<UnpackedDatabaseElementID>& tableIds) {
    using DT = std::decay_t<T>;
    [[maybe_unused]] const auto id = TypeIDT::get<DT>();
    for(const UnpackedDatabaseElementID& table : tableIds) {
      if constexpr(std::is_const_v<T>) {
        logRead(table, id);
      }
      else {
        logWrite(table, id);
      }
    }
  }

  template<class Alias>
  void log(const Alias& alias) {
    log(alias, db.queryAlias(alias).matchingTableIDs);
  }

  void log(const QueryAliasBase& alias, const std::vector<UnpackedDatabaseElementID>& tableIds);

  std::shared_ptr<ITableResolver> getResolver();

  void logRead(const UnpackedDatabaseElementID& table, TypeIDT t);
  void logWrite(const UnpackedDatabaseElementID& table, TypeIDT t);
  void logTableModifier(const UnpackedDatabaseElementID& id);

  RuntimeDatabase& db;
  AppTaskWithMetadata builtTask;
  bool submitted{};
};

//Top level object used to create all work items in the app
class IAppBuilder {
public:
  virtual ~IAppBuilder() = default;
  //Every work item starts by creating a task, getting the required data accessors, then submitting the task
  virtual RuntimeDatabaseTaskBuilder createTask() = 0;
  void submitTask(RuntimeDatabaseTaskBuilder&& task) {
    submitTask(std::move(task).finalize());
  }
  virtual void submitTask(AppTaskWithMetadata&& task) = 0;
  virtual std::shared_ptr<AppTaskNode> finalize()&& = 0;

  //Shorthand to get table ids to use in tasks that want to only access one table at a time
  template<class... Aliases>
  QueryResult<> queryAliasTables(const Aliases&... aliases) {
    auto temp = createTask();
    QueryResult<> result;
    result.matchingTableIDs = std::move(temp.queryAlias(aliases...).matchingTableIDs);
    temp.discard();
    return result;
  }

  template<class... Rows>
  QueryResult<> queryTables() {
    auto temp = createTask();
    QueryResult<> result;
    result.matchingTableIDs = std::move(temp.query<Rows...>().matchingTableIDs);
    temp.discard();
    return result;
  }
};