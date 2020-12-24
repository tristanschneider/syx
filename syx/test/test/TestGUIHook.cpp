#include "Precompile.h"
#include "test/TestGUIHook.h"

#include "imgui/imgui.h"
#include "imgui/imgui_ext.h"
#include <optional>

namespace {
  struct UIElement {
    static UIElement createWindow(std::string name) {
      return { std::move(name), TestGuiElementType::Window };
    }

    static UIElement createButton(std::string name) {
      return { std::move(name), TestGuiElementType::Button };
    }

    std::string mName;
    TestGuiElementType mType = TestGuiElementType::Unknown;
    std::vector<std::shared_ptr<UIElement>> mElements;
    std::weak_ptr<UIElement> mParent;
  };

  struct TestGuiQueryContext : public ITestGuiQueryContext {
    TestGuiQueryContext(const UIElement& element)
      : mElement(element) {
    }

    TestGuiElementType getType() const override {
      return mElement.mType;
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

    ImGuiExt::ButtonResult onButtonUpdate(ImGuiID id) override {
      return std::any_of(mPressedButtons.begin(), mPressedButtons.end(), [this, id](const std::string& button) { return doesIDMatch(id, button); })
        ? ImGuiExt::ButtonResult::PerformPress
        : ImGuiExt::ButtonResult::Continue;
    }

    void onButtonCreated(ImGuiID, const char* name) override {
      mScreenTree.push(UIElement::createButton(name));
      mScreenTree.pop();
    }

    void onWindowBegin(ImGuiID, const char* name) override {
      //Force everything open and in a consistent place so it doesn't get clipped, circumventing test logic
      ImGui::SetNextWindowCollapsed(false);
      ImGui::SetNextWindowPos(ImVec2(0, 0));
      ImGui::SetNextWindowSize(ImVec2(100, 100));
      mScreenTree.push(UIElement::createWindow(name));
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