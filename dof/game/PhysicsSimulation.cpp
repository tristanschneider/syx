#include "Precompile.h"
#include "PhysicsSimulation.h"

#include "Simulation.h"
#include "TableAdapters.h"

namespace PhysicsSimulation {
  using namespace Tags;
  using PosX = FloatRow<Pos, X>;
  using PosY = FloatRow<Pos, Y>;
  using LinVelX = FloatRow<LinVel, X>;
  using LinVelY = FloatRow<LinVel, Y>;
  using AngVel = FloatRow<AngVel, Angle>;
  using RotX = FloatRow<Rot, CosAngle>;
  using RotY = FloatRow<Rot, SinAngle>;
  //For now use the existence of this row to indicate that the given object should participate in collision
  using HasCollision = Row<CubeSprite>;

  PhysicsTableIds _getPhysicsTableIds() {
    PhysicsTableIds physicsTables;
    physicsTables.mTableIDMask = GameDatabase::ElementID::TABLE_INDEX_MASK;
    physicsTables.mSharedMassConstraintTable = GameDatabase::getTableIndex<ConstraintsTable>().mValue;
    physicsTables.mZeroMassConstraintTable = GameDatabase::getTableIndex<ContactConstraintsToStaticObjectsTable>().mValue;
    physicsTables.mSharedMassObjectTable = GameDatabase::getTableIndex<GameObjectTable>().mValue;
    physicsTables.mZeroMassObjectTable = GameDatabase::getTableIndex<StaticGameObjectTable>().mValue;
    physicsTables.mConstriantsCommonTable = GameDatabase::getTableIndex<ConstraintCommonTable>().mValue;
    physicsTables.mElementIDMask = GameDatabase::ElementID::ELEMENT_INDEX_MASK;
    return physicsTables;
  }

  SweepNPruneBroadphase::BoundariesConfig _getBoundariesConfig() {
    SweepNPruneBroadphase::BoundariesConfig result;
    result.mPadding = 0.0f;
    return result;
  }

  TaskRange _applyDamping(GameDatabase& db, const Config::PhysicsConfig& config) {
    auto result = std::make_shared<TaskNode>();
    result->mChildren.push_back(Physics::applyDampingMultiplier<LinVelX, LinVelY>(db, config.linearDragMultiplier).mBegin);

    std::vector<OwnedTask> tasks;
    Physics::details::applyDampingMultiplierAxis<AngVel>(db, config.angularDragMultiplier, result->mChildren);

    return TaskBuilder::addEndSync(result);
  }

  TaskRange _updateBroadphase(GameDatabase& db, const Config::PhysicsConfig&) {
    auto& broadphase = std::get<BroadphaseTable>(db.mTables);
    auto& collisionPairs = std::get<CollisionPairsTable>(db.mTables);
    auto& globals = std::get<GlobalGameData>(db.mTables);
    const PhysicsTableIds& physicsTables = std::get<SharedRow<PhysicsTableIds>>(globals.mRows).at();
    SweepNPruneBroadphase::ChangedCollisionPairs& changedPairs = std::get<SharedRow<SweepNPruneBroadphase::ChangedCollisionPairs>>(broadphase.mRows).at();
    auto& grid = std::get<SharedRow<Broadphase::SweepGrid::Grid>>(broadphase.mRows).at();
    auto cfg = _getBoundariesConfig();

    std::vector<SweepNPruneBroadphase::BoundariesQuery> bQuery;
    Queries::viewEachRow(db, [&](
      FloatRow<Pos, X>& posX,
      FloatRow<Pos, Y>& posY,
      SweepNPruneBroadphase::BroadphaseKeys& keys) {
        bQuery.push_back({ &posX, &posY, &keys });
    });

    TaskRange boundaries = SweepNPruneBroadphase::updateBoundaries(grid, std::move(bQuery), cfg);
    TaskRange computePairs = SweepNPruneBroadphase::computeCollisionPairs(broadphase);
    auto updatePairs = TaskNode::create([&broadphase, &collisionPairs, &changedPairs, &physicsTables, &db](...) {
      SweepNPruneBroadphase::updateCollisionPairs<CollisionPairIndexA, CollisionPairIndexB, GameDatabase>(
        std::get<SharedRow<SweepNPruneBroadphase::PairChanges>>(broadphase.mRows).at(),
        std::get<SharedRow<SweepNPruneBroadphase::CollisionPairMappings>>(broadphase.mRows).at(),
        collisionPairs,
        physicsTables,
        TableAdapters::getStableMappings({ db } ),
        changedPairs);
    });

    return boundaries.then(computePairs).then(TaskBuilder::addEndSync(updatePairs));
  }

