#include "dlx.h"

#include <iostream>

DLX::DLX()
{
  m_header = makeColumn("header");
  m_header->L = m_header->R = m_header;
}

void DLX::setup(const std::vector<Placement>& placements,
                const std::vector<bool>& boardMask,
                int boardWidth, int boardHeight,
                int numPieces)
{
  std::vector<int> boardCellToColumn(boardWidth * boardHeight, -1);
  int colIndex = 0;

  for (int i = 0; i < boardWidth * boardHeight; ++i) {
    if (boardMask[i]) {
      boardCellToColumn[i] = colIndex++;
      addColumn("C" + std::to_string(i));
    }
  }

  int pieceColsStart = colIndex;
  for (int p = 0; p < numPieces; ++p) {
    addColumn("P" + std::to_string(p));
  }

  for (size_t i = 0; i < placements.size(); ++i) {
    const Placement& pl = placements[i];
    std::vector<int> rowCols;

    bool placementValid = true;
    for (int cell : pl.cells) {
      int col = boardCellToColumn[cell];
      if (col == -1) {
        placementValid = false;
        break;
      }
      rowCols.push_back(col);
    }

    if (!placementValid) continue;

    rowCols.push_back(pieceColsStart + pl.pieceID);
    addRow(static_cast<int>(i), rowCols);
  }
}

void DLX::setHeuristic(HeuristicMode heuristic)
{
  m_heuristic = heuristic;
}

int DLX::addColumn(const std::string& name) {
  ColumnNode* c = makeColumn(name);
  c->R = m_header;
  c->L = m_header->L;
  m_header->L->R = c;
  m_header->L = c;
  c->U = c->D = c;
  m_columns.push_back(c);
  return static_cast<int>(m_columns.size() - 1);
}

void DLX::addRow(int rowID, const std::vector<int>& cols) {
  std::vector<DLXNode*> rowNodes;
  for (int col : cols) {
    ColumnNode* C = m_columns[static_cast<size_t>(col)];
    DLXNode* node = makeNode();
    node->rowID = rowID;
    node->colID = col;

    // Vertical insert
    node->D = C;
    node->U = C->U;
    C->U->D = node;
    C->U = node;
    C->size++;

    rowNodes.push_back(node);
  }

  // Horizontal circular linking
  size_t n = rowNodes.size();
  for (size_t i = 0; i < n; ++i) {
    rowNodes[i]->R = rowNodes[(i + 1) % n];
    rowNodes[i]->L = rowNodes[(i + n - 1) % n];
  }
}

void DLX::cover(ColumnNode* c) {
  c->R->L = c->L;
  c->L->R = c->R;
  for (DLXNode* i = c->D; i != c; i = i->D)
    for (DLXNode* j = i->R; j != i; j = j->R) {
      j->D->U = j->U;
      j->U->D = j->D;
      static_cast<ColumnNode*>(m_columns[j->colID])->size--;
    }
}

void DLX::uncover(ColumnNode* c) {
  for (DLXNode* i = c->U; i != c; i = i->U)
    for (DLXNode* j = i->L; j != i; j = j->L) {
      static_cast<ColumnNode*>(m_columns[j->colID])->size++;
      j->D->U = j;
      j->U->D = j;
    }
  c->R->L = c;
  c->L->R = c;
}

ColumnNode* DLX::chooseColumnNone() const {
  ColumnNode* best = nullptr;
  int bestSize = std::numeric_limits<int>::max();
  for (ColumnNode* c = static_cast<ColumnNode*>(m_header->R); c != m_header; c = static_cast<ColumnNode*>(c->R))
    if (c->size < bestSize) {
      bestSize = c->size;
      best = c;
      if (bestSize <= 1) break;
    }
  return best;
}

ColumnNode* DLX::chooseColumnLeastFilled() const
{
  return chooseColumnNone();
}

ColumnNode* DLX::chooseColumn() const
{
  if (m_heuristic == HeuristicMode::None) {
    return chooseColumnNone();
  }
  else {
    return chooseColumnLeastFilled();
  }
}

void DLX::search(int k) {
  if (p_stopFlag && p_stopFlag->load()) return;
  if (m_header->R == m_header) {
    if (handleSolution) handleSolution(m_solutionRows);
    if (p_solutionsFound) p_solutionsFound->fetch_add(1);
    return;
  }
  if (p_nodesVisited) p_nodesVisited->fetch_add(1);

  ColumnNode* c = chooseColumn();
  if (!c || c->size == 0) return;

  cover(c);
  for (DLXNode* r = c->D; r != c; r = r->D) {
    m_solutionRows.push_back(r->rowID);
    for (DLXNode* j = r->R; j != r; j = j->R) cover(m_columns[j->colID]);
    search(k + 1);
    for (DLXNode* j = r->L; j != r; j = j->L) uncover(m_columns[j->colID]);
    m_solutionRows.pop_back();
  }
  uncover(c);
}

