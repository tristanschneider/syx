#include "Precompile.h"
#include "TestApp.h"

#include "GameBuilder.h"
#include "Simulation.h"

namespace Test {
  void TestApp::initST(const DBBuilder& buildDB, const WorkBuilder& buildWork) {
    RuntimeDatabaseArgs args = DBReflect::createArgsWithMappings();
    buildDB(args);
    db = std::make_unique<RuntimeDatabase>(std::move(args));
    auto builder = GameBuilder::create(*db, { AppEnvType::UpdateMain });
    auto temp = builder->createTask();
    temp.discard();
    taskBuilder = std::make_unique<RuntimeDatabaseTaskBuilder>(std::move(temp));
    taskBuilder->discard();

    buildWork(*builder);

    workST = GameScheduler::buildSync(IAppBuilder::finalize(std::move(builder)));
  }

  void TestApp::initMT(const DBBuilder& buildDB, const WorkBuilder& buildWork) {
    RuntimeDatabaseArgs args = DBReflect::createArgsWithMappings();
    buildDB(args);

    bool hasThreadLocals = false;
    for(const RuntimeTableRowBuilder& table : args.tables) {
      if(table.contains<ThreadLocalsRow>()) {
        hasThreadLocals = true;
        break;
      }
    }

    if(!hasThreadLocals) {
      DBReflect::addDatabase<Database<Table<ThreadLocalsRow, SharedRow<Scheduler>, Events::EventsRow>>>(args);
    }
    db = std::make_unique<RuntimeDatabase>(std::move(args));

    auto builder = GameBuilder::create(*db, { AppEnvType::UpdateMain });
    auto temp = builder->createTask();
    temp.discard();
    taskBuilder = std::make_unique<RuntimeDatabaseTaskBuilder>(std::move(temp));
    taskBuilder->discard();

    std::unique_ptr<IAppBuilder> bootstrap = GameBuilder::create(*db);
    Simulation::initScheduler(*bootstrap);
    for(auto&& work : GameScheduler::buildSync(IAppBuilder::finalize(std::move(bootstrap)))) {
      work.work();
    }

    buildWork(*builder);

    ThreadLocalsInstance* tls = taskBuilder->query<ThreadLocalsRow>().tryGetSingletonElement();
    workMT = GameScheduler::buildTasks(IAppBuilder::finalize(std::move(builder)), *tls->instance);
  }

  RuntimeDatabaseTaskBuilder& TestApp::builder() {
    return *taskBuilder;
  }

  void TestApp::update() {
    for(auto&& w : workST) {
      w.work();
    }
    if(workMT) {
      Scheduler* scheduler = builder().query<SharedRow<Scheduler>>().tryGetSingletonElement();
      workMT.mBegin->mTask.addToPipe(scheduler->mScheduler);
      scheduler->mScheduler.WaitforTask(workMT.mEnd->mTask.get());
    }
  }

  ElementRef TestApp::createInTable(const TableID& table) {
    return builder().query<StableIDRow>(table).get<0>(0).at(builder().getModifierForTable(table)->addElements(1));
  }
}