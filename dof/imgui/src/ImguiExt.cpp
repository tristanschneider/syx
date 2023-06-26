#include "Precompile.h"

#include "ImguiExt.h"

#include "Config.h"
#include "curve/CurveDefinition.h"
#include "curve/CurveSolver.h"
#include "imgui.h"

namespace {
  bool getCurveType(void*, int index, const char** outName) {
    CurveMath::CurveFunction function = CurveMath::tryGetFunction(static_cast<CurveMath::CurveType>(index));
    if(function.function) {
      *outName = function.name;
      return true;
    }
    *outName = "";
    return false;
  }
}

void ImguiExt::inputSizeT(const char* label, size_t* v, size_t step, size_t step_fast, ImGuiInputTextFlags flags) {
  int temp = static_cast<int>(*v);
  ImGui::InputInt(label, &temp, (int)step, (int)step_fast, flags);
  *v = static_cast<size_t>(temp);
}

bool ImguiExt::optionalSliderFloat(const char* label, std::optional<float>& v, float defaultV, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
  float temp = v.value_or(defaultV);
  if(ImGui::SliderFloat(label, &temp, v_min, v_max, format, flags)) {
    v = temp;
    return true;
  }
  return false;
}

bool ImguiExt::curve(Config::CurveConfigExt& curve, CurveSliders& sliders) {
  return ImguiExt::curve(Config::getCurve(curve), sliders);
}

bool ImguiExt::curve(CurveDefinition& curve, CurveSliders& sliders) {
  if(!ImGui::TreeNode(sliders.label.c_str())) {
    return false;
  }

  bool changed = false;
  std::array<float, 100> plot;
  std::array<float, 100> times;
  const float durationScale = sliders.viewRelative ? 1.0f : (sliders.durationRange.y / std::max(0.001f, curve.params.duration.value_or(1.0f)));
  for(size_t i = 0; i < times.size(); ++i) {
    times[i] = std::min(1.0f, durationScale * static_cast<float>(i) / static_cast<float>(times.size() - 1));
  }
  const CurveSolver::CurveUniforms uniforms{ plot.size() };
  CurveSolver::CurveVaryings varyings{ times.data(), plot.data() };
  CurveSolver::solve(curve, uniforms, varyings);

  const float padding = 0.01f;
  const float maxPlot = sliders.viewRelative ? FLT_MAX : sliders.scaleRange.y + sliders.offsetRange.y + padding;
  const float minPlot = sliders.viewRelative ? FLT_MAX : sliders.scaleRange.y + sliders.offsetRange.x - padding;

  ImGui::PlotLines("Curve", plot.data(), static_cast<int>(plot.size()), 0, nullptr, minPlot, maxPlot, { 100.0f, 100.0f });
  ImGui::SameLine();
  ImGui::Checkbox("Fit to graph", &sliders.viewRelative);

  changed |= ImguiExt::optionalSliderFloat("Scale", curve.params.scale, 1.0f, sliders.scaleRange.x, sliders.scaleRange.y);
  changed |= ImguiExt::optionalSliderFloat("Offset", curve.params.offset, 0.0f, sliders.offsetRange.x, sliders.offsetRange.y);
  changed |= ImguiExt::optionalSliderFloat("Duration", curve.params.duration, 1.0f, sliders.durationRange.x, sliders.durationRange.y);
  int currentCurve = static_cast<int>(curve.function.type);
  if(ImGui::Combo("Curve Function", &currentCurve, &getCurveType, nullptr, static_cast<int>(CurveMath::CurveType::Count))) {
    curve.function = CurveMath::tryGetFunction(static_cast<CurveMath::CurveType>(currentCurve));
    changed = true;
  }
  changed |= ImGui::Checkbox("Flip input", &curve.params.flipInput);
  ImGui::SameLine();
  changed |= ImGui::Checkbox("Flip output", &curve.params.flipOutput);

  ImGui::TreePop();
  return changed;
}