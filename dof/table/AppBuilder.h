#pragma once

#include "Database.h"
#include "CachedRow.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include "TypeId.h"
#include "RuntimeDatabase.h"
#include "StableElementID.h"
#include "IRow.h"
#include "ITaskImpl.h"

#include <variant>

class ITableModifier {
public:
  virtual ~ITableModifier() = default;
  virtual size_t addElements(size_t count) = 0;
  virtual void resize(size_t count) = 0;
  //Resize to count and use provided IDs for new elements created. Only has meaning in stable tables
  virtual void resizeWithIDs(size_t count, const ElementRef* reservedIDs) = 0;
  virtual void swapRemove(const UnpackedDatabaseElementID& id) = 0;
};

class ElementRefResolver {
public:
  ElementRefResolver(const DatabaseDescription& desc)
    : description{ desc }
  {}

  UnpackedDatabaseElementID uncheckedUnpack(const ElementRef& r) const {
    return UnpackedDatabaseElementID{ *r.tryGet() };
  }

  std::optional<UnpackedDatabaseElementID> tryUnpack(const ElementRef& r) const {
    const StableElementMapping* result = r.tryGet();
    //Invalid is the case when a stable mapping has been reserved but not assigned yet, like in a local database
    return result && result->isValid() && result->getDatabaseIndex() == description.dbIndex ?
      std::make_optional(UnpackedDatabaseElementID{ *result }) :
      std::nullopt;
  }

  //Equivalent to tryUnpack but less annoying to use. Would be ideal to only use this
  UnpackedDatabaseElementID unpack(const ElementRef& r) const {
    return tryUnpack(r).value_or(UnpackedDatabaseElementID{});
  }

private:
  DatabaseDescription description{};
};

class IIDResolver {
public :
  virtual ~IIDResolver() = default;
  virtual ElementRef createKey() = 0;
  virtual ElementRefResolver getRefResolver() const = 0;
  virtual size_t getTotalIds() const = 0;
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
  auto tryGetOrSwapRowElement(CachedRow<Row>& row, const UnpackedDatabaseElementID& id) {
    decltype(&row->at(0)) result{};
    if(!id) {
      return result;
    }
    if(!row.row || row.tableID != id.getTableIndex()) {
      row.row = tryGetRow<Row>(id);
      row.tableID = id.getTableIndex();
    }

    if(row) {
      result = &row->at(id.getElementIndex());
    }
    return result;
  }

  template<class Row>
  auto tryGetOrSwapRowElement(CachedRow<Row>& row, const std::optional<UnpackedDatabaseElementID>& id) {
    decltype(&row->at(0)) result{};
    return id ? tryGetOrSwapRowElement(row, *id) : result;
  }

  template<class StorageRow, std::convertible_to<StorageRow> QueryRow>
  bool tryGetOrSwapRowAlias(const QueryAlias<QueryRow>& alias, CachedRow<StorageRow>& row, const UnpackedDatabaseElementID& id) {
    if(!row.row || row.tableID != id.getTableIndex()) {
      row.row = tryGetRowAlias(alias, id);
      row.tableID = id.getTableIndex();
      return row.row != nullptr;
    }
    return row.row != nullptr;
  }

  template<class Row>
  auto tryGetOrSwapRowAliasElement(const QueryAlias<Row>& alias, CachedRow<Row>& row, const UnpackedDatabaseElementID& id) {
    if(!row.row || row.tableID != id.getTableIndex()) {
      row.row = tryGetRowAlias(alias, id);
      row.tableID = id.getTableIndex();
    }
    [[maybe_unused]] const auto original = row;
    decltype(&row->at(0)) result{};
    if(row) {
      result = &row->at(id.getElementIndex());
    }
    assert(row.tableID == original.tableID && row.row == original.row && "ID Changed presumably due to race condition");
    return result;
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
  virtual IRow* tryGetRow(const UnpackedDatabaseElementID id, IDT type) = 0;
};

struct AppTaskSize {
  size_t workItemCount{};
  size_t batchSize{};
};

//This is created at configuration time to be used to change configurations at runtime
struct AppTaskConfig {
  //This can be used at runtime for tasks to set the sizes of upcoming other tasks
  //It is set by the builder implementation, not the users of the builders that add tasks
  std::function<void(const AppTaskSize&)> setSize;
};

namespace Tasks {
  struct ILocalScheduler;
};

struct IRandom;

struct AppTaskArgs {
  virtual ~AppTaskArgs() = default;

