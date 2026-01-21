#include "shapes.h"

#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>

namespace
{
std::string randomColorHex() {
  static std::mt19937 rng(std::random_device{}());
  static std::uniform_int_distribution<int> dist(0,255);
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", dist(rng), dist(rng), dist(rng));
  return std::string(buf);
}

// normalize: shift so min x,y = 0
Shape normalizeShape(const Shape& s) {
  int minx = s[0].x, miny = s[0].y;
  for (auto& c : s) {
    minx = std::min(minx, c.x);
    miny = std::min(miny, c.y);
  }
  Shape out;
  out.reserve(s.size());
  for (auto& c : s) {
    out.push_back({c.x - minx, c.y - miny});
  }
  std::sort(out.begin(), out.end());
  return out;
}

Shape rotate90(const Shape& s) {
  Shape r; r.reserve(s.size());
  for (auto& c : s) r.push_back({-c.y, c.x});
  return normalizeShape(r);
}

Shape reflectX(const Shape& s) {
  Shape r; r.reserve(s.size());
  for (auto& c : s) r.push_back({-c.x, c.y});
  return normalizeShape(r);
}

bool sameShape(const Shape& a, const Shape& b)
{
  if (a.size() != b.size()) {
    return false;
  }

  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i].x != b[i].x || a[i].y != b[i].y) {
      return false;
    }
  }

  return true;
}

/*
std::vector<Shape> generateTransforms(const Shape& base)
{
  std::vector<Shape> transformedShapes;
  Shape current = normalizeShape(base);

  for (int r = 0; r < 4; ++r)
  {
    const Shape rotated = current;

    for (int f = 0; f < 2; ++f)
    {
      const Shape transformed = normalizeShape((f == 0) ? rotated : reflectX(rotated));

      bool found = false;
      for (const Shape& t : transformedShapes)
      {
        if (sameShape(t, transformed)) {
          found = true;
          break;
        }
      }

      if (!found) {
        transformedShapes.push_back(transformed);
      }
    }

    current = rotate90(current);
  }

  return transformedShapes;
}
*/

/*
std::vector<Shape> generateTransforms(const Shape& base) {
  std::vector<Shape> result;
  Shape current = base;

  auto isDuplicate = [&](const Shape& s){
    for (const auto& existing : result)
      if (sameShape(existing, s)) return true;
    return false;
  };

  for (int rot = 0; rot < 4; ++rot) {
    Shape rotated = normalizeShape(current);
    Shape reflected = reflectX(rotated);

    if (!isDuplicate(rotated)) result.push_back(rotated);
    if (!isDuplicate(reflected)) result.push_back(reflected);

    current = rotate90(current);
  }

  return result;
}
*/

}

