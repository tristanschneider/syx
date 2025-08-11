#include "Precompile.h"
#include "GameplayExtract.h"

#include "CommonTasks.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "stat/AllStatEffects.h"
#include "stat/VelocityStatEffect.h"
#include "AppBuilder.h"

namespace GameplayExtract {
  using namespace Tags;

  template<class Src, class Dst>
  void tryExtract(IAppBuilder& builder) {
    auto temp = builder.createTask();
    QueryResult<const Src, Dst> tables = temp.query<const Src, Dst>();

    for(const TableID id : tables) {
      CommonTasks::copyRowSameSize<Src, Dst>(builder, id, id);
    }

    temp.discard();
  }

  template<class Tag, class GTag, class SubTag>
  void tryGExtract(IAppBuilder& builder) {
    tryExtract<FloatRow<Tag, SubTag>, FloatRow<GTag, SubTag>>(builder);
  }

  void extractGameplayData(IAppBuilder& builder) {
    tryGExtract<LinVel, GLinVel, X>(builder);
    tryGExtract<LinVel, GLinVel, Y>(builder);
    tryGExtract<LinVel, GLinVel, Z>(builder);
    tryGExtract<AngVel, GAngVel, Angle>(builder);
  }

  void applyGameplayImpulses(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("applyGameplayImpulses");
    auto query = task.query<FloatRow<GLinImpulse, X>,
      FloatRow<GLinImpulse, Y>,
      FloatRow<GAngImpulse, Angle>,
      const StableIDRow>();
    //Z goes through a resolver because it is optional
    auto resolver = task.getResolver<FloatRow<GLinImpulse, Z>>();
    task.setCallback([query, resolver](AppTaskArgs& args) mutable {
      for(size_t i = 0; i < query.size(); ++i) {
        auto [ x, y, a, stable ] = query.get(i);
        CachedRow<FloatRow<GLinImpulse, Z>> impulseZ;
        resolver->tryGetOrSwapRow(impulseZ, query[i]);

        for(size_t j = 0; j < x->size(); ++j) {
          const glm::vec2 linear{ x->at(j), y->at(j) };
          const float angular = a->at(j);
          const float linearZ = impulseZ ? impulseZ->at(j) : 0.0f;

          //If there is any linear or angular element, turn it into a velocity command then clear out the request
          if(linear.x || linear.y || angular || linearZ) {
            VelocityStatEffect::Builder vb{ args };
            vb.createStatEffects(1).setLifetime(StatEffect::INSTANT).setOwner(stable->at(j));
            vb.addImpulse({ linear, angular, linearZ });
          }
          x->at(j) = y->at(j) = a->at(j) = 0.0f;
          if(impulseZ) {
            impulseZ->at(j) = 0.0f;
          }
        }
      }
    });
    builder.submitTask(std::move(task));
  }
}