  TaskRange _updateNarrowphase(GameDatabase& db, const Config::PhysicsConfig&) {
    auto& collisionPairs = std::get<CollisionPairsTable>(db.mTables);
    auto& globals = std::get<GlobalGameData>(db.mTables);
    const PhysicsTableIds& physicsTables = std::get<SharedRow<PhysicsTableIds>>(globals.mRows).at();

    //Id resolution must complete before the fill bundle starts
    auto root = TaskNode::create([&](...) {
      PROFILE_SCOPE("physics", "resolveCollisionTableIds");
      Physics::resolveCollisionTableIds(collisionPairs, db, TableAdapters::getStableMappings({ db }), physicsTables);
    });

    std::vector<StableElementID>& idsA = std::get<CollisionPairIndexA>(collisionPairs.mRows).mElements;
    std::vector<StableElementID>& idsB = std::get<CollisionPairIndexB>(collisionPairs.mRows).mElements;
    root->mChildren.push_back(Physics::details::fillRow<PosX, NarrowphaseData<PairA>::PosX>(collisionPairs, db, idsA));
    root->mChildren.push_back(Physics::details::fillRow<PosY, NarrowphaseData<PairA>::PosY>(collisionPairs, db, idsA));
    root->mChildren.push_back(Physics::details::fillRow<RotX, NarrowphaseData<PairA>::CosAngle>(collisionPairs, db, idsA));
    root->mChildren.push_back(Physics::details::fillRow<RotY, NarrowphaseData<PairA>::SinAngle>(collisionPairs, db, idsA));
    root->mChildren.push_back(Physics::details::fillRow<PosX, NarrowphaseData<PairB>::PosX>(collisionPairs, db, idsB));
    root->mChildren.push_back(Physics::details::fillRow<PosY, NarrowphaseData<PairB>::PosY>(collisionPairs, db, idsB));
    root->mChildren.push_back(Physics::details::fillRow<RotX, NarrowphaseData<PairB>::CosAngle>(collisionPairs, db, idsB));
    root->mChildren.push_back(Physics::details::fillRow<RotY, NarrowphaseData<PairB>::SinAngle>(collisionPairs, db, idsB));

    //After everything is filled, generate contacts
    auto contacts = TaskNode::create([&](...) {
      PROFILE_SCOPE("physics", "generate contacts");
      Physics::generateContacts(collisionPairs);
    });
    TaskBuilder::_addSyncDependency(*root, contacts);
    //Final bundle begins with resolveIds and ends after completion of contact generation
    return { root, contacts };
  }

