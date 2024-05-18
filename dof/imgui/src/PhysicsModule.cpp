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

    task.setCallback([query, drawer, ids, shapes, config, enabled](AppTaskArgs&) mutable {
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
        const auto thisTable = query.matchingTableIDs[t];
        auto [graph, manifolds] = query.get(t);
        IslandGraph::Graph& g = graph->at();
        //Hack since by the time this runs the islands could be out of date from elements being removed
        IslandGraph::rebuildIslands(g);
        //TODO: const iterators so they can be used here
        for(size_t i = 0; i < g.islands.values.size(); ++i) {
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
              const glm::vec2 centerA = ShapeRegistry::getCenter(shapes->classifyShape(*resolvedA));
              const glm::vec2 centerB = ShapeRegistry::getCenter(shapes->classifyShape(*resolvedB));

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

  void update(IAppBuilder& builder) {
    drawIslands(builder);

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
      ImGui::End();
    });
    builder.submitTask(std::move(task));
  }
}