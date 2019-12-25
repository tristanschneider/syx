#pragma once
#include <imgui/imgui.h>

template<class Pusher, Pusher push, class Popper, Popper pop>
class ScopedImGuiCall {
public:
  template<class... Args>
  ScopedImGuiCall(Args&&... args) {
    push(std::forward<Args>(args)...);
  }
  ~ScopedImGuiCall() {
    pop();
  }
};

namespace scopedImpl {
  void _pushStringId(const char* id) {
    ImGui::PushID(id);
  }

  void _pushPtrId(const void* id) {
    ImGui::PushID(id);
  }

  void _pushIntId(int id) {
    ImGui::PushID(id);
  }
}

#define IMGUI_SCOPED(pusher, popper) ScopedImGuiCall<decltype(pusher), pusher, decltype(popper), popper>
using ScopedStringId = IMGUI_SCOPED(scopedImpl::_pushStringId, ImGui::PopID);
using ScopedPtrId = IMGUI_SCOPED(scopedImpl::_pushPtrId, ImGui::PopID);
using ScopedIntId = IMGUI_SCOPED(scopedImpl::_pushIntId, ImGui::PopID);
