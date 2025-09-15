#include "Precompile.h"
#include "PhysicsModule.h"

#include "config/Config.h"
#include "Simulation.h"
#include "TableAdapters.h"
#include "imgui.h"
#include "ImguiExt.h"
#include "AppBuilder.h"
#include "ImguiModule.h"

#include "DebugDrawer.h"
#include "Narrowphase.h"
#include "PhysicsSimulation.h"
#include "SpatialPairsStorage.h"
#include "ThreadLocals.h"
#include "Random.h"
#include <transform/TransformResolver.h>
#include <shapes/ShapeRegistry.h>
#include <TLSTaskImpl.h>

namespace PhysicsModule {
  void drawIslands(IAppBuilder& builder) {
    auto task = builder.createTask();
    task.setName("Draw islands");
    auto query = task.query<
      SP::IslandGraphRow,
      const SP::ManifoldRow
    >();
    auto drawer = TableAdapters::getDebugLines(task);
    auto ids = task.getIDResolver();
    auto shapes = PhysicsSimulation::createShapeClassifier(task);
    Config::PhysicsConfig* config = TableAdapters::getPhysicsConfigMutable(task);
    const bool* enabled = ImguiModule::queryIsEnabled(task);
    Transform::Resolver transformResolver{ task, Transform::ResolveOps{}.addInverse() };

    task.setCallback([query, drawer, ids, shapes, config, enabled, transformResolver](AppTaskArgs&) mutable {
      if(!*enabled) {
        return;
      }
      const bool drawIslandEdges = config->drawCollisionPairs;
      const bool drawContacts = config->drawContacts;
      if(!drawIslandEdges && !drawContacts) {
        return;
      }
      glm::vec3 islandColor{ 1, 1, 1 };
      const glm::vec3 contactColor{ 0, 1, 1 };
      const glm::vec3 normalColor{ 0, 0, 1 };
      std::hash<size_t> colorHash;
      const float normalLength = 0.15f;
      auto resolver = ids->getRefResolver();

      for(size_t t = 0; t < query.size(); ++t) {
        const auto thisTable = query[t];
        auto [graph, manifolds] = query.get(t);
        IslandGraph::Graph& g = graph->at();
        //Hack since by the time this runs the islands could be out of date from elements being removed
        IslandGraph::rebuildIslands(g);
        //TODO: const iterators so they can be used here
        for(size_t i = 0; i < g.islands.getValues().size(); ++i) {
          const IslandGraph::Island& island = g.islands[i];
          const size_t colorBits = colorHash(i);
          //Generate an arbitrary color using the bytes of the hash as 8 bit rgb values
          for(int c = 0; c < 3; ++c) {
             islandColor[c] = static_cast<float>((colorBits >> (8 * c)) & 0xFF)/255.f;
          }

          uint32_t edge = island.edges;
          std::optional<glm::vec2> lastIslandEdge;
          while(edge != IslandGraph::INVALID) {
            const auto& e = g.edges[edge];

            auto resolvedA = resolver.tryUnpack(g.nodes[e.nodeA].data);
            auto resolvedB = resolver.tryUnpack(g.nodes[e.nodeB].data);
            if(resolvedA && resolvedB) {
              auto pairA = transformResolver.resolvePair(*resolvedA);
              auto pairB = transformResolver.resolvePair(*resolvedB);
              const glm::vec2 centerA = ShapeRegistry::getCenter(shapes->classifyShape(*resolvedA, pairA.modelToWorld, pairA.worldToModel));
              const glm::vec2 centerB = ShapeRegistry::getCenter(shapes->classifyShape(*resolvedB, pairB.modelToWorld, pairB.worldToModel));

              if(drawIslandEdges) {
                const glm::vec2 currentIslandEdge = (centerA + centerB)*0.5f;
                if(lastIslandEdge) {
                  DebugDrawer::drawLine(drawer, *lastIslandEdge, currentIslandEdge, islandColor);
                }
                DebugDrawer::drawLine(drawer, centerA, centerB, islandColor);
                lastIslandEdge = currentIslandEdge;
              }

              if(drawContacts) {
                const SP::ContactManifold& manifold = manifolds->at(e.data);
                for(size_t p = 0; p < manifold.size; ++p) {
                  const SP::ContactPoint& cp = manifold[p];
                  const glm::vec2 contactA = centerA + cp.centerToContactA;
                  const glm::vec2 contactB = centerB + cp.centerToContactB;
                  DebugDrawer::drawLine(drawer, centerA, contactA, contactColor);
                  DebugDrawer::drawVector(drawer, contactA, cp.normal*normalLength, normalColor);
                  DebugDrawer::drawLine(drawer, centerB, contactB, contactColor);
                  DebugDrawer::drawVector(drawer, contactB, cp.normal*normalLength, normalColor);
                }
              }
            }

            edge = e.islandNext;
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  struct DrawMeshes {
    void init(RuntimeDatabaseTaskBuilder& task) {
      res = task.getRefResolver();
      classifier = PhysicsSimulation::createShapeClassifier(task);
      query = task;
      debug = TableAdapters::getDebugLines(task);
      enabled = ImguiModule::queryIsEnabled(task);
      config = TableAdapters::getPhysicsConfigMutable(task);
    }

    void execute() {
      if(!enabled || !*enabled || !config || !config->drawMesh) {
        return;
      }
      std::vector<glm::vec2> buffer;
      for(size_t t = 0; t < query.size(); ++t) {
        auto [_, stables, transforms, inverses] = query.get(t);
        for(size_t i = 0; i < stables->size(); ++i) {
          const auto id = res.unpack(stables->at(i));
          const auto& transform = transforms->at(i);

          const ShapeRegistry::Mesh mesh = std::visit([&](const auto& shape) {
            return Narrowphase::toMesh(shape, buffer);
          }, classifier->classifyShape(id, transform, inverses->at(i)).shape);

          if(mesh.points.size()) {
            //Not all implementations write to buffer, put them here so they can be transformed
            buffer = mesh.points;
            for(glm::vec2& p : buffer) {
              p = transform.transformPoint(p);
            }

            const glm::vec2* last = &buffer.back();
            for(const glm::vec2& current : buffer) {
              DebugDrawer::drawLine(debug, *last, current, glm::vec3{ 1, 0, 0 });
              last = &current;
            }
          }
        }
      }
    }

    ElementRefResolver res;
    std::shared_ptr<ShapeRegistry::IShapeClassifier> classifier;
    QueryResult<
      const Narrowphase::CollisionMaskRow,
      const StableIDRow,
      const Transform::WorldTransformRow,
      const Transform::WorldInverseTransformRow
    > query;
    DebugLineAdapter debug;
    Config::PhysicsConfig* config{};
    const bool* enabled{};
  };

  void update(IAppBuilder& builder) {
    drawIslands(builder);
    builder.submitTask(TLSTask::create<DrawMeshes>("Draw Physics Meshes"));

    auto task = builder.createTask();
    task.setName("Imgui Physics").setPinning(AppTaskPinning::MainThread{});
    Config::PhysicsConfig* config = TableAdapters::getPhysicsConfigMutable(task);
    const bool* enabled = ImguiModule::queryIsEnabled(task);
    assert(config);
    task.setCallback([config, enabled](AppTaskArgs&) mutable {
      if(!*enabled) {
        return;
      }
      ImGui::Begin("Physics");
      bool force = config->mForcedTargetWidth.has_value();
      if(ImGui::Checkbox("Force SIMD Target Width", &force)) {
        config->mForcedTargetWidth = size_t(1);
      }
      if(config->mForcedTargetWidth) {
        ImguiExt::inputSizeT("Forced Target Width", &*config->mForcedTargetWidth);
      }
      ImGui::SliderFloat("Linear Drag", &config->linearDragMultiplier, 0.5f, 1.0f);
      ImGui::SliderFloat("Angular Drag", &config->angularDragMultiplier, 0.5f, 1.0f);
      ImGui::SliderFloat("Friction Coefficient", &config->frictionCoeff, 0.0f, 1.0f);
      ImGui::InputInt("Solve Iterations", &config->solveIterations);
      ImGui::Checkbox("Draw Collision Pairs", &config->drawCollisionPairs);
      ImGui::Checkbox("Draw Contacts", &config->drawContacts);
      ImGui::Checkbox("Draw Broadphase", &config->broadphase.draw);
      ImGui::Checkbox("Draw Physics Meshes", &config->drawMesh);
      ImGui::End();
    });
    builder.submitTask(std::move(task));
  }
}