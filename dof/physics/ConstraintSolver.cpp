#include "Precompile.h"
#include "ConstraintSolver.h"

#include "AppBuilder.h"
#include "PGSSolver.h"
#include "Physics.h"
#include "SpatialPairsStorage.h"
#include "Geometric.h"
#include <bitset>
#include "generics/Enum.h"

namespace ConstraintSolver {
  using ConstraintIndex = PGS::ConstraintIndex;
  using BodyIndex = PGS::BodyIndex;

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
  struct ContactMapping {
    enum class Bit : uint8_t {
      HasContactOne,
      HasContactTwo,
      HasFriction,
      HasRestitution,
      Count
    };
    using Bitset = std::bitset<gnx::enumCast(Bit::Count)>;

    struct Indices {
      ConstraintIndex contactIndex{};
      std::optional<ConstraintIndex> frictionIndex;
    };

    bool hasFlag(Bit flag) const {
      return flags.test(gnx::enumCast(flag));
    }

    template<class V>
    void visit(const V& v) const {
      if(hasFlag(Bit::HasFriction)) {
        if(hasFlag(Bit::HasContactOne)) {
          v(Indices{ firstContactIndex, std::make_optional(static_cast<ConstraintIndex>(firstContactIndex + 1)) });
          if(hasFlag(Bit::HasContactTwo)) {
            v(Indices{ static_cast<ConstraintIndex>(firstContactIndex + 2), std::make_optional(static_cast<ConstraintIndex>(firstContactIndex + 3)) });
          }
        }
      }
      else {
        if(hasFlag(Bit::HasContactOne)) {
          v(Indices{ firstContactIndex, std::nullopt });
          if(hasFlag(Bit::HasContactTwo)) {
            v(Indices{ static_cast<ConstraintIndex>(firstContactIndex + 1), std::nullopt });
          }
        }
      }
    }

    //Point at first index then assume points are always added as contact then friction constraint
    ConstraintIndex firstContactIndex;
    Bitset flags;
    Material combinedMaterial;
  };
  struct BodyMapping {
    BodyIndex solverIndex{};
    ConstraintMask constraintMask{};
    Material material{};
  };
  struct IslandSolver {
    void clear() {
      bodies.clear();
      warmStartStorage.clear();
      contactMappings.clear();
      islandToBodyIndex.clear();
      solver.clear();
    }

    std::vector<IslandBody> bodies;
    //The place to write the warm start after solving
    //Index matches constraint index
    std::vector<float*> warmStartStorage;
    std::vector<ContactMapping> contactMappings;
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
    CachedRow<const ConstraintMaskRow> constraintMask;
    CachedRow<const SharedMaterialRow> material;
    CachedRow<Row<float>> linVelX;
    CachedRow<Row<float>> linVelY;
    CachedRow<Row<float>> angVel;
  };
  struct ShapeResolver {
    ShapeResolver(RuntimeDatabaseTaskBuilder& task, const PhysicsAliases& tableIds)
      : resolver{ task.getResolver<const SharedMassRow, const MassRow, const SharedMaterialRow>() }
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

  //Material should be provided. If it isn't use one that should look "fine"
  static constexpr Material DEFAULT_MATERIAL{};

  const Material& resolveMaterial(ShapeResolverContext& ctx, const UnpackedDatabaseElementID& id) {
    const Material* result = ctx.resolver.resolver->tryGetOrSwapRowElement(ctx.cache.material, id);
    return result ? *result : DEFAULT_MATERIAL;
  }

