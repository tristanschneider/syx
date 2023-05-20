#pragma once

#include <glm/vec2.hpp>

typedef int ImGuiInputTextFlags;
typedef int ImGuiSliderFlags;

struct CurveDefinition;

namespace ImguiExt {
  struct CurveSliders {
    std::string label;
    glm::vec2 offsetRange{ -1.0f, 1.0f };
    glm::vec2 scaleRange{ 0.0f, 1.0f };
    glm::vec2 durationRange{ 0.0f, 5.0f };
    bool viewRelative{};
  };

  void inputSizeT(const char* label, size_t* v, size_t step = 1, size_t step_fast = 100, ImGuiInputTextFlags flags = 0);
  bool optionalSliderFloat(const char* label, std::optional<float>& v, float defaultV, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
  void curve(CurveDefinition& curve, CurveSliders& sliders);
};