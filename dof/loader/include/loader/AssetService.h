#pragma once

class IAppBuilder;
struct IDatabase;
struct StableElementMappings;
class ElementRef;
struct AppTaskArgs;
struct RuntimeDatabaseArgs;

namespace Loader {
  struct Events {
    using CB = void(*)(const ElementRef&, AppTaskArgs&);
    CB notifyCreate{};
    CB requestDestroy{};
  };

  void createDB(RuntimeDatabaseArgs& args);
  void processRequests(IAppBuilder& builder, const Events& events);
}
