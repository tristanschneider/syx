#pragma once

#include "Renderer.h"
#include "Simulation.h"

struct ImguiImpl;

struct ImguiData {
  ImguiData();
  ~ImguiData();
  std::unique_ptr<ImguiImpl> mImpl;
};

struct ImguiModule {
  static void update(ImguiData& data, GameDatabase& db, RendererDatabase& renderDB);
};