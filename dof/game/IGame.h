#pragma once

struct AppTaskArgs;
struct IDatabase;

//This is an abstraction to bundle game related logic to minimize the amount of logic in the platform project.
//It is also for reusability in tests without needing to initialize rendering
//The interface here is meant to expose the minimal amount, although realistically perhaps it is always used via GameImpl
class IGame {
public:
  virtual ~IGame() = default;
  virtual void init() = 0;
  virtual void updateRendering() = 0;
  virtual void updateSimulation() = 0;
  virtual IDatabase& getDatabase() = 0;
  //Exposed for odd cases where something outside of the main tick calls into something that
  //requires AppTaskArgs, like tests. Should not be used during the tick while other threads might be using these locals
  virtual std::unique_ptr<AppTaskArgs> createAppTaskArgs(size_t threadIndex = 0) = 0;
};