  size_t begin{};
  size_t end{};
  size_t threadIndex{};

  virtual Tasks::ILocalScheduler* getScheduler() = 0;
  //Thread-local database. Changes made here are migrated to the main database at the end of the frame
  virtual RuntimeDatabase& getLocalDB() = 0;
  virtual std::unique_ptr<AppTaskArgs> clone() const = 0;
  virtual IRandom* getRandom() = 0;
};
using AppTaskCallback = std::function<void(AppTaskArgs&)>;

//Information needed to execute the task
struct AppTask {
  AppTaskCallback callback;
  std::shared_ptr<AppTaskConfig> config;
  AppTaskPinning::Variant pinning;
};

struct AppTaskNode {
  std::unique_ptr<ITaskImpl> task;
  std::string_view name;
  std::vector<std::shared_ptr<AppTaskNode>> children;
};

struct TableAccess {
  auto tie() const {
    return std::tie(rowType, tableID);
  }
  bool operator==(const TableAccess& t) const {
    return tie() == t.tie();
  }
  bool operator<(const TableAccess& t) const {
    if(rowType == t.rowType) {
      return tableID < t.tableID;
    }
    return rowType < t.rowType;
  }
  using TypeIDT = DBTypeID;
  TypeIDT rowType;
  TableID tableID;
};

//Information about the data dependencies of the task
struct AppTaskMetadata {
  using TypeIDT = DBTypeID;

  void append(const AppTaskMetadata& toAdd) {
    reads.insert(reads.end(), toAdd.reads.begin(), toAdd.reads.end());
    writes.insert(writes.end(), toAdd.writes.begin(), toAdd.writes.end());
    tableModifiers.insert(tableModifiers.end(), toAdd.tableModifiers.begin(), toAdd.tableModifiers.end());
  }

  //Reads and writes to data in rows, no addition or removal
  std::vector<TableAccess> reads;
  std::vector<TableAccess> writes;
  //Addition and removal to particular tables
  std::vector<TableID> tableModifiers;
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
  ElementRefResolver getRefResolver();

  template<class... Rows>
  std::shared_ptr<ITableResolver> getResolver() {
    //Resolvers don't require all rows to match at once so any tables with any of the rows must be logged
    (log<Rows>(), ...);
    return getResolver();
  }

  template<class... CachedRows>
  std::shared_ptr<ITableResolver> getResolver(const CachedRows&... cachedRows) {
    (log(cachedRows), ...);
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
    (log<Rows>(result.getMatchingTableIDs()), ...);
    return result;
  }

  template<class... Rows>
  QueryResult<Rows...> query(const TableID& table) {
    QueryResult<Rows...> result = db.query<Rows...>(table);
    if(result.size()) {
      const std::vector<TableID> t{ table };
      (log<Rows>(t), ...);
    }
    return result;
  }

  //Allows having a Query as a member of a class and initialize it with a RDTB
  template<class... Rows>
  operator QueryResult<Rows...>() {
    return query<Rows...>();
  }

  template<class... Aliases>
  auto queryAlias(const Aliases&... aliases) {
    QueryResult<typename Aliases::RowT...> result = db.queryAlias(aliases...);
    (log(aliases, result.getMatchingTableIDs()), ...);
    return result;
  }

  template<class... Aliases>
  auto queryAlias(const TableID& table, const Aliases&... aliases) {
    QueryResult<typename Aliases::RowT...> result = db.queryAlias(table, aliases...);
    if(result.size()) {
      const std::vector<TableID> t{ table };
      (log(aliases, t), ...);
    }
    return result;
  }

