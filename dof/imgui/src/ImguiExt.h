#pragma once

typedef int ImGuiInputTextFlags;

struct ImguiExt {
  static void inputSizeT(const char* label, size_t* v, size_t step = 1, size_t step_fast = 100, ImGuiInputTextFlags flags = 0);
};