std::vector<Piece> loadPredefinedPieces(PredefinedSet set) {
  std::vector<Piece> pieces;

  switch (set) {
  case PredefinedSet::Tetrominoes:
  {
    pieces = {
      // I Tetromino
      {{{0,0},{0,1},{0,2},{0,3}}, "#FF0000"}, // I1 - red
      {{{0,0},{0,1},{0,2},{0,3}}, "#FF7F50"}, // I2 - coral

      // O Tetromino
      {{{0,0},{1,0},{0,1},{1,1}}, "#FFA500"}, // O1 - orange
      {{{0,0},{1,0},{0,1},{1,1}}, "#FFD700"}, // O2 - gold

      // T Tetromino
      {{{0,0},{1,0},{2,0},{1,1}}, "#FFFF00"}, // T1 - yellow
      {{{0,0},{1,0},{2,0},{1,1}}, "#ADFF2F"}, // T2 - green-yellow

      // S Tetromino
      {{{1,0},{2,0},{0,1},{1,1}}, "#00FF00"}, // S1 - lime
      {{{1,0},{2,0},{0,1},{1,1}}, "#32CD32"}, // S2 - lime green

      // Z Tetromino
      {{{0,0},{1,0},{1,1},{2,1}}, "#00FFFF"}, // Z1 - cyan
      {{{0,0},{1,0},{1,1},{2,1}}, "#1E90FF"}, // Z2 - dodger blue

      // J Tetromino
      {{{0,0},{0,1},{1,1},{2,1}}, "#0000FF"}, // J1 - blue
      {{{0,0},{0,1},{1,1},{2,1}}, "#8A2BE2"}, // J2 - blue violet

      // L Tetromino
      {{{2,0},{0,1},{1,1},{2,1}}, "#FF00FF"}, // L1 - magenta
      {{{2,0},{0,1},{1,1},{2,1}}, "#FF1493"}, // L2 - deep pink
    };
    break;
  }
  case PredefinedSet::Pentominoes:
  {
    pieces = {
      {{{0,1},{1,0},{1,1},{1,2},{2,2}}, "#66B2FF"}, // F
      {{{0,0},{0,1},{0,2},{0,3},{0,4}}, "#FFEFD5"}, // I
      {{{0,0},{0,1},{0,2},{0,3},{1,0}}, "#DA70D6"}, // L
      {{{0,0},{0,1},{1,1},{1,2},{1,3}}, "#3CB371"}, // N
      {{{0,0},{0,1},{1,0},{1,1},{0,2}}, "#CD853F"}, // P
      {{{0,0},{1,0},{2,0},{1,1},{1,2}}, "#FFFF00"}, // T
      {{{0,0},{0,1},{1,0},{2,0},{2,1}}, "#FF8C00"}, // U
      {{{0,0},{0,1},{0,2},{1,2},{2,2}}, "#FF4684"}, // V
      {{{0,0},{0,1},{1,1},{1,2},{2,2}}, "#7FFFD4"}, // W
      {{{1,0},{0,1},{1,1},{2,1},{1,2}}, "#00FF7F"}, // X
      {{{0,0},{0,1},{0,2},{0,3},{1,1}}, "#8A2BE2"}, // Y
      {{{0,0},{1,0},{1,1},{2,1},{2,2}}, "#FF69B4"}, // Z
    };
    break;
  }
  case PredefinedSet::Hexominoes:
  {
    const std::vector<std::string> hexomino_colors = {
      "#FF0000", "#00FF00", "#0000FF", "#FFFF00", "#FF00FF",
      "#00FFFF", "#FFA500", "#800080", "#008000", "#800000",
      "#FFC0CB", "#808000", "#008080", "#000080", "#FFD700",
      "#FF69B4", "#4B0082", "#00FF7F", "#CD5C5C", "#7FFF00",
      "#40E0D0", "#FF8C00", "#6A5ACD", "#ADFF2F", "#FF1493",
      "#20B2AA", "#B22222", "#7B68EE", "#32CD32", "#BA55D3",
      "#FF6347", "#4682B4", "#D2691E", "#9ACD32", "#FF4500"
    };

    const std::vector<Shape> shapes = {
      {{0,0},{1,0},{2,0},{3,0},{4,0},{5,0}},        // Line
      {{0,0},{0,1},{0,2},{0,3},{0,4},{1,4}},        // L-shape
      {{0,0},{1,0},{2,0},{0,1},{0,2},{0,3}},        // J-shape
      {{0,0},{0,1},{0,2},{1,2},{2,2},{3,2}},        // Step
      {{0,0},{1,0},{1,1},{1,2},{1,3},{2,3}},        // Z-like
      {{0,0},{1,0},{2,0},{2,1},{2,2},{3,2}},        // Snake
      {{0,0},{1,0},{1,1},{2,1},{2,2},{3,2}},        // Zig-zag
      {{0,0},{0,1},{0,2},{1,2},{1,3},{2,3}},        // Step variant
      {{0,0},{0,1},{1,1},{1,2},{2,2},{2,3}},        // Double Z
      {{0,0},{1,0},{2,0},{0,1},{1,1},{2,1}},        // Block 2x3
      {{0,0},{1,0},{2,0},{3,0},{1,1},{2,1}},        // T variant
      {{0,0},{0,1},{0,2},{1,2},{2,2},{3,2}},        // Step variant
      {{0,0},{1,0},{1,1},{2,1},{2,2},{3,2}},        // Z variant
      {{0,0},{0,1},{1,1},{1,2},{2,2},{2,3}},        // Double Z variant
      {{0,0},{1,0},{2,0},{3,0},{4,0},{4,1}},        // L-long
      {{0,0},{1,0},{1,1},{2,1},{3,1},{3,2}},        // S-like
      {{0,0},{0,1},{1,1},{1,2},{2,2},{3,2}},        // Skewed Z
      {{0,0},{1,0},{2,0},{0,1},{1,1},{2,1}},        // Block 2x3 variant
      {{0,0},{0,1},{0,2},{1,0},{1,1},{2,0}},        // Small U
      {{0,0},{1,0},{2,0},{0,1},{1,1},{2,1}},        // Block 2x3 duplicate? (normalize to remove duplicates)
      {{0,0},{0,1},{0,2},{1,2},{2,2},{3,2}},        // Step
      {{0,0},{1,0},{1,1},{2,1},{2,2},{3,2}},        // Zig-zag
      {{0,0},{0,1},{1,1},{1,2},{2,2},{2,3}},        // Double Z
      {{0,0},{1,0},{2,0},{0,1},{1,1},{2,1}},        // Block 2x3
      {{0,0},{1,0},{2,0},{3,0},{1,1},{2,1}},        // T variant
      {{0,0},{0,1},{0,2},{1,2},{2,2},{3,2}},        // Step variant
      {{0,0},{0,1},{1,1},{1,2},{2,2},{2,3}},        // Double Z
      {{0,0},{1,0},{2,0},{0,1},{1,1},{2,1}},        // Block 2x3
      {{0,0},{1,0},{2,0},{3,0},{1,1},{2,1}},        // T variant
      {{0,0},{0,1},{0,2},{1,2},{2,2},{3,2}},        // Step variant
      {{0,0},{0,1},{1,1},{1,2},{2,2},{2,3}},        // Double Z
      {{0,0},{1,0},{2,0},{0,1},{1,1},{2,1}},        // Block 2x3
      {{0,0},{1,0},{2,0},{3,0},{4,0},{0,1}},        // L-long
      {{0,0},{1,0},{1,1},{2,1},{2,2},{3,2}}         // Skewed Z
    };

    pieces.clear();
    for (size_t i = 0; i < shapes.size(); ++i) {
      pieces.push_back({shapes[i], hexomino_colors[i]});
    }
    break;
  }


  case PredefinedSet::IQ:
  {
    pieces = {
      {{{0,0},{1,0},{2,0},{2,1},{2,2},{3,2},{4,2}}, "#66B2FF"}, // scar (7)
      {{{0,0},{1,0},{2,0},{3,0},{4,0},{0,1},{1,1}}, "#FFEFD5"}, // long stick (7)
      {{{0,0},{1,0},{2,0},{0,1},{1,1},{2,1},{0,2},{0,3}}, "#DA70D6"}, // big fat L (8)
      {{{0,0},{1,0},{2,0},{0,1},{1,1},{2,1},{0,2},{1,2}}, "#3CB371"}, // bite taken out (8)
      {{{0,0},{1,0},{2,0},{3,0},{0,1},{1,1},{2,1},{3,1}}, "#CD853F"}, // rectangle (8)
      {{{0,0},{1,0},{2,0},{0,1},{0,2},{0,3}}, "#FFFF00"}, // big L (6)
      {{{0,0},{1,0},{2,0},{0,1},{0,2}}, "#FF8C00"}, // medium L (5)
      {{{0,0},{1,0},{2,0},{3,0},{0,1},{1,1}}, "#FF4684"}, // medium stick (6)
      {{{0,0},{1,0},{2,0},{0,1},{1,1}}, "#7FFFD4"}, // short stick (5)
      {{{0,0},{1,0},{0,1},{0,2}}, "#00FF7F"} // tiny L (4)
    };

    break;
  }

  }
  for (auto &p : pieces) p.shape = normalizeShape(p.shape);
  return pieces;
}

