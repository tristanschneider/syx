#include "Precompile.h"

#include "test/PhysicsTestModule.h"

#include <AppBuilder.h>
#include <GameDatabase.h>
#include <GraphicsTables.h>
#include <IAppModule.h>
#include <loader/ReflectionModule.h>
#include <SceneNavigator.h>
#include <TLSTaskImpl.h>
#include <TableName.h>
#include <Narrowphase.h>
#include <shapes/Rectangle.h>

namespace PhysicsTestModule {
  //Objects with collision being checked for correctness
  struct ValidationTargetRow : Loader::PersistentElementRefRow {
    static constexpr std::string_view KEY = "ValidationTarget";
  };
  //Markers indicating the correct collision information for objects in the ValidationTargetRow
  struct ValidationMarkerRow : TagRow{};
  //Table of static physics objects with collision
  //Table of marker objects indicating the correct collision area
  class Module : public IAppModule {
  public:
    static void addBase(StorageTableBuilder& table) {
      table.setStable();
      table.addRows<
        SceneNavigator::IsClearedWithSceneTag,
        Narrowphase::SharedThicknessRow
      >();
      GameDatabase::addTransform25D(table);
      GameDatabase::addRenderable(table, GameDatabase::RenderableOptions{
        .sharedTexture = true,
        .sharedMesh = false,
      });
      GameDatabase::addGameplayCopy(table);
    }

    void createDatabase(RuntimeDatabaseArgs& args) {
      std::invoke([]{
        StorageTableBuilder table;
        addBase(table);
        GameDatabase::addCollider(table);
        table.addRows<
          ValidationTargetRow,
          Shapes::SharedRectangleRow
        >().setTableName({ "CollisionToValidate" });
        return table;
      }).finalize(args);

      std::invoke([]{
        StorageTableBuilder table;
        addBase(table);
        table.addRows<
          ValidationMarkerRow
        >().setTableName({ "CollisionMarkers" });
        return table;
      }).finalize(args);
    }

    void init(IAppBuilder& builder) {
      Reflection::registerLoaders(builder, std::make_unique<Reflection::ObjIDLoader<ValidationTargetRow>>());
    }
  };

  std::unique_ptr<IAppModule> create() {
    return std::make_unique<Module>();
  }
}