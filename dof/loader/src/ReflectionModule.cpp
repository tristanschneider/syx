#include "Precompile.h"

#include "loader/ReflectionModule.h"
#include "AppBuilder.h"
#include "TLSTaskImpl.h"
#include "IAppModule.h"
#include "Events.h"
#include "TableName.h"
#include "loader/SceneAsset.h"

namespace Reflection {
  constexpr bool DEBUG_LOAD = true;

  struct Mappings {
    std::unordered_map<DBTypeID, std::unique_ptr<const IRowLoader>> loaders;
    //TableName::TableNameRow hash to the corresponding table in the game database
    std::unordered_map<size_t, RuntimeTable*> nameHashToTable;
  };
  struct MappingsRow : SharedRow<Mappings> {};

  struct RegisterTask {
    struct Group {
      void init(RuntimeDatabaseTaskBuilder& task) {
        mappings = task.query<MappingsRow>().tryGetSingletonElement();
      }

      void init(std::vector<std::unique_ptr<IRowLoader>>&& loads) {
        loaders = std::move(loads);
      }

      Mappings* mappings{};
      std::vector<std::unique_ptr<IRowLoader>> loaders;
    };

    void init() {}

    void execute(Group& group) {
      if(group.mappings) {
        for(auto&& loader : group.loaders) {
          const DBTypeID id = loader->getTypeID();
          group.mappings->loaders.emplace(std::make_pair(id, std::move(loader)));
        }
      }
    }
  };

  void registerLoaders(IAppBuilder& builder, std::vector<std::unique_ptr<IRowLoader>>&& loaders) {
    switch(builder.getEnv().type) {
    case AppEnvType::InitMain:
    case AppEnvType::InitScheduler:
    case AppEnvType::InitThreadLocal:
      builder.submitTask(TLSTask::createWithArgs<RegisterTask, RegisterTask::Group>("register loaders", std::move(loaders)));
      break;
    case AppEnvType::UpdateMain:
      assert(false && "loader registration during update not implemented");
      break;
    }
  }

  void copySceneTable(const Mappings& mappings, RuntimeTable& src, RuntimeTable& dst) {
    if(!src.size()) {
      return;
    }

    //Default construct all the elements
    const size_t begin = dst.addElements(src.size());

    //Read elements row by row
    const auto range = gnx::makeIndexRangeBeginCount(begin, src.size());
    for(auto [type, row] : src) {
      if(auto loader = mappings.loaders.find(type); loader != mappings.loaders.end()) {
        if(loader->second) {
          loader->second->load(*row, dst, range);
        }
      }
    }

    //Flag creation of all the newly added elements
    if(Events::EventsRow* events = dst.tryGet<Events::EventsRow>()) {
      for(size_t i = begin; i < dst.size(); ++i) {
        events->getOrAdd(i).setCreate();
      }
    }
  }

  void copySceneDatabase(const Mappings& mappings, RuntimeDatabase& scene) {
    for(size_t i = 0; i < scene.size(); ++i) {
      RuntimeTable& src = scene[i];
      if constexpr(DEBUG_LOAD) {
        if (const TableName::TableNameRow* name = Loader::tryGetDynamicRow<TableName::TableNameRow>(src)) {
          printf("%s: %d\n", name->at().name.c_str(), static_cast<int>(src.size()));
        }
      }
      if(auto found = mappings.nameHashToTable.find(src.getType().value); found != mappings.nameHashToTable.end() && found->second) {
        copySceneTable(mappings, src, *found->second);
      }
    }
  }

  struct ReflectionReader : IReflectionReader {
    ReflectionReader(RuntimeDatabaseTaskBuilder& task)
      : mappings{ task.query<MappingsRow>().tryGetSingletonElement() } {
      // Artificial dependency on entire database as it's accessible through the mappings
      task.getDatabase();
    }

    void loadFromDBIntoGame(RuntimeDatabase& toRead) final {
      if(mappings) {
        copySceneDatabase(*mappings, toRead);
      }
    }

    const Mappings* mappings{};
  };

  std::unique_ptr<IReflectionReader> createReader(RuntimeDatabaseTaskBuilder& task) {
    return std::make_unique<ReflectionReader>(task);
  }
}

namespace ReflectionModule {
  struct FinalizeTableNames {
    struct Group {
      void init(RuntimeDatabaseTaskBuilder& task) {
        db = &task.getDatabase();
      }

      RuntimeDatabase* db{};
    };

    void init() {}

    void execute(Group& group) {
      assert(group.db);
      Reflection::Mappings* mappings = group.db->query<Reflection::MappingsRow>().tryGetSingletonElement();
      if(!mappings) {
        return;
      }
      auto names = group.db->query<const TableName::TableNameRow>();
      for(size_t i = 0; i < names.size(); ++i) {
        mappings->nameHashToTable[gnx::Hash::constHash(names.get<0>(i).at().name)] = group.db->tryGet(names[i]);
      }
    }
  };

  struct Module : IAppModule {
    using DB = Database<Table<Reflection::MappingsRow>>;

    void createDatabase(RuntimeDatabaseArgs& args) final {
      DBReflect::addDatabase<DB>(args);
    }

    void init(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::create<FinalizeTableNames, FinalizeTableNames::Group>("names"));
    }
  };

  std::unique_ptr<IAppModule> create() {
    return std::make_unique<Module>();
  }
}