// --- File-based pieces ---
std::vector<Piece> loadPiecesFromFile(const std::filesystem::path& file)
{
  std::vector<Piece> pieces;
  std::ifstream in(file);
  if (!in) throw std::runtime_error("Cannot open pieces file: " + file.string());

  std::string line;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    Shape shape;
    std::string color, token;
    while (iss >> token) {
      if (token[0] == '#') { color = token; }
      else {
        auto pos = token.find(',');
        int x = std::stoi(token.substr(0,pos));
        int y = std::stoi(token.substr(pos+1));
        shape.push_back({x,y});
      }
    }
    if (shape.empty()) continue;
    if (color.empty()) color = randomColorHex();
    pieces.push_back({normalizeShape(shape), color});
  }
  return pieces;
}

PiecesAndMask loadPiecesAndMaskFromBoardFile(const std::filesystem::path& filename)
{
  std::ifstream fin(filename);
  if (!fin) throw std::runtime_error("Failed to open board file: " + filename.string());

  std::vector<std::vector<int>> grid;
  std::string line;
  while (std::getline(fin, line)) {
    if (line.empty()) continue;
    std::istringstream iss(line);
    std::vector<int> row;
    int val;
    while (iss >> val) row.push_back(val);
    grid.push_back(row);
  }

  if (grid.empty()) throw std::runtime_error("Board file is empty: " + filename.string());

  int rows = static_cast<int>(grid.size());
  int cols = static_cast<int>(grid[0].size());
  for (const auto &r : grid)
    if (static_cast<int>(r.size()) != cols)
      throw std::runtime_error("Inconsistent row lengths in board file.");

  std::vector<bool> mask(rows * cols, true);
  std::map<int, std::vector<Coord>> pieceCells; // map piece ID -> cells

  for (int y = 0; y < rows; ++y) {
    for (int x = 0; x < cols; ++x) {
      int val = grid[y][x];
      mask[y * cols + x] = (val != 0); // 0 = hole
      if (val != 0) {
        pieceCells[val].push_back({x, y});
      }
    }
  }

  std::vector<Piece> pieces;
  for (const auto &[id, cells] : pieceCells) {
    pieces.push_back({cells, randomColorHex()});
  }

  return PiecesAndMask{pieces, mask, cols, rows};
}



