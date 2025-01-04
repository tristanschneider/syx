#pragma once

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
};