#include "Precompile.h"
#include "ConstraintSolver.h"

#include "AppBuilder.h"
#include "PGSSolver.h"
#include "Physics.h"
#include "SpatialPairsStorage.h"
#include "Geometric.h"

namespace ConstraintSolver {
  using ConstraintIndex = PGS::ConstraintIndex;
  using BodyIndex = PGS::BodyIndex;

  constexpr float biasTerm = 0.91f;
  constexpr float slop = 0.005f;
  constexpr float frictionTerm = 0.8f;

  struct IslandBody {
    operator bool() const {
      return velocityX && velocityY && angularVelocity;
    }

    float* velocityX{};
    float* velocityY{};
    float* angularVelocity{};
    //Index of this body in the solver
    BodyIndex solverIndex{};
  };
  //The limit of friction constraints is determined by the force already applied by contact constraints
  //This mapping allows easy indexing of both to update the friction bounds
  struct FrictionMapping {
    ConstraintIndex frictionIndex{};
    ConstraintIndex contactIndex{};
  };
  struct BodyMapping {
    BodyIndex solverIndex{};
    ConstraintMask constraintMask{};
  };
  struct IslandSolver {
    void clear() {
      bodies.clear();
      warmStartStorage.clear();
      frictionMappings.clear();
      islandToBodyIndex.clear();
      solver.clear();
    }

    std::vector<IslandBody> bodies;
    //The place to write the warm start after solving
    //Index matches constraint index
    std::vector<float*> warmStartStorage;
    std::vector<FrictionMapping> frictionMappings;
    std::unordered_map<IslandGraph::NodeUserdata, BodyMapping> islandToBodyIndex;
    PGS::SolverStorage solver;
  };
  struct IslandSolverCollection {
    //Each in separate memory to discourage false sharing
    std::vector<std::unique_ptr<IslandSolver>> solvers;
    size_t size{};
  };
  struct ShapeResoverCache {
    CachedRow<const SharedMassRow> sharedMass;
    CachedRow<const MassRow> individualMass;
    CachedRow<Row<float>> linVelX;
    CachedRow<Row<float>> linVelY;
    CachedRow<Row<float>> angVel;
    CachedRow<const ConstraintMaskRow> constraintMask;
  };
  struct ShapeResolver {
    ShapeResolver(RuntimeDatabaseTaskBuilder& task, const PhysicsAliases& tableIds)
      : resolver{ task.getResolver<const SharedMassRow, const MassRow>() }
      , tables{ tableIds }
      , aliasResolver{ task.getAliasResolver(tableIds.linVelX, tableIds.linVelY, tableIds.angVel) }
    {}

    PhysicsAliases tables;
    std::shared_ptr<ITableResolver> resolver;
    std::shared_ptr<ITableResolver> aliasResolver;
  };
  struct ShapeResolverContext {
    ShapeResolver& resolver;
    ShapeResoverCache& cache;
  };

  struct BodyVelocity {
    operator bool() const {
      return linearX && linearY && angular;
    }
    float* linearX{};
    float* linearY{};
    float* angular{};
  };

  std::optional<BodyMass> resolveBodyMass(ShapeResolverContext& ctx, const UnpackedDatabaseElementID& id) {
    const BodyMass* result = ctx.resolver.resolver->tryGetOrSwapRowElement(ctx.cache.sharedMass, id);
    if(!result) {
      result = ctx.resolver.resolver->tryGetOrSwapRowElement(ctx.cache.individualMass, id);
    }
    //Having explicit zero mass is the same as not having any
    return result && (result->inverseInertia || result->inverseMass) ? std::make_optional(*result) : std::nullopt;
  }

  BodyVelocity resolveBodyVelocity(ShapeResolverContext& ctx, const UnpackedDatabaseElementID& id) {
    BodyVelocity result;
    result.linearX = ctx.resolver.resolver->tryGetOrSwapRowAliasElement(ctx.resolver.tables.linVelX, ctx.cache.linVelX, id);
    result.linearY = ctx.resolver.resolver->tryGetOrSwapRowAliasElement(ctx.resolver.tables.linVelY, ctx.cache.linVelY, id);
    result.angular = ctx.resolver.resolver->tryGetOrSwapRowAliasElement(ctx.resolver.tables.angVel, ctx.cache.angVel, id);
    return result;
  }

  ConstraintMask resolveConstraintMask(ShapeResolverContext& ctx, const UnpackedDatabaseElementID& id) {
    const ConstraintMask* result = ctx.resolver.resolver->tryGetOrSwapRowElement(ctx.cache.constraintMask, id);
    return result ? *result : ConstraintMask{};
  }

  constexpr BodyIndex INFINITE_MASS_INDEX{};
  //This is a special entry at index zero that all infinite mass objects can point at because
  //none of their velocities can change
  void insertInfiniteMassBody(IslandSolver& solver) {
    solver.solver.setMass(INFINITE_MASS_INDEX, 0, 0);
    solver.solver.setVelocity(INFINITE_MASS_INDEX, { 0, 0 }, 0);
    //Add empty entry so that these indices still always line up
    solver.bodies.emplace_back();
  }

