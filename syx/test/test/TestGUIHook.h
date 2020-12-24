#pragma once
#include "util/Finally.h"

enum class TestGuiElementType : uint8_t {
  Unknown,
  Window,
  Button,
};

struct ITestGuiQueryContext {
  using VisitCallback = std::function<bool(const ITestGuiQueryContext&)>;

  virtual ~ITestGuiQueryContext() = default;
  virtual TestGuiElementType getType() const = 0;
  virtual const std::string& getName() const = 0;
  virtual void visitChildrenShallow(const VisitCallback& callback) const = 0;
  //Depth first
  virtual void visitChildrenRecursive(const VisitCallback& callback) const = 0;
};

struct ITestGuiHook {
  using ResetToken = FinalAction<std::function<void()>>;

  virtual ~ITestGuiHook() = default;
  virtual void addPressedButton(const std::string& label) = 0;
  virtual void removePressedButton(const std::string& label) = 0;
  virtual ResetToken addScopedButtonPress(const std::string& label) = 0;
  virtual std::shared_ptr<ITestGuiQueryContext> query() = 0;
};

namespace Create {
  std::unique_ptr<ITestGuiHook> createTestGuiHook();
  std::shared_ptr<ITestGuiHook> createAndRegisterTestGuiHook();
}