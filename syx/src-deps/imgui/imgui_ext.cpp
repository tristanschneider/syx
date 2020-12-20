#include "imgui_ext.h"

#include "imgui.h"
#include "imgui_internal.h"

namespace ImGuiExt {
  //Wrap in function to make initialization order well defined even if this is called as part of static initialization
  std::weak_ptr<TestHook>& _getHookSingleton() {
    static std::weak_ptr<TestHook> hook;
    return hook;
  }

  bool TestHook::doesIDMatch(ImGuiID id, std::string_view label) const {
    //This matches what imgui does to turn labels into ids. The window should exist because this is being called from imgui logic that presumably just did a similar window lookup
    if(ImGuiWindow* window = ImGui::GetCurrentWindow()) {
      return window->GetIDNoKeepAlive(label.data(), nullptr) == id;
    }
    return false;
  }

  void registerTestHook(std::weak_ptr<TestHook> hook) {
    _getHookSingleton() = hook;
  }

  namespace Hook {
    template<class MemberFunc, class ReturnType, class... Args>
    auto _forwardCall(const MemberFunc& func, const ReturnType& defaultValue, Args&&... args) {
      if(auto hook = _getHookSingleton().lock()) {
        return std::invoke(func, hook.get(), std::forward<Args>(args)...);
      }
      return defaultValue;
    }

    ButtonResult onButtonUpdate(ImGuiID id) {
      return _forwardCall(&TestHook::onButtonUpdate, ButtonResult::Continue, id);
    }

    HookBoolResult shouldClip(ImGuiID id) {
      return _forwardCall(&TestHook::shouldClip, HookBoolResult::Continue, id);
    }
  }
}