  BodyMapping getOrCreateBody(
    IslandGraph::NodeUserdata data,
    IslandSolver& solver,
    ShapeResolverContext& resolver,
    IIDResolver& ids
  ) {
    if(auto it = solver.islandToBodyIndex.find(data); it != solver.islandToBodyIndex.end()) {
      return it->second;
    }
    ConstraintMask constraintMask{};
    //TODO: lookup could be avoided if using the StableElementID on the spatial pairs table
    if(auto resolved = ids.tryResolveAndUnpack(StableElementID::fromStableID(data))) {
      auto mass = resolveBodyMass(resolver, resolved->unpacked);
      auto velocity = resolveBodyVelocity(resolver, resolved->unpacked);
      constraintMask = resolveConstraintMask(resolver, resolved->unpacked);

      //Mass, velocity, and index found, create a new body entry for this and write it all into the solver
      if(mass && velocity) {
        IslandBody body;
        body.solverIndex = static_cast<BodyIndex>(solver.bodies.size());
        const BodyMapping mapping{ body.solverIndex, constraintMask };
        solver.islandToBodyIndex[data] = mapping;
        body.velocityX = velocity.linearX;
        body.velocityY = velocity.linearY;
        body.angularVelocity = velocity.angular;
        solver.solver.setMass(body.solverIndex, mass->inverseMass, mass->inverseInertia);
        solver.solver.setVelocity(body.solverIndex, { *body.velocityX, *body.velocityY }, *body.angularVelocity);
        solver.bodies.emplace_back(body);
        return mapping;
      }
    }
    //Something was missing, write this as the special object with infinite mass
    const BodyMapping mapping{ INFINITE_MASS_INDEX, constraintMask };
    solver.islandToBodyIndex[data] = mapping;
    return mapping;
  }

  ConstraintIndex addConstraints(
    IslandSolver& solver,
    BodyIndex bodyA,
    BodyIndex bodyB,
    SP::ContactManifold& manifold,
    ConstraintIndex constraintIndex
  ) {
    ConstraintIndex newConstraintIndex = constraintIndex;
    auto& s = solver.solver;
    for(uint32_t i = 0; i < manifold.size; ++i) {
      SP::ContactPoint& point = manifold[i];
      const glm::vec2& normalA{ point.normal };
      const glm::vec2 normalB{ -point.normal };
      //Contact constraint
      s.setJacobian(newConstraintIndex, bodyA, bodyB,
        normalA,
        Geo::cross(point.centerToContactA, normalA),
        normalB,
        Geo::cross(point.centerToContactB, normalB)
      );
      const float bias = point.overlap - slop;
      if(bias > 0) {
        s.setBias(newConstraintIndex, bias*biasTerm);
      }
      else {
        s.setBias(newConstraintIndex, 0);
      }
      s.setLambdaBounds(newConstraintIndex, 0, PGS::SolverStorage::UNLIMITED_MAX);
      s.setWarmStart(newConstraintIndex, point.contactWarmStart);
      solver.warmStartStorage.push_back(&point.contactWarmStart);
      const ConstraintIndex contactIndex = newConstraintIndex++;

      contactIndex;
      /* TODO: put this back
      //Friction constraint
      const glm::vec2 frictionA{ Geo::orthogonal(normalA) };
      const glm::vec2 frictionB{ -frictionA };
      const ConstraintIndex frictionIndex = newConstraintIndex++;
      s.setJacobian(frictionIndex, bodyA, bodyB,
        frictionA,
        Geo::cross(point.centerToContactA, frictionA),
        frictionB,
        Geo::cross(point.centerToContactB, frictionB)
      );
      //Store this mapping so the bounds of friction can be updated from the corresponding contact
      solver.frictionMappings.push_back({ frictionIndex, contactIndex });
      //Friction bounds determined by contact, and the starting bounds of the contact are the warm start
      const float frictionBound = std::abs(point.contactWarmStart*frictionTerm);
      s.setLambdaBounds(frictionIndex, -frictionBound, frictionBound);
      s.setWarmStart(frictionIndex, point.frictionWarmStart);
      s.setBias(frictionIndex, 0);
      solver.warmStartStorage.push_back(&point.frictionWarmStart);
      */
    }

    return newConstraintIndex - constraintIndex;
  }

  void createSolvers(IAppBuilder& builder, std::shared_ptr<IslandSolverCollection> collection, std::shared_ptr<AppTaskConfig> solverConfig) {
    auto task = builder.createTask();
    task.setName("create solvers");
    //Assume there is only one for now
    IslandGraph::Graph* graph = task.query<SP::IslandGraphRow>().tryGetSingletonElement();
    task.setCallback([graph, collection, solverConfig](AppTaskArgs&) {
      IslandGraph::rebuildIslands(*graph);
      const size_t desiredIslands = graph->islands.size();
      const size_t currentIslands = collection->solvers.size();
      if(currentIslands < desiredIslands) {
        collection->solvers.resize(desiredIslands);
        for(size_t i = currentIslands; i < desiredIslands; ++i) {
          collection->solvers[i] = std::make_unique<IslandSolver>();
        }
      }
      collection->size = desiredIslands;
      AppTaskSize taskSize;
      taskSize.batchSize = 1;
      taskSize.workItemCount = collection->size;
      solverConfig->setSize(taskSize);
    });
    builder.submitTask(std::move(task));
  }