void DLX::searchWithDebug(int depth) {
  if (m_header->R == m_header) {
    std::cout << "SOLUTION FOUND at depth " << depth << "\n";
    return;
  }
  ColumnNode* c = chooseColumn();
  if (!c || c->size == 0) {
    std::cout << "DEAD END at depth " << depth << "\n";
    return;
  }

  std::cout << "Depth " << depth << ": choosing column " << c->name
            << " (id=" << c->colID << ") with " << c->size << " rows\n";

  cover(c);
  for (DLXNode* r = c->D; r != c; r = r->D) {
    m_solutionRows.push_back(r->rowID);
    for (DLXNode* j = r->R; j != r; j = j->R) cover(m_columns[j->colID]);
    searchWithDebug(depth + 1);
    for (DLXNode* j = r->L; j != r; j = j->L) uncover(m_columns[j->colID]);
    m_solutionRows.pop_back();
  }
  uncover(c);
}

ColumnNode* DLX::makeColumn(const std::string& name) {
  auto col = std::make_unique<ColumnNode>(name);
  ColumnNode* ptr = col.get();
  m_nodePool.push_back(std::move(col));
  return ptr;
}

DLXNode* DLX::makeNode() {
  auto node = std::make_unique<DLXNode>();
  DLXNode* ptr = node.get();
  m_nodePool.push_back(std::move(node));
  return ptr;
}

/*
void debugDLX(DLX& dlx, const std::vector<Placement>& placements,
              int boardWidth, int boardHeight)
{
  std::cout << "=== DLX Debug Summary ===\n";

  // 1. Column summary
  std::cout << "Columns:\n";
  for (size_t i = 0; i < dlx.m_columns.size(); ++i) {
    const ColumnNode* c = dlx.m_columns[i];
    std::cout << "  " << c->name << " (size=" << c->size << ")\n";
  }

  // 2. Sample rows
  std::cout << "\nSample rows (first 10 placements):\n";
  for (size_t r = 0; r < std::min<size_t>(10, placements.size()); ++r) {
    const Placement& pl = placements[r];
    std::cout << "  Placement " << r << " piece " << pl.pieceID
              << " covers columns: ";
    for (int cell : pl.cells) {
      std::cout << cell << " ";
    }
    std::cout << "\n";
  }

  // 3. Board coverage sanity check
  std::vector<bool> covered(boardWidth * boardHeight, false);
  for (const Placement& pl : placements) {
    for (int idx : pl.cells) {
      covered[idx] = true;
    }
  }

  int uncovered = 0;
  for (int i = 0; i < boardWidth * boardHeight; ++i) {
    if (!covered[i]) uncovered++;
  }
  std::cout << "\nBoard coverage: " << (boardWidth*boardHeight - uncovered)
            << " / " << (boardWidth*boardHeight) << " cells covered\n";

  // 4. Limited search debug
  std::cout << "\nStarting DLX search (limited debug to depth 0..3)...\n";
  std::function<void(int)> limitedSearch = [&](int depth) {
    if (dlx.m_header->R == dlx.m_header) {
      std::cout << "  Solution found at depth " << depth << "\n";
      return;
    }
    if (depth > 3) return;

    ColumnNode* c = dlx.chooseColumn();
    if (!c || c->size == 0) {
      std::cout << "  DEAD END at depth " << depth
                << " (column " << (c ? c->name : "null") << ")\n";
      return;
    }

    std::cout << "  Depth " << depth << ": choosing column " << c->name
              << " (size=" << c->size << ")\n";

    dlx.cover(c);
    for (DLXNode* r = c->D; r != c; r = r->D) {
      dlx.m_solutionRows.push_back(r->rowID);
      for (DLXNode* j = r->R; j != r; j = j->R) {
        dlx.cover(dlx.m_columns[j->colID]);
      }

      limitedSearch(depth + 1);

      for (DLXNode* j = r->L; j != r; j = j->L) {
        dlx.uncover(dlx.m_columns[j->colID]);
      }
      dlx.m_solutionRows.pop_back();
    }
    dlx.uncover(c);
  };

  limitedSearch(0);
  std::cout << "=== DLX Debug End ===\n";
}
*/