// std::vector<Shape> generateTransforms(const Shape& base) {
//   std::set<Shape> uniq; // relies on Coord::< and normalizeShape
//   Shape cur = normalizeShape(base);
//   for (int i = 0; i < 4; ++i) {
//     Shape rot = (i == 0 ? cur : rotate90(cur));
//     rot = normalizeShape(rot);
//     uniq.insert(rot);
//     uniq.insert(normalizeShape(reflectX(rot)));
//     cur = rot; // important: rotate from normalized each turn
//   }
//   return std::vector<Shape>(uniq.begin(), uniq.end());
// }

std::vector<Shape> generateTransforms(const Shape& base) {
  std::set<Shape> unique;
  std::vector<Shape> result;

  Shape cur = normalizeShape(base);
  for (int r = 0; r < 4; r++) {
    Shape rot = cur;
    for (int f = 0; f < 2; f++) {
      Shape t = (f == 0) ? rot : reflectX(rot);
      t = normalizeShape(t);
      if (unique.insert(t).second) {
        result.push_back(t);
      }
    }
    cur = rotate90(cur);
  }
  return result;
}



// board_mask.size() == BOARD_CELLS, board_mask[cellIndex] == true if usable
// Precompute all valid placements for IQ Blocks, fully optimized.
// mask: vector<bool> size BOARD_CELLS, true = available cell; empty = full board.
// Outputs same as before: placements, placements_by_piece, all_transforms.
std::vector<Placement> enumeratePlacements(
  const std::vector<Piece>& pieces,
  int boardWidth, int boardHeight,
  const std::vector<bool>& mask)
{
  auto linearIndex = [&boardWidth] (int x, int y) {
    return y * boardWidth + x;
  };

  // Precompute all transforms for each piece
  std::vector<std::vector<Shape>> allTransforms;
  int piece_id = 0;
  for (const Piece& piece : pieces) {
    allTransforms.emplace_back(generateTransforms(piece.shape));

    for (auto &shape : allTransforms.back()) {
      int maxX=0,maxY=0; for(auto &c:shape){if(c.x>maxX)maxX=c.x;if(c.y>maxY)maxY=c.y;}
      std::cout<<"Piece "<<piece_id<<" transform maxX="<<maxX<<" maxY="<<maxY<<"\n";
    }
    piece_id++;
  }

  auto cellAvailable = [&](int x, int y) -> bool
  {
    if (x < 0 || y < 0 || x >= boardWidth || y >= boardHeight) {
      return false;
    }

    if (!mask.empty()) {
      return mask[linearIndex(x, y)];
      // return mask[y * boardWidth + x];
    }

    return true;
  };

  // For each piece transform, precompute all valid linear indices sets for placements
  using PrecompPlacement = std::vector<int>;

  // precomputed board cell indices for this placement
  PrecompPlacement linearIndices;

  // For each piece, for each transform: vector of valid placements' linear indices sets
  std::vector<std::vector<std::vector<PrecompPlacement>>> precomp(pieces.size());

  for (size_t pid = 0; pid < allTransforms.size(); ++pid) {
    precomp[pid].resize(allTransforms[pid].size());

    for (size_t tid = 0; tid < allTransforms[pid].size(); ++tid) {
      const Shape& shape = allTransforms[pid][tid];

      int maxx = 0;
      int maxy = 0;
      for (const Coord& c : shape) {
        if (c.x > maxx) maxx = c.x;
        if (c.y > maxy) maxy = c.y;
      }

      auto& placementsForTransform = precomp[pid][tid];

      // Fixed loop: use <= so piece can touch the last row/column
      for (int oy = 0; oy <= boardHeight - (maxy + 1); ++oy) {
        for (int ox = 0; ox <= boardWidth - (maxx + 1); ++ox) {
          bool ok = true;
          for (const Coord& c : shape) {
            if (!cellAvailable(ox + c.x, oy + c.y)) {
              ok = false;
              break;
            }
          }
          if (!ok) continue;

          // Precompute linear indices once
          PrecompPlacement pp;
          pp.reserve(shape.size());
          for (const auto& c : shape) {
            pp.emplace_back(linearIndex(ox + c.x, oy + c.y));
          }

          placementsForTransform.emplace_back(std::move(pp));
        }
      }
    }
  }

  for (size_t pid=0; pid<precomp.size(); ++pid)
  {
    int total=0;
    for(auto& t: precomp[pid]) total += t.size();
    std::cout<<"Piece "<<pid<<" has "<<total<<" placements\n";
  }


  // Now generate output placements from precomputed linear indices sets
  std::vector<Placement> placements;

  for (size_t pid = 0; pid < precomp.size(); ++pid) {
    for (size_t tid = 0; tid < precomp[pid].size(); ++tid) {
      for (const PrecompPlacement& pp : precomp[pid][tid])
      {
        Placement pl;
        pl.pieceID = static_cast<int>(pid);
        pl.cells = pp; // copy precomputed indices directly
        placements.emplace_back(std::move(pl));
      }
    }
  }

  return placements;
}


