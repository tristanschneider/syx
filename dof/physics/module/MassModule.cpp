#include <module/MassModule.h>

#include <IAppModule.h>
#include <Mass.h>
#include <module/PhysicsEvents.h>
#include <shapes/ShapeRegistry.h>
#include <TLSTaskImpl.h>
#include <Events.h>

namespace MassModule {
  struct FlagNewElements {
    void init(RuntimeDatabaseTaskBuilder& task) {
      query = task;
    }

    void execute() {
      for(size_t t = 0; t < query.size(); ++t) {
        //Flag all newly created elements for a mass update
        auto&& [events, flags] = query.get(t);
        for(auto it : events) {
          if(it.second.isCreate() || it.second.isMove()) {
            flags->getOrAdd(it.first);
          }
        }
      }
    }

    QueryResult<const Events::EventsRow, PhysicsEvents::RecomputeMassRow> query;
  };

  struct PointCache {
    std::vector<glm::vec2> points, temp;
  };

  struct ComputeMass {
    Mass::OriginMass operator()(const ShapeRegistry::Rectangle& v) const {
      return Mass::computeQuadMass(Mass::Quad{
        .fullSize = v.halfWidth*2.f,
        .density = density,
      }).body;
    }

    Mass::OriginMass operator()(const ShapeRegistry::AABB& v) const {
      return Mass::computeQuadMass(Mass::Quad{
        .fullSize = v.max - v.min,
        .density = density
      }).body;
    }

    Mass::OriginMass operator()(const ShapeRegistry::Circle& v) const {
      return Mass::computeCircleMass(Mass::Circle{
        .center = v.pos,
        .radius = v.radius,
        .density = density
      }).body;
    }

    Mass::OriginMass operator()(const ShapeRegistry::Mesh& v) const {
      //Scale affects resulting mass, the rest of the transform doesn't
      const glm::vec2 scale = v.modelToWorld.decompose().scale;
      cache.points.resize(v.points.size());
      cache.temp.resize(v.points.size());
      std::transform(v.points.begin(), v.points.end(), cache.points.begin(), [&](const glm::vec2& p) {
        return p * scale;
      });

      return Mass::computeMeshMass(Mass::Mesh{
        .ccwPoints = cache.points.data(),
        .temp = cache.temp.data(),
        .count = static_cast<uint32_t>(cache.points.size()),
        .radius = 0.f,
        .density = density
      }).body;
    }

    Mass::OriginMass operator()(const ShapeRegistry::Raycast&) const { return unsupported(); }
    Mass::OriginMass operator()(std::monostate) const { return unsupported(); }
    Mass::OriginMass unsupported() const { return {}; }

    const float density{ 1.f };
    PointCache& cache;
  };

  struct UpdateMasses {
    void init(RuntimeDatabaseTaskBuilder& task) {
      query = task;
      shapes = ShapeRegistry::get(task)->createShapeClassifier(task);
      resolver = task.getResolver<const MassModule::IsImmobile>();
      ref = task.getIDResolver()->getRefResolver();
    }

    void execute() {
      PointCache cache;
      for(size_t t = 0; t < query.size(); ++t) {
        auto&& [flags, ids, masses] = query.get(t);
        CachedRow<const MassModule::IsImmobile> isImmobile;
        for(size_t index : flags) {
          const ElementRef& element = ids->at(index);
          const UnpackedDatabaseElementID rawId = ref.unpack(element);

          ShapeRegistry::BodyType shape = shapes->classifyShape(ref.unpack(element));

          //For now, immobile tables are zero mass and everything else assumes 1 density.
          //Eventually, both should be done from a material, or even skip shape lookups for immobile
          const float density = resolver->tryGetOrSwapRow(isImmobile, rawId) ? 0.f : 1.f;

          masses->at(index) = std::visit(ComputeMass{ .density = density, .cache = cache }, shape.shape);
        }
      }
    }

    QueryResult<const PhysicsEvents::RecomputeMassRow, const StableIDRow, MassRow> query;
    std::shared_ptr<ShapeRegistry::IShapeClassifier> shapes;
    std::shared_ptr<ITableResolver> resolver;
    ElementRefResolver ref;
  };

  class MassModuleImpl : public IAppModule {
  public:
    void preProcessEvents(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::create<FlagNewElements>("MassEvents"));
    }

    void update(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::create<UpdateMasses>("UpdateMass"));
    }
  };

  std::unique_ptr<IAppModule> createModule() {
    return std::make_unique<MassModuleImpl>();
  }
}