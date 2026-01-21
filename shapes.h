#pragma once

#include <filesystem>
#include <vector>

struct Coord
{
  int x, y;

  bool operator<(const Coord& other) const {
    return (x < other.x) || (x == other.x && y < other.y);
  }

  bool operator==(const Coord& o) const noexcept {
    return x == o.x && y == o.y;
  }
};

using Shape = std::vector<Coord>;

struct Piece
{
  Shape shape;
  std::string color;
};

struct Placement
{
  int pieceID;
  std::vector<int> cells;
};


// --- Predefined sets ---
enum class PredefinedSet { Tetrominoes, Pentominoes, Hexominoes, IQ };

std::vector<Piece> loadPredefinedPieces(PredefinedSet set);
std::vector<Piece> loadPiecesFromFile(const std::filesystem::path& file);
// std::vector<Piece> loadPiecesFromBoard(const std::filesystem::path& file);

struct PiecesAndMask {
  std::vector<Piece> pieces;
  std::vector<bool> boardMask;
  int boardWidth = 0;
  int boardHeight = 0;
};

PiecesAndMask loadPiecesAndMaskFromBoardFile(const std::filesystem::path& file);


std::vector<Shape> generateTransforms(const Shape& base);

// board_mask.size() == BOARD_CELLS, board_mask[cellIndex] == true if usable
// Precompute all valid placements for IQ Blocks, fully optimized.
// mask: vector<bool> size BOARD_CELLS, true = available cell; empty = full board.
// Outputs same as before: placements, placements_by_piece, all_transforms.
std::vector<Placement> enumeratePlacements(
  const std::vector<Piece>& baseShapes,
  int boardWidth, int boardHeight,
  const std::vector<bool>& mask = {});

const std::vector<Piece> fourpieces = {
  {{{0,0},{1,0},{0,1}}, "#66B2FF"}, // baby blue
  {{{0,0},{1,0},{0,1}}, "#FF4684"}, // red
  {{{0,0},{1,0},{0,1}}, "#FFFF00"}, // yellow
  {{{0,0},{1,0},{0,1}}, "#00FF7F"}, // light green
  {{{0,0},{1,0},{0,1},{1,1}}, "#DA70D6"} // purple
};

const std::vector<Piece> iqblocks_old = {
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

const std::vector<Piece> tetrominos = {
  // I Tetromino
  {{{0,0},{0,1},{0,2},{0,3}}, "#FF6F61"}, // I1
  {{{0,0},{0,1},{0,2},{0,3}}, "#FF6F61"}, // I2

  // O Tetromino
  {{{0,0},{1,0},{0,1},{1,1}}, "#6B5B95"}, // O1
  {{{0,0},{1,0},{0,1},{1,1}}, "#6B5B95"}, // O2

  // T Tetromino
  {{{0,0},{1,0},{2,0},{1,1}}, "#88B04B"}, // T1
  {{{0,0},{1,0},{2,0},{1,1}}, "#88B04B"}, // T2

  // S Tetromino
  {{{1,0},{2,0},{0,1},{1,1}}, "#F7CAC9"}, // S1
  {{{1,0},{2,0},{0,1},{1,1}}, "#F7CAC9"}, // S2

  // Z Tetromino
  {{{0,0},{1,0},{1,1},{2,1}}, "#92A8D1"}, // Z1
  {{{0,0},{1,0},{1,1},{2,1}}, "#92A8D1"}, // Z2

  // J Tetromino
  {{{0,0},{0,1},{1,1},{2,1}}, "#955251"}, // J1
  {{{0,0},{0,1},{1,1},{2,1}}, "#955251"}, // J2

  // L Tetromino
  {{{2,0},{0,1},{1,1},{2,1}}, "#B565A7"}, // L1
  {{{2,0},{0,1},{1,1},{2,1}}, "#B565A7"}, // L2
};

const std::vector<Piece> pentominos = {
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

const std::vector<Piece> pieces2 = {
  {{{0,0},{1,0},{0,1}}, "#66B2FF"}, // baby blue
  {{{0,0},{1,0},{0,1}}, "#FFEFD5"}, // ivory
  {{{0,0},{1,0},{0,1}}, "#DA70D6"}, // purple
  {{{0,0},{1,0},{0,1}}, "#3CB371"}  // dark green
};

const std::vector<Piece> iqblocks = {
  {{{0,0},{1,0},{1,1},{2,1},{3,1}}, "#FF57AB"},  // pink
  {{{0,0},{1,0},{2,0},{0,1},{0,2}}, "#1082F5"},  // blue
  {{{0,0},{1,0},{2,0},{3,0},{2,1}}, "#FFC400"},  // yellow
    {{{0,0},{1,0},{2,0},{3,0},{0,1}}, "#CC0000"},  // red
    {{{0,0},{0,1},{1,1},{2,1},{1,2}}, "#FF8000"},  // orange
    {{{0,0},{0,1},{1,1},{1,2},{2,2}}, "#460046"},  // purple
    {{{0,0},{1,0},{2,0},{0,1},{2,1}}, "#6B8E23"},  // green
    {{{0,0},{1,0},{0,1}}, "#56B5FD"},              // light blue
    {{{0,0},{1,0},{2,0},{0,1},{1,1}}, "#67FCDE"},  // mint
    {{{0,0},{1,0},{2,0},{1,1}}, "#048383"},        // teal
    {{{0,0},{1,0},{2,0},{2,1}}, "#000066"},        // dark blue
    {{{0,0},{1,0},{1,1},{2,1}}, "#660000"}         // dark red
};
