#include "Precompile.h"

#include "ImguiExt.h"
#include "imgui.h"

void ImguiExt::inputSizeT(const char* label, size_t* v, size_t step, size_t step_fast, ImGuiInputTextFlags flags) {
  int temp = static_cast<int>(*v);
  ImGui::InputInt(label, &temp, (int)step, (int)step_fast, flags);
  *v = static_cast<size_t>(temp);
}
