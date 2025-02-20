#pragma once

#include "AppBuilder.h"
#include "RuntimeDatabase.h"

class RuntimeDatabase;
struct AppTaskArgs;
struct StableElementMappings;
struct RuntimeDatabaseArgs;
class IAppBuilder;

namespace StatEffectDatabase {
  template<class RowT>
  RuntimeTable& getStatTable(AppTaskArgs& task) {
    RuntimeDatabase& db = task.getLocalDB();
    auto q = db.query<RowT>();
    assert(q.size());
    return *db.tryGet(q[0]);
  }
};

namespace StatEffect {
  void createDatabase(RuntimeDatabaseArgs& args);

  //Remove all elements of 'from' and put them in 'to'
  //Intended to be used to move newly created thread local effects to the central database
  void moveThreadLocalToCentral(IAppBuilder& builder);
  void createTasks(IAppBuilder& builder);
  void init(IAppBuilder& builder);
};