/*
std::vector<Placement> enumeratePlacements(
  const std::vector<Shape>& pieceShapes,
  int W, int H,
  const std::vector<bool>& mask // empty => all true
  ) {
  auto lin = [W](int x,int y){ return y*W + x; };
  auto avail = [&](int x,int y)->bool {
    if (x<0||y<0||x>=W||y>=H) return false;
    return mask.empty() ? true : mask[lin(x,y)];
  };

  // All unique, normalized transforms per piece
  std::vector<std::vector<Shape>> allT;
  allT.reserve(pieceShapes.size());
  for (auto& s : pieceShapes) allT.push_back(generateTransforms(s));

  std::vector<Placement> out;
  out.reserve(5000);

  for (size_t pid=0; pid<allT.size(); ++pid) {
    for (const auto& shapeN : allT[pid]) {
      // compute max extents from the *normalized* transform
      int maxX=0, maxY=0;
      for (auto& c : shapeN) { if (c.x>maxX) maxX=c.x; if (c.y>maxY) maxY=c.y; }

      for (int oy=0; oy<=H-(maxY+1); ++oy) {
        for (int ox=0; ox<=W-(maxX+1); ++ox) {
          bool ok=true;
          for (auto& c : shapeN) {
            if (!avail(ox+c.x, oy+c.y)) { ok=false; break; }
          }
          if (!ok) continue;

          Placement pl; pl.pieceID = (int)pid;
          pl.cells.reserve(shapeN.size());
          for (auto& c : shapeN) pl.cells.push_back(lin(ox+c.x, oy+c.y));
          out.emplace_back(std::move(pl));
        }
      }
    }
  }

  return out;
}
*/
