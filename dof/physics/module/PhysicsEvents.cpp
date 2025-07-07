#include <Precompile.h>
#include <module/PhysicsEvents.h>

#include <IAppModule.h>
#include <TLSTaskImpl.h>

namespace PhysicsEvents {
  struct ClearTask {
    void init(RuntimeDatabaseTaskBuilder& task) {
      recomputeMass = task;
    }

    void execute() {
      for(size_t t = 0; t < recomputeMass.size(); ++t) {
        auto&& [masses] = recomputeMass.get(t);
        masses->clear();
      }
    }

    QueryResult<RecomputeMassRow> recomputeMass;
  };

  class ClearModule : public IAppModule {
  public:
    void update(IAppBuilder& app) final {
      return app.submitTask(TLSTask::create<ClearTask>("ClearPhysicsEvents"));
    }
  };

  std::unique_ptr<IAppModule> clearEvents() {
    return std::make_unique<ClearModule>();
  }
}