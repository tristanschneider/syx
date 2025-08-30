#include "Precompile.h"

#include <cstdio>
#include "Events.h"
#include "Simulation.h"
#include "GameBuilder.h"
#include "GameScheduler.h"
#include "AppBuilder.h"
#include "TableAdapters.h"
#include "ThreadLocals.h"
#include "Profile.h"
#include "Game.h"
#include "IGame.h"
#include "IAppModule.h"
#include <transform/TransformRows.h>

//TODO: this doesn't make much sense anymore
//Better approach is likely to measure performance of particular imported scenes using the default game db
namespace Performance {
  struct App {
    std::unique_ptr<IGame> game;
    std::unique_ptr<IAppBuilder> builder;
    std::unique_ptr<AppTaskArgs> args;
    std::unique_ptr<ThreadLocalData> data = std::make_unique<ThreadLocalData>();
  };

  App createApp() {
    Performance::App app;
    app.game = Game::createGame(GameDefaults::createDefaultGameArgs());

    app.game->init();

    app.builder = GameBuilder::create(app.game->getDatabase(), { AppEnvType::UpdateMain });
    app.args = app.game->createAppTaskArgs();
    return app;
  }

  void initStaticScene(IAppBuilder& builder, AppTaskArgs&) {
    auto task = builder.createTask();
    task.discard();
    auto q = task.query<
      const SharedMassObjectTableTag,
      Transform::WorldTransformRow,
      Events::EventsRow
    >();
    auto modifiers = task.getModifiersForTables(q.getMatchingTableIDs());
    for(size_t t = 0; t < q.size(); ++t) {
      auto [_, transforms, stable] = q.get(t);
      constexpr size_t sx = 100;
      constexpr size_t sy = 100;
      modifiers[t]->resize(sx*sy);
      for(size_t x = 0; x < sx; ++x) {
        for(size_t y = 0; y < sy; ++y) {
          const size_t i = x + sx*y;
          Transform::PackedTransform& transform = transforms->at(i);
          transform.setPos(glm::vec2{ static_cast<float>(x), static_cast<float>(y) });
          stable->getOrAdd(i).setCreate();
        }
      }
    }
  }

  using Duration = size_t;

  Duration update(App& app) {
    using Clock = std::chrono::steady_clock;
    PROFILE_SCOPE("app", "update");
    auto before = Clock::now();
    app.game->updateSimulation();
    auto after = Clock::now();
    PROFILE_UPDATE(nullptr);
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

  initStaticScene(*app.builder, *app.args);
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