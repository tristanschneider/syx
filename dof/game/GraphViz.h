#pragma once

struct AppTaskNode;

// Turn into svg:
// dot -Tsvg graph.gv > graph.svg
namespace GraphViz {
  void build(std::ostream& stream, const AppTaskNode& root);
  void writeHere(const std::string& location, const AppTaskNode& root);
}