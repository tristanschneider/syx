#pragma once

#include "AppBuilder.h"
#include "GameScheduler.h"
#include "Scheduler.h"

namespace Test {
  struct TestApp {
    using WorkBuilder = std::function<void(IAppBuilder&)>;
    using DBBuilder = std::function<void(RuntimeDatabaseArgs&)>;

    template<class DBT>
    void initMTFromDB(const WorkBuilder& workBuilder) {
      initMT([](RuntimeDatabaseArgs& args) { return DBReflect::addDatabase<DBT>(args); }, workBuilder);
    }

    template<class DBT>
    void initSTFromDB(const WorkBuilder& workBuilder) {
      initST([](RuntimeDatabaseArgs& args) { return DBReflect::addDatabase<DBT>(args); }, workBuilder);
    }

    //Multi-threaded or single-threaded
    void initMT(const DBBuilder& buildDB, const WorkBuilder& buildWork);
    void initST(const DBBuilder& buildDB, const WorkBuilder& buildWork);
    RuntimeDatabaseTaskBuilder& builder();
    void update();
    ElementRef createInTable(const TableID& table);

    std::unique_ptr<IDatabase> db;
    std::unique_ptr<RuntimeDatabaseTaskBuilder> taskBuilder;
    std::vector<GameScheduler::SyncWorkItem> workST;
    TaskRange workMT;
  };
}