  //Query table ids without the actual data. Does not log the dependency
  template<class... Rows>
  QueryResultBase queryTables() {
    return db.query<Rows...>();
  }

  template<class... Aliases>
  QueryResultBase queryAliasTables(const Aliases&... aliases) {
    return db.queryAliasTables({ aliases... });
  }

  //Modifiers allow immediately adding or removing elements from tables.
  //Deferred alternatives exist:
  //Add: add to the local database on AppTaskArgs and it'll be migrated to the main database by CommonTasks. This will also emit the migration as a creation event.
  //Move: use Events::EventsRow to emit an event from one table to another. Events module will then perform this move.
  //Delete: use Events::EventsRow to mark for deletion. The deletion will be performed by events module.
  std::shared_ptr<ITableModifier> getModifierForTable(const TableID& table);
  std::vector<std::shared_ptr<ITableModifier>> getModifiersForTables(const std::vector<TableID>& tables);
  std::vector<std::shared_ptr<ITableModifier>> getModifiersForTables(const QueryResultBase& tables);
  std::shared_ptr<AppTaskConfig> getConfig();

  RuntimeDatabaseTaskBuilder& setPinning(AppTaskPinning::Variant pinning);
  RuntimeDatabaseTaskBuilder& setCallback(AppTaskCallback&& callback);
  RuntimeDatabaseTaskBuilder& setName(std::string_view name);

  //Get the entire database, which turns this into a synchronous task since it could do anything
  RuntimeDatabase& getDatabase();

  AppTaskWithMetadata finalize()&&;
  void discard();

  void logDependency(std::initializer_list<QueryAliasBase> aliases);

private:
  template<class T>
  void log() {
    log<T>(db.query<std::decay_t<T>>().getMatchingTableIDs());
  }

  template<class T>
  void log(const CachedRow<T>&) {
    log<T>(db.query<std::decay_t<T>>().getMatchingTableIDs());
  }

  template<class T>
  void log(const std::vector<TableID>& tableIds) {
    using DT = std::decay_t<T>;
    log(tableIds, TypeIDT::get<DT>(), std::is_const_v<T>);
  }

  void log(const std::vector<TableID>& tableIds, const TypeIDT& id, bool isConst);

  template<class Alias>
  void log(const Alias& alias) {
    log(alias, db.queryAlias(alias).getMatchingTableIDs());
  }

  void log(const QueryAliasBase& alias, const std::vector<TableID>& tableIds);

  std::shared_ptr<ITableResolver> getResolver();

  void logRead(const TableID& table, TypeIDT t);
  void logWrite(const TableID& table, TypeIDT t);
  void logTableModifier(const TableID& id);

  RuntimeDatabase& db;
  AppTaskWithMetadata builtTask;
  bool submitted{};
};

enum class AppEnvType : uint8_t {
  InitScheduler,
  InitMain,
  InitThreadLocal,
  UpdateMain,
};
struct AppEnvironment {
  bool isThreadLocal() const {
    return type == AppEnvType::InitThreadLocal;
  }

  AppEnvType type;
  uint8_t threadCount{};
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
  virtual void submitTask(std::unique_ptr<ITaskImpl> impl) = 0;
  virtual std::shared_ptr<AppTaskNode> finalize()&& = 0;
  virtual const AppEnvironment& getEnv() const = 0;

  static std::shared_ptr<AppTaskNode> finalize(std::unique_ptr<IAppBuilder> builder) {
    return std::move(*builder).finalize();
  }

  //Shorthand to get table ids to use in tasks that want to only access one table at a time
  template<class... Aliases>
  QueryResultBase queryAliasTables(const Aliases&... aliases) {
    auto temp = createTask();
    return temp.discard(), temp.queryAliasTables(aliases...);
  }

  template<class... Rows>
  QueryResultBase queryTables() {
    auto temp = createTask();
    return temp.discard(), temp.queryTables<Rows...>();
  }

  //Query if the table has the row
  template<class... Rows>
  bool queryTable(const TableID& id) {
    return queryTables<Rows...>().contains(id);
  }
};