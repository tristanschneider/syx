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
    std::unordered_map<DBTypeID, std::vector<std::unique_ptr<const IRowLoader>>> loaders;
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
          group.mappings->loaders[id].push_back(std::move(loader));
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

  void copySceneTable(Loader::ObjIDMappings& idMappings, const Mappings& mappings, RuntimeTable& src, RuntimeTable& dst) {
    if(!src.size()) {
      return;
    }

    //Default construct all the elements
    const size_t begin = dst.addElements(src.size());
    const auto range = gnx::makeIndexRangeBeginCount(begin, src.size());

    //Store id mappings for postProcessEvents
    const StableIDRow* dstIds = dst.tryGet<const StableIDRow>();
    if(Loader::IDRow* idRow = Loader::tryGetDynamicRow<Loader::IDRow>(src); dstIds && idRow) {
      size_t si{};
      for(size_t di : range) {
        idMappings.mappings[idRow->at(si++)] = dstIds->at(di);
      }
    }

    //Read elements row by row
    for(auto [type, row] : src) {
      if(auto loader = mappings.loaders.find(type); loader != mappings.loaders.end()) {
        for(auto&& l : loader->second) {
          l->load(*row, dst, range);
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

  void copySceneDatabase(Loader::ObjIDMappings& idMappings, const Mappings& mappings, RuntimeDatabase& scene) {
    //Lazily clear previous any time new ones are loaded
    idMappings.mappings.clear();

    for(size_t i = 0; i < scene.size(); ++i) {
      RuntimeTable& src = scene[i];
      if constexpr(DEBUG_LOAD) {
        if (const TableName::TableNameRow* name = Loader::tryGetDynamicRow<TableName::TableNameRow>(src)) {
          printf("%s: %d\n", name->at().name.c_str(), static_cast<int>(src.size()));
        }
      }
      if(auto found = mappings.nameHashToTable.find(src.getType().value); found != mappings.nameHashToTable.end() && found->second) {
        copySceneTable(idMappings, mappings, src, *found->second);
      }
    }
  }

  struct ReflectionReader : IReflectionReader {
    ReflectionReader(RuntimeDatabaseTaskBuilder& task)
      : mappings{ task.query<MappingsRow>().tryGetSingletonElement() }
      , ids{ task.query<Loader::SharedObjIDMappingsRow>().tryGetSingletonElement() } {
      // Artificial dependency on entire database as it's accessible through the mappings
      task.getDatabase();
    }

    void loadFromDBIntoGame(RuntimeDatabase& toRead) final {
      if(mappings && ids) {
        copySceneDatabase(*ids, *mappings, toRead);
      }
    }

    const Mappings* mappings{};
    Loader::ObjIDMappings* ids{};
  };

  std::unique_ptr<IReflectionReader> createReader(RuntimeDatabaseTaskBuilder& task) {
    return std::make_unique<ReflectionReader>(task);
  }

  ObjIDLoaderBase::ObjIDLoaderBase(QueryAlias<Loader::PersistentElementRefRow> dstType, std::string_view rowName)
    : srcType{ Loader::getDynamicRowKey<Loader::IDRefRow>(rowName) }
    , dstQuery{ dstType }
    , name{ rowName } {
  }

  DBTypeID ObjIDLoaderBase::getTypeID() const {
    return srcType;
  }

  std::string_view ObjIDLoaderBase::getName() const {
    return name;
  }

  //Load in the raw ObjID and store it in the PersistentElementRef
  void ObjIDLoaderBase::load(const IRow& src, RuntimeTable& dst, gnx::IndexRange range) const {
    Loader::PersistentElementRefRow* dstIds = static_cast<Loader::PersistentElementRefRow*>(dst.tryGet(dstQuery.type));
    const Loader::IDRow& srcIds = static_cast<const Loader::IDRow&>(src);
    size_t si{};
    for(size_t di : range) {
      dstIds->at(di).set(srcIds.at(si++));
    }
  }

  struct InitIDS {
    struct Group {
      void init(QueryAlias<Loader::PersistentElementRefRow> q) {
        queryAlias = q;
      }

      void init(RuntimeDatabaseTaskBuilder& task) {
        events = task.queryAlias(queryAlias.write(), QueryAlias<const Events::EventsRow>::create());
        mappings = task.query<const Loader::SharedObjIDMappingsRow>().tryGetSingletonElement();
      }

      QueryAlias<Loader::PersistentElementRefRow> queryAlias;
      QueryResult<Loader::PersistentElementRefRow, const Events::EventsRow> events;
      const Loader::ObjIDMappings* mappings{};
    };

    void init() {}

    void execute(Group& group) {
      if(!group.mappings) {
        return;
      }
      for(size_t t = 0; t < group.events.size(); ++t) {
        auto [elements, events] = group.events.get(t);
        //For each creation event, resolve its object id
        for(const auto& e : events) {
          if(e.second.isCreate()) {
            Loader::PersistentElementRef& id = elements->at(e.first);
            //Only assign if it hasn't already, which is presumably always, but no harm skipping if it was
            if(Loader::ObjID rawId = id.tryGetID()) {
              //Mapping should exist in a valid scene, if it's missing, leave unset
              if(auto it = group.mappings->mappings.find(rawId); it != group.mappings->mappings.end()) {
                id.set(it->second);
              }
            }
          }
        }
      }
    }
  };

  void ObjIDLoaderBase::postProcessEvents(IAppBuilder& builder) const {
    builder.submitTask(TLSTask::createWithArgs<InitIDS, InitIDS::Group>("resolve ids", dstQuery));
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
    using DB = Database<
      Table<
        Reflection::MappingsRow,
        Loader::SharedObjIDMappingsRow
      >
    >;

    void createDatabase(RuntimeDatabaseArgs& args) final {
      DBReflect::addDatabase<DB>(args);
    }

    void init(IAppBuilder& builder) final {
      builder.submitTask(TLSTask::create<FinalizeTableNames, FinalizeTableNames::Group>("names"));
    }

    void postProcessEvents(IAppBuilder& builder) {
      //Build the post process event tasks for the loaders
      auto temp = builder.createTask();
      const Reflection::Mappings* mappings = temp.query<const Reflection::MappingsRow>().tryGetSingletonElement();
      for(auto&& loader : mappings->loaders) {
        for(auto&& l : loader.second) {
          l->postProcessEvents(builder);
        }
      }
      temp.discard();
    }
  };

  std::unique_ptr<IAppModule> create() {
    return std::make_unique<Module>();
  }
}