#pragma once
namespace Lua {
  class State;

  void printTop(State& state);
  void printGlobal(State& state, const std::string& global);
}