#pragma once

#include <glm/vec2.hpp>

class IAppModule;
class RuntimeDatabaseTaskBuilder;

namespace PhysicsTestModule {
  struct ValidationError {
    explicit operator bool() const {
      return message.size();
    }

    std::string message;
  };

  struct ValidationStats {
    size_t total() const { return pass + fail; }

    size_t pass{};
    size_t fail{};
  };

  class ILogger {
  public:
    virtual ~ILogger() = default;
    virtual void logValidationError(const ValidationStats& stats, const glm::vec2& pos, ValidationError error) = 0;
    virtual void logValidationSuccess(const ValidationStats& stats) = 0;
    virtual void logResults(const ValidationStats& stats) = 0;
  };
  using LogFactory = std::function<std::unique_ptr<ILogger>(RuntimeDatabaseTaskBuilder&)>;
  LogFactory createDebugLogger();

  std::unique_ptr<IAppModule> create(LogFactory factory = createDebugLogger());
}