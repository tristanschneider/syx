#pragma once
#include "util/Finally.h"
#include <variant>

//Type specific data or a gui element
struct TestGuiElementData {
  struct Unknown {};
  struct Window {};
  struct Button {};
  struct Text {};
  struct InputText {
    std::string mEditText;
    //No way to copy this for safety, so use at your own peril
    const void* mUserdata = nullptr;
  };
  struct Checkbox {
    bool mValue;
  };
  struct InputFloats {
    std::vector<float> mValues;
  };
  struct InputInts {
    std::vector<int> mValues;
  };

  //Shorthand for data.is<Button>()
  template<class T>
  bool is() const {
    return std::holds_alternative<T>(mVariant);
  }

  template<class T>
  const T* tryGet() const {
    return std::get_if<T>(&mVariant);
  }

  std::string toString() const;

  using Variant = std::variant<Unknown, Window, InputText, Checkbox, InputFloats, InputInts, Button, Text>;
  Variant mVariant;
};

struct ITestGuiQueryContext {
  using VisitCallback = std::function<bool(const ITestGuiQueryContext&)>;

  virtual ~ITestGuiQueryContext() = default;
  virtual const TestGuiElementData& getData() const = 0;
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
  virtual ResetToken addScopedItemHover(const std::string& label) = 0;
  virtual ResetToken addScopedPickerPick(const std::string& pickedItem) = 0;
  virtual std::shared_ptr<ITestGuiQueryContext> query() = 0;
  //Pretty print hierarchy for debugging
  virtual std::string toString(const std::string& tab = "  ", const std::string& newline = "\n") const = 0;
};

namespace Create {
  std::unique_ptr<ITestGuiHook> createTestGuiHook();
  std::shared_ptr<ITestGuiHook> createAndRegisterTestGuiHook();
}