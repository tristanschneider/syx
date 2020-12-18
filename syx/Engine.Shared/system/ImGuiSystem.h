#pragma once
#include "system/System.h"

struct IImGuiImpl;

struct IImGuiSystem : public System {
  virtual ~IImGuiSystem() = default;
  IImGuiSystem(const SystemArgs& args);

  virtual IImGuiImpl* _getImpl() = 0;
};

class ImGuiSystem : public IImGuiSystem {
public:
  ImGuiSystem(const SystemArgs& args);
  ~ImGuiSystem();

  void init() override;
  void queueTasks(float, IWorkerPool&, std::shared_ptr<Task>) override;
  void uninit() override;

  IImGuiImpl* _getImpl() override;

private:
  std::unique_ptr<IImGuiImpl> mImpl;
};