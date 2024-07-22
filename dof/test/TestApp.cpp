#include "Precompile.h"
#include "TestApp.h"

#include "GameBuilder.h"
#include "Simulation.h"

namespace Test {
  void TestApp::initST(const DBBuilder& buildDB, const WorkBuilder& buildWork) {
    auto mappings = std::make_unique<StableElementMappings>();
    db = buildDB(*mappings);
    db = DBReflect::bundle(std::move(db), std::move(mappings));
    auto builder = GameBuilder::create(*db);
    auto temp = builder->createTask();
    temp.discard();
    taskBuilder = std::make_unique<RuntimeDatabaseTaskBuilder>(std::move(temp));
    taskBuilder->discard();

    buildWork(*builder);

    workST = GameScheduler::buildSync(IAppBuilder::finalize(std::move(builder)));
  }

  void TestApp::initMT(const DBBuilder& buildDB, const WorkBuilder& buildWork) {
    auto mappings = std::make_unique<StableElementMappings>();
    db = buildDB(*mappings);
    if(!db->getRuntime().query<ThreadLocalsRow>().size()) {
      auto temp = DBReflect::createDatabase<Database<Table<ThreadLocalsRow, SharedRow<Scheduler>, Events::EventsRow>>>(*mappings);
      db = DBReflect::merge(std::move(db), std::move(temp));
    }
    db = DBReflect::bundle(std::move(db), std::move(mappings));
    auto builder = GameBuilder::create(*db);
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

  UnpackedDatabaseElementID TestApp::createInTable(const TableID& table) {
    return builder().getIDResolver()->getRefResolver().uncheckedUnpack(builder().query<StableIDRow>(table).get<0>(0).at(builder().getModifierForTable(table)->addElements(1));
  }
}