  Material combineMaterials(const Material& a, const Material& b) {
    //There are many potential ways to do this to result in realism. For these purposes a plain multilpication is good enough
    //to be able to notice the effects of different materials colliding.
    return {
      //Since friction values 0-1 make most sense multiplication is appropriate
      a.frictionCoefficient * b.frictionCoefficient,
      //Materials with zero restitution should still bounce on bouncy materials so make it additive
      a.restitutionCoefficient + b.restitutionCoefficient
    };
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
    Material material{ DEFAULT_MATERIAL };

    //TODO: lookup could be avoided if using the StableElementID on the spatial pairs table
    if(auto resolved = ids.tryResolveAndUnpack(StableElementID::fromStableID(data))) {
      auto mass = resolveBodyMass(resolver, resolved->unpacked);
      auto velocity = resolveBodyVelocity(resolver, resolved->unpacked);
      constraintMask = resolveConstraintMask(resolver, resolved->unpacked);
      material = resolveMaterial(resolver, resolved->unpacked);

      //Mass, velocity, and index found, create a new body entry for this and write it all into the solver
      if(mass && velocity) {
        IslandBody body;
        body.solverIndex = static_cast<BodyIndex>(solver.bodies.size());
        const BodyMapping mapping{ body.solverIndex, constraintMask, material };
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
    const BodyMapping mapping{ INFINITE_MASS_INDEX, constraintMask, material };
    solver.islandToBodyIndex[data] = mapping;
    return mapping;
  }

  float computeBiasWithRestitution(float baseBias, float restitutionCoefficient, float normalForce) {
    return std::max(baseBias, restitutionCoefficient*normalForce);
  }

  ConstraintIndex addConstraints(
    IslandSolver& solver,
    BodyIndex bodyA,
    BodyIndex bodyB,
    SP::ContactManifold& manifold,
    ConstraintIndex constraintIndex,
    const Material& combinedMaterial,
    const SolverGlobals& globals
  ) {
    ConstraintIndex newConstraintIndex = constraintIndex;
    auto& s = solver.solver;
    ContactMapping contactMapping;
    contactMapping.firstContactIndex = constraintIndex;
    if(combinedMaterial.frictionCoefficient > 0) {
      contactMapping.flags.set(gnx::enumCast(ContactMapping::Bit::HasFriction));
    }
    if(combinedMaterial.restitutionCoefficient > 0) {
      contactMapping.flags.set(gnx::enumCast(ContactMapping::Bit::HasRestitution));
    }
    for(uint32_t i = 0; i < manifold.size; ++i) {
      contactMapping.flags.set(i);
      SP::ContactPoint& point = manifold[i];
      const glm::vec2& normalA{ point.normal };
      const glm::vec2 normalB{ -point.normal };
      const ConstraintIndex contact{ newConstraintIndex++ };
      //Contact constraint
      s.setJacobian(contact, bodyA, bodyB,
        normalA,
        Geo::cross(point.centerToContactA, normalA),
        normalB,
        Geo::cross(point.centerToContactB, normalB)
      );
      const float baseBias = (point.overlap - *globals.slop)**globals.biasTerm;
      float finalBias = baseBias;
      if(combinedMaterial.restitutionCoefficient > 0) {
        finalBias = computeBiasWithRestitution(baseBias, combinedMaterial.restitutionCoefficient, point.contactWarmStart);
      }
      if(finalBias > 0) {
        s.setBias(contact, finalBias);
      }
      else {
        s.setBias(contact, 0);
      }
      s.setLambdaBounds(contact, 0, PGS::SolverStorage::UNLIMITED_MAX);
      s.setWarmStart(contact, point.contactWarmStart);
      solver.warmStartStorage.push_back(&point.contactWarmStart);

      if(combinedMaterial.frictionCoefficient > 0) {
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
        //Friction bounds determined by contact, and the starting bounds of the contact are the warm start
        const float frictionBound = std::abs(point.contactWarmStart*combinedMaterial.frictionCoefficient);
        s.setLambdaBounds(frictionIndex, -frictionBound, frictionBound);
        s.setWarmStart(frictionIndex, point.frictionWarmStart);
        s.setBias(frictionIndex, 0);
        solver.warmStartStorage.push_back(&point.frictionWarmStart);
      }
    }

    static const auto NEEDS_MAPPING = ContactMapping::Bitset{}.
      set(gnx::enumCast(ContactMapping::Bit::HasFriction)).
      set(gnx::enumCast(ContactMapping::Bit::HasRestitution));
    if((NEEDS_MAPPING & contactMapping.flags).any()) {
      //Store this mapping so the bounds of friction can be updated from the corresponding contact
      solver.contactMappings.emplace_back(contactMapping);
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

  void solveIsland(IAppBuilder& builder, const PhysicsAliases& tables, const SolverGlobals& globals) {
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

    task.setCallback([shapes, pairs, graph, collection, ids, globals](AppTaskArgs& args) mutable {
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
                constraintIndex += addConstraints(solver,
                  bodyA.solverIndex,
                  bodyB.solverIndex,
                  manifold,
                  constraintIndex,
                  combineMaterials(bodyA.material, bodyB.material),
                  globals
                );
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
          for(const ContactMapping& mapping : solver.contactMappings) {
            mapping.visit([&](const ContactMapping::Indices& indices) {
              if(indices.frictionIndex) {
                //Friction force is proportional to normal force, update the bounds based on the currently
                //applied normal force
                const float normalForce = std::abs(context.lambda[indices.contactIndex])*mapping.combinedMaterial.frictionCoefficient;
                solver.solver.setLambdaBounds(*indices.frictionIndex, -normalForce, normalForce);
              }
            });
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

  void solveConstraints(IAppBuilder& builder, const PhysicsAliases& tables, const SolverGlobals& globals) {
    solveIsland(builder, tables, globals);
  }
}