  void solveIsland(IAppBuilder& builder, const PhysicsAliases& tables) {
    auto task = builder.createTask();
    task.setName("solve islands");
    auto collection = std::make_shared<IslandSolverCollection>();
    std::shared_ptr<AppTaskConfig> solveConfig = task.getConfig();
    createSolvers(builder, collection, solveConfig);
    const IslandGraph::Graph* graph = task.query<const SP::IslandGraphRow>().tryGetSingletonElement();
    auto pairs = task.query<
      SP::ManifoldRow
    >();
    ShapeResolver shapes{ task, tables };
    auto ids = task.getIDResolver();

    task.setCallback([shapes, pairs, graph, collection, ids](AppTaskArgs& args) mutable {
      ShapeResoverCache cache;
      ShapeResolverContext shapeContext{ shapes, cache };
      SP::ManifoldRow& manifolds = pairs.get<0>(0);
      for(size_t i = args.begin; i < args.end; ++i) {
        const IslandGraph::Island& island = graph->islands[i];
        IslandSolver& solver = *collection->solvers[i];
        //TODO: only clear what has a chance of being untouched by the initialization below
        solver.clear();
        //This may overshoot a bit if objects are close enough to have edges but didn't pass narrowphase
        //Each edge can have two contacts each of which can have two friction constraints
        solver.solver.resize(static_cast<BodyIndex>(island.nodeCount + 1), static_cast<ConstraintIndex>(island.edgeCount*4));
        insertInfiniteMassBody(solver);
        ConstraintIndex constraintIndex{};

        //TODO: const iterator on graph
        uint32_t currentEdge = island.edges;
        while(currentEdge != IslandGraph::INVALID) {
          const IslandGraph::Edge& e = graph->edges[currentEdge];
          //Use the edge to get the manifold to see if there is anything to solve
          if(auto resolved = ids->tryResolveAndUnpack(StableElementID::fromStableID(e.data))) {
            assert(pairs.matchingTableIDs[0].getTableIndex() == resolved->unpacked.getTableIndex());
            SP::ContactManifold& manifold = manifolds.at(resolved->unpacked.getElementIndex());
            if(manifold.size) {
              //This is a constraint to solve, pull out the required information
              const BodyMapping bodyA = getOrCreateBody(graph->nodes[e.nodeA].data, solver, shapeContext, *ids);
              const BodyMapping bodyB = getOrCreateBody(graph->nodes[e.nodeB].data, solver, shapeContext, *ids);
              if(bodyA.constraintMask & bodyB.constraintMask) {
                constraintIndex += addConstraints(solver, bodyA.solverIndex, bodyB.solverIndex, manifold, constraintIndex);
              }
            }
          }
          currentEdge = e.islandNext;
        }

        //Remove any excess. This could be since multiple objects mapped to the infinite mass index, some data was not present,
        //or their constraint masks indicated they don't want to solve against each-other
        solver.solver.resize(static_cast<BodyIndex>(solver.bodies.size()), constraintIndex);

        solver.solver.premultiply();
        PGS::SolveContext context{ solver.solver.createContext() };
        //TODO: consider skipping when there is no warm start. Maybe it's uncommon enough not to be worth it
        PGS::warmStart(context);
        PGS::SolveResult solveResult;
        do {
          solveResult = PGS::advancePGS(context);
          for(const FrictionMapping& mapping : solver.frictionMappings) {
            //Friction force is proportional to normal force, update the bounds based on the currently
            //applied normal force
            const float normalForce = std::abs(context.lambda[mapping.contactIndex]);
            solver.solver.setLambdaBounds(mapping.frictionIndex, -normalForce, normalForce);
          }
        } while(!solveResult.isFinished);

        //Write out the solved velocities
        for(IslandBody& body : solver.bodies) {
          if(body) {
            PGS::BodyVelocity v = context.velocity.getBody(body.solverIndex);
            //TODO: accumulate this energy to check for sleep elligibility
            *body.velocityX = v.linear.x;
            *body.velocityY = v.linear.y;
            *body.angularVelocity = v.angular;
          }
        }
        //Store warm starts for next time
        for(ConstraintIndex c = 0; c < constraintIndex; ++c) {
          if(float* storage = solver.warmStartStorage[c]) {
            *storage = solver.solver.lambda[c];
          }
        }
      }
    });

    builder.submitTask(std::move(task));
  }

  void solveConstraints(IAppBuilder& builder, const PhysicsAliases& tables) {
    solveIsland(builder, tables);
  }
}