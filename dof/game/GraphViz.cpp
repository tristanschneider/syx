#include "Precompile.h"
#include "GraphViz.h"

#include "AppBuilder.h"

namespace GraphViz {
  void build(std::ostream& stream, const AppTaskNode& root) {
    stream << R"(
      digraph mygraph {
        fontname="Helvetica,Arial,sans-serif"
        node [fontname="Helvetica,Arial,sans-serif"]
        edge [fontname="Helvetica,Arial,sans-serif"]
        node [shape=box];
    )";
    std::unordered_set<const AppTaskNode*> visited;
    std::vector<const AppTaskNode*> todo;
    todo.push_back(&root);
    while(todo.size()) {
      const AppTaskNode* current = todo.back();
      todo.pop_back();
      if(!visited.insert(current).second) {
        continue;
      }

      for(const auto& child : current->children) {
        stream << "  \"" << current->name << "\" -> \"" << child->name << "\"" << std::endl;
        todo.push_back(child.get());
      }
    }

    stream << "}";
  }

  void writeHere(const std::string& location, const AppTaskNode& root) {
    std::ofstream stream(location, std::ios::out);
    if(stream.good()) {
      build(stream, root);
      stream.flush();
    }
  }
}