  TaskRange _debugUpdate(GameDatabase& db, const Config::PhysicsConfig& config) {
    auto task = TaskNode::create([&db, &config](...) {
      auto& debug = std::get<DebugLineTable>(db.mTables);
      auto& collisionPairs = std::get<CollisionPairsTable>(db.mTables);

      auto addLine = [&debug](glm::vec2 a, glm::vec2 b, glm::vec3 color) {
        DebugLineTable::ElementRef e = TableOperations::addToTable(debug);
        e.get<0>().mPos = a;
        e.get<0>().mColor = color;
        e = TableOperations::addToTable(debug);
        e.get<0>().mPos = b;
        e.get<0>().mColor = color;
      };

      const bool drawCollisionPairs = config.drawCollisionPairs;
      const bool drawContacts = config.drawContacts;
      if(drawCollisionPairs) {
        auto& ax = std::get<NarrowphaseData<PairA>::PosX>(collisionPairs.mRows);
        auto& ay = std::get<NarrowphaseData<PairA>::PosY>(collisionPairs.mRows);
        auto& bx = std::get<NarrowphaseData<PairB>::PosX>(collisionPairs.mRows);
        auto& by = std::get<NarrowphaseData<PairB>::PosY>(collisionPairs.mRows);
        for(size_t i = 0; i < ax.size(); ++i) {
          addLine(glm::vec2(ax.at(i), ay.at(i)), glm::vec2(bx.at(i), by.at(i)), glm::vec3(0.0f, 1.0f, 0.0f));
        }
      }

      if(drawContacts) {
        for(size_t i = 0; i < TableOperations::size(collisionPairs); ++i) {
          CollisionPairsTable::ElementRef e = TableOperations::getElement(collisionPairs, i);
          float overlapOne = e.get<12>();
          float overlapTwo = e.get<15>();
          glm::vec2 posA{ e.get<2>(), e.get<3>() };
          glm::vec2 posB{ e.get<6>(), e.get<7>() };
          glm::vec2 contactOne{ e.get<10>(), e.get<11>() };
          glm::vec2 contactTwo{ e.get<13>(), e.get<14>() };
          glm::vec2 normal{ e.get<16>(), e.get<17>() };
          if(overlapOne >= 0.0f) {
            addLine(posA, contactOne, glm::vec3(1.0f, 0.0f, 0.0f));
            addLine(contactOne, contactOne + normal*0.25f, glm::vec3(0.0f, 1.0f, 0.0f));
            addLine(contactOne, contactOne + normal*overlapOne, glm::vec3(1.0f, 1.0f, 0.0f));
          }
          if(overlapTwo >= 0.0f) {
            addLine(posA, contactTwo, glm::vec3(1.0f, 0.0f, 1.0f));
            addLine(contactTwo, contactTwo + normal*0.25f, glm::vec3(0.0f, 1.0f, 1.0f));
            addLine(contactTwo, contactTwo + normal*overlapTwo, glm::vec3(1.0f, 1.0f, 1.0f));
          }
        }
      }
    });
    return { task, task };
  }

  void init(GameDB game) {
    *TableAdapters::getGlobals(game).physicsTables = _getPhysicsTableIds();
    auto& grid = std::get<SharedRow<Broadphase::SweepGrid::Grid>>(std::get<BroadphaseTable>(game.db.mTables).mRows).at();
    grid.definition.bottomLeft = glm::vec2{ -1000.0f, -1000.0f };
    grid.definition.cellSize = glm::vec2{ 20.0f, 20.0f };
    grid.definition.cellsX = 100;
    grid.definition.cellsY = 100;
    Broadphase::SweepGrid::init(grid);
  }

