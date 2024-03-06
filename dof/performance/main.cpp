#include "Precompile.h"

#include <cstdio>
#include "Simulation.h"
#include "PerformanceDB.h"
#include "GameBuilder.h"
#include "GameScheduler.h"
#include "AppBuilder.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"

namespace Performance {
  struct App {
    std::unique_ptr<IDatabase> db;
    std::unique_ptr<IAppBuilder> builder;
    Scheduler* scheduler{};
    TaskRange update;
    AppTaskArgs args;
    std::unique_ptr<ThreadLocalData> data = std::make_unique<ThreadLocalData>();
  };

  App createApp() {
    Performance::App app;
    app.db = PerformanceDB::create();
    app.builder = GameBuilder::create(*app.db);

    //First initialize just the scheduler synchronously
    {
      std::unique_ptr<IAppBuilder> bootstrap = GameBuilder::create(*app.db);
      Simulation::initScheduler(*bootstrap);
      for(auto&& work : GameScheduler::buildSync(IAppBuilder::finalize(std::move(bootstrap)))) {
        work.work();
      }
    }
    auto temp = app.builder->createTask();
    temp.discard();
    ThreadLocalsInstance* tls = temp.query<ThreadLocalsRow>().tryGetSingletonElement();
    Scheduler* scheduler = temp.query<SharedRow<Scheduler>>().tryGetSingletonElement();
    app.scheduler = scheduler;

    {
      std::unique_ptr<IAppBuilder> initBuilder = GameBuilder::create(*app.db);
      Simulation::init(*initBuilder);
      TaskRange initTasks = GameScheduler::buildTasks(IAppBuilder::finalize(std::move(initBuilder)), *tls->instance);

      initTasks.mBegin->mTask.addToPipe(scheduler->mScheduler);
      scheduler->mScheduler.WaitforTask(initTasks.mEnd->mTask.get());
    }

    {
      std::unique_ptr<IAppBuilder> builder = GameBuilder::create(*app.db);
      Simulation::buildUpdateTasks(*builder, {});
      std::shared_ptr<AppTaskNode> appTaskNodes = IAppBuilder::finalize(std::move(builder));

      app.update = GameScheduler::buildTasks(std::move(appTaskNodes), *tls->instance);
    }

    *app.data = tls->instance->get(0);
    app.args.threadLocal = app.data.get();
    return app;
  }

  void initStaticScene(IAppBuilder& builder, AppTaskArgs& args) {
    auto task = builder.createTask();
    task.discard();
    auto q = task.query<
      const SharedMassObjectTableTag,
      Tags::PosXRow,
      Tags::PosYRow,
      const StableIDRow
    >();
    auto modifiers = task.getModifiersForTables(q.matchingTableIDs);
    for(size_t t = 0; t < q.size(); ++t) {
      auto [_, px, py, stable] = q.get(t);
      constexpr size_t sx = 100;
      constexpr size_t sy = 100;
      modifiers[t]->resize(sx*sy);
      for(size_t x = 0; x < sx; ++x) {
        for(size_t y = 0; y < sy; ++y) {
          const size_t i = x + sx*y;
          TableAdapters::write(i, glm::vec2{ static_cast<float>(x), static_cast<float>(y) }, *px, *py);
          Events::onNewElement(StableElementID::fromStableRow(i, *stable), args);
        }
      }
    }
  }

  using Duration = size_t;

  Duration update(App& app) {
    using Clock = std::chrono::steady_clock;
    auto before = Clock::now();
    app.update.mBegin->mTask.addToPipe(app.scheduler->mScheduler);
    app.scheduler->mScheduler.WaitforTask(app.update.mEnd->mTask.get());
    auto after = Clock::now();
    return static_cast<Duration>(std::chrono::duration_cast<std::chrono::nanoseconds>(after - before).count());
  }

  struct TimeStats {
    uint64_t total{};
    Duration min = std::numeric_limits<Duration>::max();
    Duration max = std::numeric_limits<Duration>::min();
    Duration average{};
  };

  TimeStats computeStats(const std::vector<Duration>& timings) {
    TimeStats result;
    for(const Duration& d : timings) {
      result.max = std::max(result.max, d);
      result.min = std::min(result.min, d);
      result.total += d;
    }
    result.average = result.total / timings.size();
    return result;
  }
};

int main() {
  using namespace Performance;
  printf("Starting performance test...\n");
  App app = createApp();

  initStaticScene(*app.builder, app.args);
  constexpr size_t iterations = 1000;
  std::vector<Duration> timings(iterations);

  //Initial update is way more expensive so track it separately so it doesn't throw off the average
  const Duration firstUpdate = update(app);

  for(size_t i = 0; i < 1000; ++i) {
    timings[i] = update(app);
    printf("%s\n", std::to_string(timings[i]).c_str());
  }

  const auto stats = computeStats(timings);
  printf("Results\n"
    "Start %s\n"
    "Total %s\n"
    "Min %s\n"
    "Max %s\n"
    "Avg %s\n",
    std::to_string(firstUpdate).c_str(),
    std::to_string(stats.total).c_str(),
    std::to_string(stats.min).c_str(),
    std::to_string(stats.max).c_str(),
    std::to_string(stats.average).c_str()
  );
  return 0;
}