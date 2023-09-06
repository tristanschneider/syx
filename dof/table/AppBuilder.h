#pragma once

#include "Database.h"
#include "TableOperations.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include "TypeId.h"
#include "RuntimeDatabase.h"

class ITableModifier {
public:
  virtual ~ITableModifier() = default;
  virtual size_t addElements(size_t count) = 0;
  //TODO: delete?
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
    return static_cast<Row*>(tryGetRow(id, IDT::get<Row>()));
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

  template<class... Args>
  bool tryGetOrSwapAllRows(const UnpackedDatabaseElementID& id, Args&... rows) {
    return (tryGetOrSwapRow(rows, id) && ...);
  }

  template<class... Args>
  bool tryGetOrSwapAnyRows(const UnpackedDatabaseElementID& id, Args&... rows) {
    return (tryGetOrSwapRow(rows, id) || ...);
  }

private:
  virtual void* tryGetRow(const UnpackedDatabaseElementID id, IDT type) = 0;
};

struct AppTaskConfig {
  size_t workItemCount{};
  size_t batchSize{};
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
};

struct AppTaskNode {
  AppTask task;
  std::vector<std::shared_ptr<AppTaskNode>> children;
};

//Information about the data dependencies of the task
struct AppTaskMetadata {
  using TypeIDT = DBTypeID;

  //Reads and writes to data in rows, no addition or removal
  std::vector<TypeIDT> reads;
  std::vector<TypeIDT> writes;
  //Addition and removal to particular tables
  std::vector<UnpackedDatabaseElementID> tableModifiers;
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

  template<class... Rows>
  std::unique_ptr<ITableResolver> getResolver() {
    (log<Rows>(), ...);
    return TableResolverImpl::create(getDB());
  }

  template<class... Rows>
  QueryResult<Rows...> query() {
    (log<Rows>(), ...);
    getDB().query<std::decay_t<Rows>...>();
  }

  std::unique_ptr<ITableModifier> getModifierForTable(const UnpackedDatabaseElementID& table);
  std::unique_ptr<IAnyTableModifier> getAnyModifier();

  AppTaskWithMetadata finalize()&&;

private:
  template<class T>
  void log() {
    [[maybe_unused]] const auto id = TypeIDT::get<std::decay_t<T>>();
    if constexpr(std::is_const_v<T>) {
      logRead(id);
    }
    else {
      logWrite(id);
    }
  }

  std::unique_ptr<ITableResolver> getResolver();

  void logRead(TypeIDT t);
  void logWrite(TypeIDT t);
  void logTableModifier(const UnpackedDatabaseElementID& id);

  RuntimeDatabase& db;
  AppTaskWithMetadata builtTask;
};

//Top level object used to create all work items in the app
class IAppBuilder {
public:
  virtual ~IAppBuilder() = default;
  //Every work item starts by creating a task, getting the required data accessors, then submitting the task
  virtual RuntimeDatabaseTaskBuilder createTask() = 0;
  virtual void submitTask(AppTaskWithMetadata&& task) = 0;
  virtual std::shared_ptr<AppTaskNode> finalize()&& = 0;
};