  TaskRange updatePhysics(GameDB game) {
    PROFILE_SCOPE("physics", "update");

    GameDatabase& db = game.db;
    const Config::PhysicsConfig& config = *TableAdapters::getConfig(game).physics;
    auto& broadphase = std::get<BroadphaseTable>(db.mTables);
    auto& constraints = std::get<ConstraintsTable>(db.mTables);
    auto& staticConstraints = std::get<ContactConstraintsToStaticObjectsTable>(db.mTables);
    auto& constraintsCommon = std::get<ConstraintCommonTable>(db.mTables);
    auto& globals = std::get<GlobalGameData>(db.mTables);
    const PhysicsTableIds& physicsTables = std::get<SharedRow<PhysicsTableIds>>(globals.mRows).at();
    ConstraintsTableMappings& constraintsMappings = std::get<SharedRow<ConstraintsTableMappings>>(globals.mRows).at();

    auto root = std::make_shared<TaskNode>();
    auto current = root;

    TaskRange velocityTasks = PhysicsSimulation::_applyDamping(db, config);
    TaskRange broadphaseUpdate = PhysicsSimulation::_updateBroadphase(db, config);
    TaskRange narrowphase = PhysicsSimulation::_updateNarrowphase(db, config);

    //Narrowphase depends on completion of the broadphase
    current->mChildren.push_back(velocityTasks.mBegin);
    current = velocityTasks.mEnd;
    current->mChildren.push_back(broadphaseUpdate.mBegin);
    current = broadphaseUpdate.mEnd;
    current->mChildren.push_back(narrowphase.mBegin);
    current = narrowphase.mEnd;

    SweepNPruneBroadphase::ChangedCollisionPairs& changedPairs = std::get<SharedRow<SweepNPruneBroadphase::ChangedCollisionPairs>>(broadphase.mRows).at();

    //Build can start as soon as broadphase is complete during narrowphase
    TaskRange buildConstraints = ConstraintsTableBuilder::build(db, changedPairs, TableAdapters::getStableMappings({ db }), constraintsMappings, physicsTables, config);
    broadphaseUpdate.mEnd->mChildren.push_back(buildConstraints.mBegin);
    //Fill after build
    TaskRange fillConstraints = Physics::fillConstraintVelocities<LinVelX, LinVelY, AngVel>(constraintsCommon, db);
    buildConstraints.mEnd->mChildren.push_back(fillConstraints.mBegin);
    //Setup in parallel with velocity since it doesn't use velocity, but it requires contact generation to finish
    TaskRange setupConstraints = Physics::setupConstraints(constraints, staticConstraints);
    buildConstraints.mEnd->mChildren.push_back(setupConstraints.mBegin);
    narrowphase.mEnd->mChildren.push_back(setupConstraints.mBegin);

    TaskRange debug = PhysicsSimulation::_debugUpdate(db, config);

    //Sync everything down before doing several sequential solves
    auto solving = std::make_shared<TaskNode>();
    TaskBuilder::_addSyncDependency(*root, solving);

    const int solveIterations = config.solveIterations;
    //TODO: stop early if global lambda sum falls below tolerance
    for(int i = 0; i < solveIterations; ++i) {
      auto solver = Physics::solveConstraints(constraints, staticConstraints, constraintsCommon, config);
      solving->mChildren.push_back(solver.mBegin);
      solving = solver.mEnd;
    }

    auto postSolve = solving;
    TaskRange storeVelocity = Physics::storeConstraintVelocities<LinVelX, LinVelY, AngVel>(constraintsCommon, db);
    postSolve->mChildren.push_back(storeVelocity.mBegin);
    postSolve = storeVelocity.mEnd;

    TaskRange integratePosition = Physics::integratePosition<LinVelX, LinVelY, PosX, PosY>(db);
    postSolve->mChildren.push_back(integratePosition.mBegin);
    postSolve = integratePosition.mEnd;

    TaskRange integrateRotation = Physics::integrateRotation<RotX, RotY, AngVel>(db);
    postSolve->mChildren.push_back(integrateRotation.mBegin);
    postSolve = integrateRotation.mEnd;

    postSolve->mChildren.push_back(debug.mBegin);
    postSolve = debug.mEnd;

    return { root, postSolve };
  }

  TaskRange processEvents(GameDB db) {
    auto root = TaskNode::create([db](...) {
      auto resolver = TableResolver<PosX, PosY, SweepNPruneBroadphase::BroadphaseKeys, StableIDRow, SharedRow<IsImmobile>>::create(db.db);
      const DBEvents& events = Events::getPublishedEvents(db);
      auto& grid = std::get<SharedRow<Broadphase::SweepGrid::Grid>>(std::get<BroadphaseTable>(db.db.mTables).mRows).at();
      SweepNPruneBroadphase::processEvents(events, grid, resolver, _getBoundariesConfig(), GameDatabase::getDescription());
    });
    return TaskBuilder::addEndSync(root);
  }
}