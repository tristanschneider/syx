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
    ButtonResult onButtonUpdate(ImGuiID id) {
      if(auto hook = _getHookSingleton().lock()) {
        return hook->onButtonUpdate(id);
      }
      return ButtonResult::Continue;
    }
  }
}