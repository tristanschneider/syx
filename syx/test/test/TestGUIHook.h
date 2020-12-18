#pragma once
#include "util/Finally.h"

struct ITestGuiHook {
  using ResetToken = FinalAction<std::function<void()>>;

  virtual ~ITestGuiHook() = default;
  virtual void addPressedButton(const std::string& label) = 0;
  virtual void removePressedButton(const std::string& label) = 0;
  virtual ResetToken addScopedButtonPress(const std::string& label) = 0;
};

namespace Create {
  std::unique_ptr<ITestGuiHook> createTestGuiHook();
  std::shared_ptr<ITestGuiHook> createAndRegisterTestGuiHook();
}