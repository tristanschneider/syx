#include "Precompile.h"

#include "GameBuilder.h"
#include "AppBuilder.h"

//Reads need to wait for writes to finish
//Writes need to wait for reads and writes to finish
//Modifications need to wait for everything in that table
namespace GameBuilder {
  using IDT = DBTypeID;

  struct DependencyGroup {
    std::vector<std::shared_ptr<AppTaskNode>> tasks;
  };
  struct TableDependencies {
    UnpackedDatabaseElementID id;
    //All nodes that are reading in parallel since the last write or modify
    std::unordered_map<IDT, DependencyGroup> reads;
    //The last node to write to the given type
    std::unordered_map<IDT, std::shared_ptr<AppTaskNode>> writes;
    //The last node to add or remove elements from the table
    std::shared_ptr<AppTaskNode> modify;
  };

  struct AddChildTask {
    void operator()(std::pair<const IDT, DependencyGroup>& pair) {
      operator()(pair.second);
    }

    void operator()(std::pair<const IDT, std::shared_ptr<AppTaskNode>>& pair) {
      operator()(pair.second);
    }

    void operator()(DependencyGroup& group) {
      std::for_each(group.tasks.begin(), group.tasks.end(), *this);
    }

    void operator()(std::shared_ptr<AppTaskNode>& node) {
      //Avoid duplicates. Since nodes are all added at once duplicates would be at the back
      if(node && (node->children.empty() || node->children.back() != toAdd)) {
        node->children.push_back(toAdd);
      }
    }

    std::shared_ptr<AppTaskNode> toAdd;
  };

  void addTableModifier(TableDependencies& table, std::shared_ptr<AppTaskNode> node) {
    //All tasks must finish before the modifier runs
    AddChildTask addChild{ node };
    std::for_each(table.reads.begin(), table.reads.end(), addChild);
    std::for_each(table.writes.begin(), table.writes.end(), addChild);
    addChild(table.modify);

    table.modify = node;

    //All further tasks will depend on this so nothing else needs to be tracked
    table.reads.clear();
    table.writes.clear();
  }

  void addTableWrite(TableDependencies& table, IDT type, std::shared_ptr<AppTaskNode> node) {
    AddChildTask addChild{ node };
    auto& read = table.reads[type];
    auto& write = table.writes[type];
    //All reads or writes to this type must finish, along with any synchronous operation
    addChild(read);
    addChild(write);
    addChild(table.modify);

    //Writes depend on the previous, so only the latest needs to be tracked
    write = node;

    //All subsequent reads must depend on this and this depends on all reads so they don't need to be tracked anymore
    read.tasks.clear();
  }

  void addTableRead(TableDependencies& table, IDT type, std::shared_ptr<AppTaskNode> node) {
    AddChildTask addChild{ node };
    //Reads can be in parallel with other reads and must be after writes or modifies
    addChild(table.writes[type]);
    addChild(table.modify);

    table.reads[type].tasks.push_back(node);
  }

  struct Impl : IAppBuilder {

    Impl(IDatabase& d, AppEnvironment e)
      : db{ d }
      , env{ e } 
    {
      std::vector<TableID> tableids = std::move(db.getRuntime().query().getMatchingTableIDs());
      dependencies.resize(tableids.size());
      root->name = "root";
      //Add root as synchronous base to everything
      for(size_t i = 0; i < tableids.size(); ++i) {
        auto& table = dependencies[i];
        table.modify = root;
        table.id = tableids[i];
      }
      assert(tableids.size());
      struct NoOp{};
      noOpAccess = TableAccess{ DBTypeID::get<NoOp>(), tableids[0] };
    }

    RuntimeDatabaseTaskBuilder createTask() override {
      return { db.getRuntime() };
    }

    struct TableMatches {
      bool operator()(const TableAccess& access) const {
        return access.tableID == id;
      }

      const TableID& id;
    };

