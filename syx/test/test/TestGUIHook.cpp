#include "Precompile.h"
#include "test/TestGUIHook.h"

#include "imgui/imgui_ext.h"

namespace {
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

    ImGuiExt::ButtonResult onButtonUpdate(ImGuiID id) {
      return std::any_of(mPressedButtons.begin(), mPressedButtons.end(), [this, id](const std::string& button) { return doesIDMatch(id, button); })
        ? ImGuiExt::ButtonResult::PerformPress
        : ImGuiExt::ButtonResult::Continue;
    }

    virtual ImGuiExt::HookBoolResult shouldClip(ImGuiID) override {
      // Don't clip anything while the test hook is active, clipping may prevent the intended gui logic from being triggered
      return ImGuiExt::HookBoolResult::ForceFalse;
    }

    std::vector<std::string> mPressedButtons;
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