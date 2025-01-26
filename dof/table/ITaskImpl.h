#pragma once

#include <variant>

class RuntimeDatabaseTaskBuilder;
struct AppTaskMetadata;
class RuntimeDatabase;
struct AppTaskArgs;
struct AppTaskConfig;

namespace AppTaskPinning {
  //No restrictions, meaning task may run on any thread
  struct None {};
  //Executes on main thread, other tasks can still run on other threads
  struct MainThread {};
  //Nothing else will be scheduled in parallel with this
  struct Synchronous {};
  struct ThreadID {
    uint8_t id{};
  };
  using Variant = std::variant<None, MainThread, Synchronous, ThreadID>;
};

class ITaskImpl {
public:
  virtual ~ITaskImpl() = default;
  virtual void setWorkerCount(size_t count) = 0;
  virtual AppTaskMetadata init(RuntimeDatabase& db) = 0;
  virtual void initThreadLocal(AppTaskArgs& args) = 0;
  virtual void execute(AppTaskArgs& args) = 0;
  virtual std::shared_ptr<AppTaskConfig> getConfig() = 0;
  virtual AppTaskPinning::Variant getPinning() = 0;
};