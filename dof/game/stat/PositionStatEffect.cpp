#include "Precompile.h"
#include "stat/PositionStatEffect.h"

#include "AllStatEffects.h"
#include "AppBuilder.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include <transform/TransformRows.h>

namespace PositionStatEffect {
  RuntimeTable& getArgs(AppTaskArgs& args) {
    return StatEffectDatabase::getStatTable<PositionStatEffect::CommandRow>(args);
  }

  Builder::Builder(AppTaskArgs& args)
    : BuilderBase{ getArgs(args), args.getLocalDB() }
  {
    command = table.tryGet<CommandRow>();
  }

  Builder& Builder::setZ(float z) {
    for(auto i : currentEffects) {
      command->at(i).posZ = z;
    }
    return *this;
  }

  Builder& Builder::setPos(const glm::vec2& p) {
    for(auto i : currentEffects) {
      command->at(i).pos = p;
    }
    return *this;
  }

  Builder& Builder::setRot(const glm::vec2& r) {
    for(auto i : currentEffects) {
      command->at(i).rot = r;
    }
    return *this;
  }

  void processStat(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("position stat");
    auto query = task.query<
      const CommandRow,
      const StatEffect::Owner
    >();
    auto res = task.getRefResolver();
    using namespace Tags;

    auto resolver = task.getResolver<Transform::WorldTransformRow, Transform::TransformNeedsUpdateRow>();

    task.setCallback([query, res, resolver](AppTaskArgs&) mutable {
      CachedRow<Transform::WorldTransformRow> dst;
      CachedRow<Transform::TransformNeedsUpdateRow> needsUpdate;

      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [commands, owners] = query.get(t);
        for(size_t i = 0; i < commands->size(); ++i) {
          const auto owner = res.unpack(owners->at(i));
          if(!resolver->tryGetOrSwapAllRows(owner, dst, needsUpdate)) {
            continue;
          }
          const size_t si = owner.getElementIndex();

          const PositionStatEffect::PositionCommand& cmd = commands->at(i);
          Transform::PackedTransform& transform = dst->at(si);
          Transform::Parts parts = transform.decompose();
          if(cmd.pos) {
            parts.translate.x = cmd.pos->x;
            parts.translate.y = cmd.pos->y;
          }
          if(cmd.posZ) {
            parts.translate.z = *cmd.posZ;
          }
          if(cmd.rot) {
            parts.rot = parts.rot;
          }
          //Assume a command always changes something
          transform = Transform::PackedTransform::build(parts);
          needsUpdate->getOrAdd(si);
        }
      }
    });

    builder.submitTask(std::move(task));
  }
}