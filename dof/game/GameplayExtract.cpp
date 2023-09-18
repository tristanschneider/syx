#include "Precompile.h"
#include "GameplayExtract.h"

#include "CommonTasks.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "stat/AllStatEffects.h"
#include "AppBuilder.h"

namespace GameplayExtract {
  using namespace Tags;

  template<class Src, class Dst>
  void tryExtract(IAppBuilder& builder) {
    auto temp = builder.createTask();
    QueryResult<const Src, Dst> tables = temp.query<const Src, Dst>();

    for(const UnpackedDatabaseElementID& id : tables.matchingTableIDs) {
      CommonTasks::copyRowSameSize<Src, Dst>(builder, id, id);
    }

    temp.discard();
  }

  template<class Tag, class GTag, class SubTag>
  void tryGExtract(IAppBuilder& builder) {
    tryExtract<FloatRow<Tag, SubTag>, FloatRow<GTag, SubTag>>(builder);
  }

  void extractGameplayData(IAppBuilder& builder) {
    tryGExtract<Pos, GPos, X>(builder);
    tryGExtract<Pos, GPos, Y>(builder);
    tryGExtract<Rot, GRot, CosAngle>(builder);
    tryGExtract<Rot, GRot, SinAngle>(builder);
    tryGExtract<LinVel, GLinVel, X>(builder);
    tryGExtract<LinVel, GLinVel, Y>(builder);
    tryGExtract<AngVel, GAngVel, Angle>(builder);
  }

  void applyGameplayImpulses(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("applyGameplayImpulses");
    auto query = task.query<FloatRow<GLinImpulse, X>,
      FloatRow<GLinImpulse, Y>,
      FloatRow<GAngImpulse, Angle>,
      const StableIDRow>();
    task.setCallback([query](AppTaskArgs& args) mutable {
      VelocityStatEffectAdapter v = TableAdapters::getVelocityEffects(args);
      for(size_t i = 0; i < query.size(); ++i) {
        auto [ x, y, a, stable ] = query.get(i);
        for(size_t j = 0; j < x->size(); ++j) {
          const glm::vec2 linear{ x->at(j), y->at(j) };
          const float angular = a->at(j);
          //If there is any linear or angular element, turn it into a velocity command then clear out the request
          if(linear.x || linear.y || a) {
            const size_t id = TableAdapters::addStatEffectsSharedLifetime(v.base, StatEffect::INSTANT, &stable->at(j), 1);
            v.command->at(id) = VelocityStatEffect::VelocityCommand{ linear, angular };
          }
          x->at(j) = y->at(j) = a->at(j) = 0.0f;
        }
      }
    });
    builder.submitTask(std::move(task));
  }
}