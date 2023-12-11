#include "Precompile.h"
#include "CppUnitTest.h"

#include "IslandGraph.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Test {
  TEST_CLASS(IslandGraphTest) {
    TEST_METHOD(Base) {
      IslandGraph::Graph graph;
      Assert::IsTrue(graph.begin() == graph.end());

      const IslandGraph::NodeUserdata nodeA{ 1 };
      const IslandGraph::NodeUserdata nodeB{ 2 };
      const IslandGraph::EdgeUserdata edgeAB{ 3 };
      //A
      //|
      //B
      IslandGraph::addNode(graph, nodeA);
      IslandGraph::addNode(graph, nodeB);
      IslandGraph::addEdge(graph, nodeA, nodeB, edgeAB);
      IslandGraph::rebuildIslands(graph);

      Assert::AreEqual(size_t(1), graph.end() - graph.begin());

      auto it = graph.begin();
      Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeA) != it.endNodes());
      Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeB) != it.endNodes());
      Assert::AreEqual(edgeAB, *it.beginEdges());

      const IslandGraph::NodeUserdata nodeC{ 4 };
      const IslandGraph::NodeUserdata nodeD{ 5 };
      const IslandGraph::EdgeUserdata edgeCD{ 6 };
      //A   C
      //|   |
      //B   D
      IslandGraph::addNode(graph, nodeC);
      IslandGraph::addNode(graph, nodeD);
      IslandGraph::addEdge(graph, nodeC, nodeD, edgeCD);
      IslandGraph::rebuildIslands(graph);

      Assert::AreEqual(size_t(2), graph.end() - graph.begin());
      it = graph.begin() + 1;
      Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeC) != it.endNodes());
      Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeD) != it.endNodes());
      Assert::AreEqual(edgeCD, *it.beginEdges());

      //A---C
      //|   |
      //B   D
      const IslandGraph::EdgeUserdata edgeAC{ 7 };
      IslandGraph::addEdge(graph, nodeA, nodeC, edgeAC);
      IslandGraph::rebuildIslands(graph);
      int nonEmpty{};
      for(it = graph.begin(); it != graph.end(); ++it) {
        if(it->size()) {
          ++nonEmpty;
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeA) != it.endNodes());
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeB) != it.endNodes());
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeC) != it.endNodes());
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeD) != it.endNodes());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAB) != it.endEdges());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAC) != it.endEdges());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeCD) != it.endEdges());
        }
      }
      Assert::AreEqual(1, nonEmpty);

      //A--C
      //|\ |
      //B \|
      //   D
      const IslandGraph::EdgeUserdata edgeAD{ 8 };
      IslandGraph::addEdge(graph, nodeA, nodeD, edgeAD);
      IslandGraph::rebuildIslands(graph);
      nonEmpty = 0;
      for(it = graph.begin(); it != graph.end(); ++it) {
        if(it->size()) {
          ++nonEmpty;
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeA) != it.endNodes());
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeB) != it.endNodes());
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeC) != it.endNodes());
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeD) != it.endNodes());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAB) != it.endEdges());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAC) != it.endEdges());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeCD) != it.endEdges());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAD) != it.endEdges());
        }
      }
      Assert::AreEqual(1, nonEmpty);

      //A--C
      //|\ |
      //| \|
      //B  D
      //|
      //E
      const IslandGraph::NodeUserdata nodeE{ 9 };
      const IslandGraph::EdgeUserdata edgeBE{ 10 };
      IslandGraph::addNode(graph, nodeE);
      IslandGraph::addEdge(graph, nodeB, nodeE, edgeBE);
      IslandGraph::rebuildIslands(graph);
      nonEmpty = 0;
      for(it = graph.begin(); it != graph.end(); ++it) {
        if(it->size()) {
          ++nonEmpty;
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeA) != it.endNodes());
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeB) != it.endNodes());
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeC) != it.endNodes());
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeD) != it.endNodes());
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeE) != it.endNodes());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAB) != it.endEdges());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAC) != it.endEdges());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeCD) != it.endEdges());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAD) != it.endEdges());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeBE) != it.endEdges());
        }
      }
      Assert::AreEqual(1, nonEmpty);

      //A--C
      // \ |
      //  \|
      //B  D
      //|
      //E
      IslandGraph::removeEdge(graph, nodeA, nodeB);
      IslandGraph::rebuildIslands(graph);
      nonEmpty = 0;
      bool foundBE{};
      bool foundACD{};
      for(it = graph.begin(); it != graph.end(); ++it) {
        if(it->size()) {
          if(std::find(it.beginEdges(), it.endEdges(), edgeBE) != it.endEdges()) {
            Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeB) != it.endNodes());
            Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeE) != it.endNodes());

            Assert::IsFalse(std::find(it.beginNodes(), it.endNodes(), nodeA) != it.endNodes());
            Assert::IsFalse(std::find(it.beginNodes(), it.endNodes(), nodeC) != it.endNodes());
            Assert::IsFalse(std::find(it.beginNodes(), it.endNodes(), nodeD) != it.endNodes());
            Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeAC) != it.endEdges());
            Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeCD) != it.endEdges());
            Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeAD) != it.endEdges());

            foundBE = true;
          }
          else {
            foundACD = true;
            Assert::IsFalse(std::find(it.beginNodes(), it.endNodes(), nodeB) != it.endNodes());
            Assert::IsFalse(std::find(it.beginNodes(), it.endNodes(), nodeE) != it.endNodes());

            Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeA) != it.endNodes());
            Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeC) != it.endNodes());
            Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeD) != it.endNodes());
            Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeAB) != it.endEdges());
            Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAC) != it.endEdges());
            Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeCD) != it.endEdges());
            Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAD) != it.endEdges());
            Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeBE) != it.endEdges());
          }
        }
      }
      Assert::IsTrue(foundBE);
      Assert::IsTrue(foundACD);

      //A--C
      //   |
      //   |
      //B  D
      //|
      //E
      IslandGraph::removeEdge(graph, nodeA, nodeD);
      IslandGraph::rebuildIslands(graph);
      foundBE = false;
      foundACD = false;
      for(it = graph.begin(); it != graph.end(); ++it) {
        if(it->size()) {
          if(std::find(it.beginEdges(), it.endEdges(), edgeBE) != it.endEdges()) {
            Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeB) != it.endNodes());
            Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeE) != it.endNodes());

            Assert::IsFalse(std::find(it.beginNodes(), it.endNodes(), nodeA) != it.endNodes());
            Assert::IsFalse(std::find(it.beginNodes(), it.endNodes(), nodeC) != it.endNodes());
            Assert::IsFalse(std::find(it.beginNodes(), it.endNodes(), nodeD) != it.endNodes());
            Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeAC) != it.endEdges());
            Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeCD) != it.endEdges());
            Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeAD) != it.endEdges());

            foundBE = true;
          }
          else {
            foundACD = true;
            Assert::IsFalse(std::find(it.beginNodes(), it.endNodes(), nodeB) != it.endNodes());
            Assert::IsFalse(std::find(it.beginNodes(), it.endNodes(), nodeE) != it.endNodes());

            Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeA) != it.endNodes());
            Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeC) != it.endNodes());
            Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeD) != it.endNodes());
            Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeAB) != it.endEdges());
            Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAC) != it.endEdges());
            Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeCD) != it.endEdges());
            Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeAD) != it.endEdges());
            Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeBE) != it.endEdges());
          }
        }
      }
      Assert::IsTrue(foundBE);
      Assert::IsTrue(foundACD);

      //A--C
      //   |
      //   |
      //B  D
      //
      //E
      IslandGraph::removeEdge(graph, nodeB, nodeE);
      IslandGraph::rebuildIslands(graph);
      bool foundB{};
      bool foundE{};
      foundACD = false;
      for(it = graph.begin(); it != graph.end(); ++it) {
        if(it->size()) {
          if(std::find(it.beginNodes(), it.endNodes(), nodeB) != it.endNodes()) {
            foundB = true;
            Assert::IsTrue(it.beginEdges() == it.endEdges());
          }
          else if(std::find(it.beginNodes(), it.endNodes(), nodeE) != it.endNodes()) {
            foundE = true;
            Assert::IsTrue(it.beginEdges() == it.endEdges());
          }
          else {
            foundACD = true;
            Assert::IsFalse(std::find(it.beginNodes(), it.endNodes(), nodeB) != it.endNodes());
            Assert::IsFalse(std::find(it.beginNodes(), it.endNodes(), nodeE) != it.endNodes());

            Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeA) != it.endNodes());
            Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeC) != it.endNodes());
            Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeD) != it.endNodes());
            Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAC) != it.endEdges());
            Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeCD) != it.endEdges());
          }
        }
      }
      Assert::IsTrue(foundB);
      Assert::IsTrue(foundE);
      Assert::IsTrue(foundACD);

      //A  
      //   
      //   
      //B  D
      //
      //E
      IslandGraph::removeNode(graph, nodeC);
      IslandGraph::rebuildIslands(graph);
      bool foundA{};
      foundB = false;
      bool foundD{};
      foundE = false;
      for(it = graph.begin(); it != graph.end(); ++it) {
        if(it->size()) {
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeC) == it.endNodes());
          if(std::find(it.beginNodes(), it.endNodes(), nodeA) != it.endNodes()) {
            foundA = true;
            Assert::IsTrue(it.beginEdges() == it.endEdges());
          }
          else if(std::find(it.beginNodes(), it.endNodes(), nodeB) != it.endNodes()) {
            foundB = true;
            Assert::IsTrue(it.beginEdges() == it.endEdges());
          }
          else if(std::find(it.beginNodes(), it.endNodes(), nodeD) != it.endNodes()) {
            foundD = true;
            Assert::IsTrue(it.beginEdges() == it.endEdges());
          }
          else if(std::find(it.beginNodes(), it.endNodes(), nodeE) != it.endNodes()) {
            foundE = true;
            Assert::IsTrue(it.beginEdges() == it.endEdges());
          }
        }
      }
      Assert::IsTrue(foundA);
      Assert::IsTrue(foundB);
      Assert::IsTrue(foundD);
      Assert::IsTrue(foundE);

      //A  
      //|  
      //B--D
      //|
      //E
      const IslandGraph::EdgeUserdata edgeBD{ 11 };
      IslandGraph::addEdge(graph, nodeA, nodeB, edgeAB);
      IslandGraph::addEdge(graph, nodeB, nodeE, edgeBE);
      IslandGraph::addEdge(graph, nodeB, nodeD, edgeBD);
      IslandGraph::rebuildIslands(graph);
      nonEmpty = 0;
      for(it = graph.begin(); it != graph.end(); ++it) {
        if(it->size()) {
          ++nonEmpty;
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeA) != it.endNodes());
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeB) != it.endNodes());
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeD) != it.endNodes());
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeE) != it.endNodes());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAB) != it.endEdges());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeBE) != it.endEdges());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeBD) != it.endEdges());
        }
      }
      Assert::AreEqual(1, nonEmpty);

      //A
      //|
      //B
      //|
      //E
      IslandGraph::removeNode(graph, nodeD);
      IslandGraph::rebuildIslands(graph);
      nonEmpty = 0;
      for(it = graph.begin(); it != graph.end(); ++it) {
        if(it->size()) {
          ++nonEmpty;
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeA) != it.endNodes());
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeB) != it.endNodes());
          Assert::IsFalse(std::find(it.beginNodes(), it.endNodes(), nodeD) != it.endNodes());
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeE) != it.endNodes());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAB) != it.endEdges());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeBE) != it.endEdges());
          Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeBD) != it.endEdges());
        }
      }
      Assert::AreEqual(1, nonEmpty);

      //A
      //|
      //B
      //
      //E
      IslandGraph::removeEdge(graph, nodeB, nodeE);
      IslandGraph::rebuildIslands(graph);
      bool foundAB{};
      foundE = false;
      for(it = graph.begin(); it != graph.end(); ++it) {
        Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeBE) != it.endEdges());
        if(std::find(it.beginNodes(), it.endNodes(), nodeA) != it.endNodes()) {
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeB) != it.endNodes());
          Assert::IsFalse(std::find(it.beginNodes(), it.endNodes(), nodeE) != it.endNodes());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAB) != it.endEdges());
          foundAB = true;
        }
        else {
          foundE = true;
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeE) != it.endNodes());
          Assert::IsTrue(it.beginEdges() == it.endEdges());
        }
      }
      Assert::IsTrue(foundAB);
      Assert::IsTrue(foundE);

      //A
      //|
      //B
      IslandGraph::removeNode(graph, nodeE);
      IslandGraph::rebuildIslands(graph);
      nonEmpty = 0;
      for(it = graph.begin(); it != graph.end(); ++it) {
        Assert::IsFalse(std::find(it.beginNodes(), it.endNodes(), nodeE) != it.endNodes());
        if(it->size()) {
          ++nonEmpty;
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeA) != it.endNodes());
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeB) != it.endNodes());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAB) != it.endEdges());
        }
      }
      Assert::AreEqual(1, nonEmpty);

      //Empty
      IslandGraph::removeNode(graph, nodeA);
      IslandGraph::removeNode(graph, nodeB);
      IslandGraph::rebuildIslands(graph);
      for(it = graph.begin(); it != graph.end(); ++it) {
        Assert::IsTrue(it.beginNodes() == it.endNodes());
        Assert::IsTrue(it.beginEdges() == it.endEdges());
      }
    }

    TEST_METHOD(Propagation) {
      IslandGraph::Graph graph;
      size_t k{};
      const IslandGraph::NodeUserdata nodeA{ ++k };
      const IslandGraph::NodeUserdata staticB{ ++k };
      const IslandGraph::NodeUserdata nodeC{ ++k };
      const IslandGraph::EdgeUserdata edgeAB{ ++k };
      const IslandGraph::EdgeUserdata edgeBC{ ++k };
      //A-(B)-C
      IslandGraph::addNode(graph, nodeA);
      IslandGraph::addNode(graph, staticB, IslandGraph::PROPAGATE_NONE);
      IslandGraph::addNode(graph, nodeC);
      IslandGraph::addEdge(graph, nodeA, staticB, edgeAB);
      IslandGraph::addEdge(graph, staticB, nodeC, edgeBC);
      IslandGraph::rebuildIslands(graph);

      {
        bool foundA{};
        bool foundC{};
        for(auto it = graph.begin(); it != graph.end(); ++it) {
          if(std::find(it.beginNodes(), it.endNodes(), nodeA) != it.endNodes()) {
            foundA = true;
            Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAB) != it.endEdges());
            Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeBC) != it.endEdges());
          }
          else if(std::find(it.beginNodes(), it.endNodes(), nodeC) != it.endNodes()) {
            foundC = true;
            Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeAB) != it.endEdges());
            Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeBC) != it.endEdges());
          }
          //I don't care one way or the other if B gets its own island
        }
        Assert::IsTrue(foundA);
        Assert::IsTrue(foundC);
      }

      //A-(B)-C
      //   |
      //  (D)
      const IslandGraph::NodeUserdata staticD{ ++k };
      const IslandGraph::EdgeUserdata edgeBD{ ++k };
      IslandGraph::addNode(graph, staticD, IslandGraph::PROPAGATE_NONE);
      IslandGraph::addEdge(graph, staticB, staticD, edgeBD);
      IslandGraph::rebuildIslands(graph);
      {
        bool foundA{};
        bool foundC{};
        for(auto it = graph.begin(); it != graph.end(); ++it) {
          //Edges that don't cause island propagation don't show up in the island, but can still be found in the graph
          Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeBD) != it.endEdges());
          Assert::IsFalse(std::find(it.beginNodes(), it.endNodes(), staticD) != it.endNodes());
          if(std::find(it.beginNodes(), it.endNodes(), nodeA) != it.endNodes()) {
            foundA = true;
            Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAB) != it.endEdges());
            Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeBC) != it.endEdges());
          }
          else if(std::find(it.beginNodes(), it.endNodes(), nodeC) != it.endNodes()) {
            foundC = true;
            Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeAB) != it.endEdges());
            Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeBC) != it.endEdges());
          }
          //I don't care one way or the other if B gets its own island
        }
        Assert::IsTrue(foundA);
        Assert::IsTrue(foundC);
        Assert::IsTrue(graph.findEdge(staticB, staticD) != graph.edgesEnd());
      }

      //A---C
      // \ /
      // (B)
      //  |
      // (D)
      const IslandGraph::EdgeUserdata edgeAC{ ++k };
      IslandGraph::addEdge(graph, nodeA, nodeC, edgeAC);
      IslandGraph::rebuildIslands(graph);
      {
        bool found{};
        for(auto it = graph.begin(); it != graph.end(); ++it) {
          found = true;
          Assert::IsFalse(std::find(it.beginEdges(), it.endEdges(), edgeBD) != it.endEdges());
          Assert::IsFalse(std::find(it.beginNodes(), it.endNodes(), staticD) != it.endNodes());

          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeA) != it.endNodes());
          Assert::IsTrue(std::find(it.beginNodes(), it.endNodes(), nodeC) != it.endNodes());

          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAB) != it.endEdges());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeBC) != it.endEdges());
          Assert::IsTrue(std::find(it.beginEdges(), it.endEdges(), edgeAC) != it.endEdges());
        }
        Assert::IsTrue(found);
        Assert::IsTrue(graph.findEdge(staticB, staticD) != graph.edgesEnd());
      }
    }

    TEST_METHOD(NodeReuse) {
      IslandGraph::Graph graph;
      // 1
      IslandGraph::addNode(graph, 1);
      // 1 3
      IslandGraph::addNode(graph, 3);
      // 1-3
      IslandGraph::addEdge(graph, 1, 3, 0);
      // 3
      IslandGraph::removeNode(graph, 1);
      // 1 3
      IslandGraph::addNode(graph, 1);
      // 1 2 3
      IslandGraph::addNode(graph, 2);
      Assert::IsTrue(graph.edgesEnd() == graph.findEdge(1, 2));
    }
  };
}