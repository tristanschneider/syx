#pragma once
#include "system/System.h"

class EditorNavigator : public System {
public:
  RegisterSystemH(EditorNavigator);
  using System::System;

  void update(float dt, IWorkerPool& pool, std::shared_ptr<Task> frameTask) override;
};