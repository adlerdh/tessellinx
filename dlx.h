#pragma once

#include "shapes.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct DLXNode
{
  DLXNode() : L(this), R(this), U(this), D(this), rowID(-1), colID(-1) {}
  DLXNode *L, *R, *U, *D;
  int rowID;
  int colID;
};

struct ColumnNode : DLXNode
{
  ColumnNode(const std::string& n = "") : DLXNode(), size(0), name(n) {}
  int size;
  std::string name;
};

enum class HeuristicMode { None, LeastFilled };

class DLX
{
public:
  // external control/monitoring hooks
  std::function<void(const std::vector<int>&)> handleSolution;
  std::atomic<uint64_t>* p_nodesVisited = nullptr;
  std::atomic<uint64_t>* p_solutionsFound = nullptr;
  std::atomic<bool>* p_stopFlag = nullptr;

  DLX();
  ~DLX() = default;

  // Setup DLX columns:
  // Columns represent board cells (0..63) + piece usage constraints (one column per piece)
  void setup(const std::vector<Placement>& placements,
             const std::vector<bool>& boardMask,
             int boardWidth, int boardHeight, int numPieces);

  void setHeuristic(HeuristicMode);

  int addColumn(const std::string& name);

  void addRow(int rowID, const std::vector<int>& cols);

  void cover(ColumnNode* c);

  void uncover(ColumnNode* c);

  ColumnNode* chooseColumnNone() const;

  ColumnNode* chooseColumnLeastFilled() const;

  ColumnNode* chooseColumn() const;

  void search(int k = 0);
  void searchWithDebug(int k = 0);

private:
  HeuristicMode m_heuristic = HeuristicMode::None;
  ColumnNode* m_header = nullptr;
  std::vector<ColumnNode*> m_columns;
  std::vector<int> m_solutionRows;
  std::vector<std::unique_ptr<DLXNode>> m_nodePool; // owns all nodes

  ColumnNode* makeColumn(const std::string& name);

  DLXNode* makeNode();
};


// void debugDLX(DLX& dlx, const std::vector<Placement>& placements,
//               int boardWidth, int boardHeight);
