#include "Precompile.h"
#include "test/TestGUIHook.h"

#include "imgui/imgui.h"
#include "imgui/imgui_ext.h"
#include <optional>

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

std::string TestGuiElementData::toString() const {
  auto printValues = [](const char* typeName, const auto& type) {
    std::string result = std::string(typeName) + "[ ";
    for(size_t i = 0; i < type.mValues.size(); ++i) {
      result += i ? ", " : "";
      result += std::to_string(type.mValues[i]);
    }
    result += " ]";
    return result;
  };

  std::string result = std::visit(overloaded{
    [](const Unknown&) { return std::string("Unknown"); },
    [](const Window&) { return std::string("Window"); },
    [](const InputText& input) { return "InputText [ " + input.mEditText + ", " + std::to_string(reinterpret_cast<size_t>(input.mUserdata)) + " ]"; },
    [](const Checkbox& checkbox) { return "Checkbox [" + std::string(checkbox.mValue ? "true" : "false") + " ]"; },
    [&printValues](const InputFloats& floats) { return printValues("InputFloats", floats); },
    [&printValues](const InputInts& ints) { return printValues("InputInts", ints); },
    [](const Button&) { return std::string("Button"); },
    [](const Text&) { return std::string("Text"); }
  }, mVariant);
  return result;
}

namespace {
  struct UIElement {
    std::string mName;
    TestGuiElementData mData;
    std::vector<std::shared_ptr<UIElement>> mElements;
    std::weak_ptr<UIElement> mParent;
  };

  struct TestGuiQueryContext : public ITestGuiQueryContext {
    TestGuiQueryContext(const UIElement& element)
      : mElement(element) {
    }

    const TestGuiElementData& getData() const override {
      return mElement.mData;
    }

    const std::string& getName() const override {
      return mElement.mName;
    }

    void visitChildrenShallow(const VisitCallback& callback) const {
      for(const auto& child : mElement.mElements) {
        if(!callback(TestGuiQueryContext(*child))) {
          return;
        }
      }
    }

    bool _visitChildrenRecursive(const VisitCallback& callback) const {
      for(const auto& child : mElement.mElements) {
        const TestGuiQueryContext childContext(*child);
        if(!callback(childContext)) {
          return false;
        }
        if(!childContext._visitChildrenRecursive(callback)) {
          return false;
        }
      }
      return true;
    }

    void visitChildrenRecursive(const VisitCallback& callback) const {
      _visitChildrenRecursive(callback);
    }

  private:
    const UIElement& mElement;
  };


  //A screen tree that preserves the history of a screen stack as it is built
  //A new tree begins when an element is pushed at the base depth, assuming that a new frame has begun
  class PreservingScreenTree {
  public:
    void push(UIElement element) {
      //If the screen has been popped all the way and a new element is being pushed, this must be the start of a new stack, clear the old one
      if(auto currentElement = mCurrentElement.lock()) {
        auto newElement = std::make_shared<UIElement>(std::move(element));
        newElement->mParent = currentElement;
        mCurrentElement = newElement;
        currentElement->mElements.push_back(std::move(newElement));
      }
      else {
        mRoot = std::make_shared<UIElement>(std::move(element));
        mCurrentElement = mRoot;
      }
    }

    void pop() {
      //It is expected for this to lead to a null mCurrentElement upon popping the topmost stack element
      if(auto currentElement = mCurrentElement.lock()) {
        mCurrentElement = currentElement->mParent;
      }
    }

    void clear() {
      mRoot = nullptr;
      mCurrentElement = {};
    }

    const UIElement* tryGetRoot() const {
      return mRoot.get();
    }

  private:
    //Root of the tree
    std::shared_ptr<UIElement> mRoot;
    //Current location in the tree based on push/pop stack calls
    std::weak_ptr<UIElement> mCurrentElement;
  };

  struct TestGuiHook : public ITestGuiHook, public ImGuiExt::TestHook {
    void addPressedButton(const std::string& label) override {
      mPressedButtons.push_back(label);
    }

    void removePressedButton(const std::string& label) override {
      mPressedButtons.erase(std::remove(mPressedButtons.begin(), mPressedButtons.end(), label), mPressedButtons.end());
    }