    template<class T>
    static void eraseDuplicates(T& container) {
      std::sort(container.begin(), container.end());
      container.erase(std::unique(container.begin(), container.end()), container.end());
    }

    //Reduce access types to their least restricting forms. So read+write becomes write, write+modify becomes modify
    static void reduce(AppTaskMetadata& data) {
      eraseDuplicates(data.tableModifiers);

      for(const TableID& modifiedTable : data.tableModifiers) {
        data.writes.erase(std::remove_if(data.writes.begin(), data.writes.end(), TableMatches{ modifiedTable }), data.writes.end());
        data.reads.erase(std::remove_if(data.reads.begin(), data.reads.end(), TableMatches{ modifiedTable }), data.reads.end());
      }

      eraseDuplicates(data.writes);
      for(const TableAccess& writeTable : data.writes) {
        data.reads.erase(std::remove(data.reads.begin(), data.reads.end(), writeTable), data.reads.end());
      }

      eraseDuplicates(data.reads);
    }

    struct TaskWrapper : ITaskImpl {
      TaskWrapper(AppTaskWithMetadata&& w)
        : wrappedTask{ std::move(w) }
      {
      }

      void setWorkerCount(size_t) final {}

      AppTaskMetadata init(RuntimeDatabase&) final {
        return std::move(wrappedTask.data);
      }

      void initThreadLocal(AppTaskArgs&) final {}
      void execute(AppTaskArgs& args) final {
        wrappedTask.task.callback(args);
      }

      std::shared_ptr<AppTaskConfig> getConfig() final {
        return wrappedTask.task.config;
      }

      AppTaskPinning::Variant getPinning() final {
        return wrappedTask.task.pinning;
      }

      AppTaskWithMetadata wrappedTask;
    };

    void submitTask(AppTaskWithMetadata&& task) override {
      submitTask(std::make_unique<TaskWrapper>(std::move(task)));
    }

    void submitTask(std::unique_ptr<ITaskImpl> impl) final {
      impl->setWorkerCount(getEnv().threadCount);
      AppTaskMetadata meta = impl->init(db.getRuntime());
      const AppTaskPinning::Variant pinning = impl->getPinning();

      auto node = std::make_shared<AppTaskNode>();

      node->task = std::move(impl);
      assert(!meta.name.empty() && "Name please");
      node->name = meta.name;

      reduce(meta);
      //If everything is empty add an artificial dependency on an empty type so it still gets scheduled somewhere
      if(meta.reads.empty() && meta.writes.empty() && meta.tableModifiers.empty()) {
        meta.reads.push_back(noOpAccess);
      }
      if(std::get_if<AppTaskPinning::Synchronous>(&pinning)) {
        for(TableDependencies& table : dependencies) {
          addTableModifier(table, node);
        }
      }
      else {
        for(const UnpackedDatabaseElementID& modifiedTable : meta.tableModifiers) {
          addTableModifier(dependencies[modifiedTable.getTableIndex()], node);
        }
        for(const TableAccess& write : meta.writes) {
          addTableWrite(dependencies[write.tableID.getTableIndex()], write.rowType, node);
        }
        for(const TableAccess& read : meta.reads) {
          addTableRead(dependencies[read.tableID.getTableIndex()], read.rowType, node);
        }
      }
    }

    std::shared_ptr<AppTaskNode> finalize()&& override {
      auto finalSync = std::make_shared<AppTaskNode>();
      finalSync->name = "end";
      for(TableDependencies& table : dependencies) {
        addTableModifier(table, finalSync);
      }

      return std::move(root);
    }

    const AppEnvironment& getEnv() const final {
      return env;
    }

    IDatabase& db;
    std::shared_ptr<AppTaskNode> root = std::make_shared<AppTaskNode>();
    std::vector<TableDependencies> dependencies;
    TableAccess noOpAccess;
    AppEnvironment env;
  };

  std::unique_ptr<IAppBuilder> create(IDatabase& db, AppEnvironment env) {
    return std::make_unique<Impl>(db, env);
  }
}