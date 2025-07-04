#include "Precompile.h"
#include "RespawnArea.h"

#include "AppBuilder.h"
#include "IAppModule.h"
#include "RuntimeDatabase.h"
#include "RowTags.h"
#include "TableName.h"
#include "Narrowphase.h"
#include "TLSTaskImpl.h"
#include "stat/VelocityStatEffect.h"
#include "stat/PositionStatEffect.h"
#include "stat/FragmentBurstStatEffect.h"
#include "loader/ReflectionModule.h"

namespace RespawnArea {
  struct RespawnTagRow : TagRow {};
  struct RespawnBurstRadius : Row<float> {
    static constexpr std::string_view KEY = "RespawnBurstRadius";
  };

  using RespawnDB = Database<
    Table<
      RespawnTagRow,
      Tags::PosXRow,
      Tags::PosYRow,
      Tags::PosZRow,
      Tags::RotXRow,
      Tags::RotYRow,
      Tags::ScaleXRow,
      Tags::ScaleYRow,
      Narrowphase::ThicknessRow,
      RespawnBurstRadius
    >
  >;

  struct DoRespawn {
    struct TLS {
      TLS(AppTaskArgs& args)
        : velocity{ args }
        , position{ args }
        , burst{ args } {
      }

      VelocityStatEffect::Builder velocity;
      PositionStatEffect::Builder position;
      FragmentBurstStatEffect::Builder burst;
    };

    void init(RuntimeDatabaseTaskBuilder& task) {
      others = task;
      areas = task;
    }

    void init(AppTaskArgs& args) {
      tls.emplace(args);
    }

    void execute() {
      for(size_t t = 0; t < areas.size(); ++t) {
        auto [_, areaZ, areaThickness, burstRadius] = areas.get(t);
        for(size_t i = 0; i < areaZ->size(); ++i) {
          const float zMin = areaZ->at(i);
          const float zMax = zMin + areaThickness->at(i);
          const float radius = burstRadius->at(i);
          checkRespawn(zMin, zMax, radius);
        }
      }
    }

    void checkRespawn(float zMin, float zMax, float burstRadius) {
      for(size_t t = 0; t < others.size(); ++t) {
        auto [posZ, stable] = others.get(t);
        for(size_t i = 0; i < posZ->size(); ++i) {
          if(posZ->at(i) < zMin) {
            const auto* stableID = &stable->at(i);
            tls->position.createStatEffects(1).setLifetime(StatEffect::INSTANT).setOwner(*stableID);
            tls->position.setZ(zMax);
            tls->velocity.createStatEffects(1).setLifetime(StatEffect::INSTANT).setOwner(*stableID);
            tls->velocity.setZ({ 0.0f });
            tls->burst.createStatEffects(1).setLifetime(StatEffect::INFINITE).setOwner(*stableID);
            tls->burst.setRadius(burstRadius);
          }
        }
      }
    }

    QueryResult<const Tags::GPosZRow, const StableIDRow> others;
    QueryResult<
      const RespawnTagRow,
      const Tags::GPosZRow,
      const Narrowphase::ThicknessRow,
      const RespawnBurstRadius
    > areas;
    std::optional<TLS> tls;
  };

  struct RespawnModule : IAppModule {
    void createDatabase(RuntimeDatabaseArgs& args) final {
      DBReflect::addDatabase<RespawnDB>(args);
    }

    void init(IAppBuilder& builder) final {
      Reflection::registerLoaders(builder, Reflection::createDirectRowLoader<Loader::FloatRow, RespawnArea::RespawnBurstRadius>());
      TableName::setName<RespawnTagRow>(builder, { "RespawnArea" });
    }

    void update(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::create<DoRespawn>("DoRespawn"));
    }
  };

  std::unique_ptr<IAppModule> createModule() {
    return std::make_unique<RespawnModule>();
  }
}