    ResetToken addScopedButtonPress(const std::string& label) override {
      addPressedButton(label);
      return finally(std::function<void()>([this, label] {
        removePressedButton(label);
      }));
    }

    std::shared_ptr<ITestGuiQueryContext> query() override {
      if(const UIElement* root = mScreenTree.tryGetRoot()) {
        return std::make_shared<TestGuiQueryContext>(*root);
      }
      return nullptr;
    }

    static void _recurseToString(const UIElement& current, std::string depthSpacer, const std::string& tab, const std::string& newline, std::string& result) {
      //Tab this in
      result += depthSpacer;
      //Print this
      result += current.mName + " (" + current.mData.toString() + ")" + newline;

      //Recurse into children
      depthSpacer += tab;
      for(const auto& child : current.mElements) {
        if(child) {
          _recurseToString(*child, depthSpacer, tab, newline, result);
        }
      }
    }

    std::string toString(const std::string& tab, const std::string& newline) const override {
      std::string result;
      if(const UIElement* root = mScreenTree.tryGetRoot()) {
        _recurseToString(*root, "", tab, newline, result);
      }
      else {
        result = "(Empty)";
      }
      return result;
    }

    ImGuiExt::ButtonResult onButtonUpdate(ImGuiID id) override {
      return std::any_of(mPressedButtons.begin(), mPressedButtons.end(), [this, id](const std::string& button) { return doesIDMatch(id, button); })
        ? ImGuiExt::ButtonResult::PerformPress
        : ImGuiExt::ButtonResult::Continue;
    }

    void onButtonCreated(ImGuiID, const char* name) override {
      mScreenTree.push({ name, TestGuiElementData::Button() });
      mScreenTree.pop();
    }

    void onTextCreated(const char* text) override {
      mScreenTree.push({ text, TestGuiElementData::Text() });
      mScreenTree.pop();
    }

    void onInputTextCreated(const char* label, std::string_view buffer, const void* userdata) override {
      //Only take the cstring portion of the buffer, discard the unused space behind it
      mScreenTree.push({ label, TestGuiElementData::InputText{ std::string(buffer.data(), std::strlen(buffer.data())), userdata } });
      mScreenTree.pop();
    }

    void onCheckboxCreated(const char* label, bool value) override {
      mScreenTree.push({ label, TestGuiElementData::Checkbox{ value } });
      mScreenTree.pop();
    }

    void onInputFloatsCreated(const char* label, const float* elements, size_t elementCount) override {
      std::vector<float> values;
      values.reserve(elementCount);
      for(size_t i = 0; i < elementCount; ++i) {
        values.push_back(elements[i]);
      }
      mScreenTree.push({ label, TestGuiElementData::InputFloats{ std::move(values) } });
      mScreenTree.pop();
    }

    void onInputIntsCreated(const char* label, const int* elements, size_t elementCount) override {
      std::vector<int> values;
      values.reserve(elementCount);
      for(size_t i = 0; i < elementCount; ++i) {
        values.push_back(elements[i]);
      }
      mScreenTree.push({ label, TestGuiElementData::InputInts{ std::move(values) } });
      mScreenTree.pop();
    }

    void onWindowBegin(ImGuiID, const char* name) override {
      //Force everything open and in a consistent place so it doesn't get clipped, circumventing test logic
      ImGui::SetNextWindowCollapsed(false);
      ImGui::SetNextWindowPos(ImVec2(0, 0));
      ImGui::SetNextWindowSize(ImVec2(100, 100));
      mScreenTree.push({ name, TestGuiElementData::Window() });
    }

    void onWindowEnd() override {
      mScreenTree.pop();
    }

    ImGuiExt::HookBoolResult shouldClip(ImGuiID) override {
      //Don't clip anything while the test hook is active, clipping may prevent the intended gui logic from being triggered
      return ImGuiExt::HookBoolResult::ForceFalse;
    }

    std::vector<std::string> mPressedButtons;
    PreservingScreenTree mScreenTree;
  };
}

namespace Create {
  std::unique_ptr<ITestGuiHook> createTestGuiHook() {
    return std::make_unique<TestGuiHook>();
  }

  std::shared_ptr<ITestGuiHook> createAndRegisterTestGuiHook() {
    auto result = std::make_shared<TestGuiHook>();
    ImGuiExt::registerTestHook(result);
    return result;
  }
}