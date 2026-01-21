/*
How it works:
Pieces + board cells -> exact cover columns
Each placement -> a row covering the cells + the piece usage column
DLX search recursively covers columns
Heuristic switches between plain min-column and your “least filled area” (implemented as min column size, customizable)
Solutions printed in colored ASCII (or plain)
SVG files named solution_1.svg, solution_2.svg, ... saved unless disabled
CSV summary written if filename specified (one line per placement per solution)
Progress printed on stderr every interval seconds if requested

https://isomerdesign.com/Pentomino/
https://puzzler.sourceforge.net/docs/polyominoes.html
 */

/// @todo improve random colors, maybe using hue?
/// @todo we seem to get more solutions. maybe diagonal flip needs to be accounted for to remove duplicates and get unique solutions?
/// @todo pipes between steps?

#include "dlx.h"
#include "reporting.h"
#include "shapes.h"
#include "video.h"

#include <CLI/CLI.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
// Store all canonical solutions here
static std::set<std::string> uniqueSolutions;

enum SymmetryOp {
  ROT_0,
  ROT_90,
  ROT_180,
  ROT_270,
  REFLECT_X,   // horizontal flip (mirror over vertical axis)
  REFLECT_Y,   // vertical flip (mirror over horizontal axis)
  REFLECT_D1,  // reflect over main diagonal (y = x)
  REFLECT_D2   // reflect over anti-diagonal (y = -x)
};

// Apply a symmetry to (x, y) coordinates
void transformCoord(int x, int y, int W, int H, SymmetryOp op, int& nx, int& ny)
{
  switch (op) {
  case ROT_0:     nx = x;         ny = y;         break;
  case ROT_90:    nx = H - 1 - y; ny = x;         break;
  case ROT_180:   nx = W - 1 - x; ny = H - 1 - y; break;
  case ROT_270:   nx = y;         ny = W - 1 - x; break;
  case REFLECT_X: nx = W - 1 - x; ny = y;         break;
  case REFLECT_Y: nx = x;         ny = H - 1 - y; break;
  case REFLECT_D1: nx = y;        ny = x;         break;
  case REFLECT_D2: nx = H - 1 - y; ny = W - 1 - x; break;
  }
}

// Apply a symmetry to the whole board
std::vector<int> applySymmetry(const std::vector<int>& board, int W, int H, SymmetryOp op)
{
  int newW = W, newH = H;
  if (op == ROT_90 || op == ROT_270 || op == REFLECT_D1 || op == REFLECT_D2) {
    newW = H;
    newH = W;
  }

  std::vector<int> newBoard(newW * newH, -1);

  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      int nx, ny;
      transformCoord(x, y, W, H, op, nx, ny);
      newBoard[ny * newW + nx] = board[y * W + x];
    }
  }
  return newBoard;
}

struct TransformedBoard
{
  std::vector<int>  board; // piece ids (or -1); holes will be in mask
  std::vector<bool> mask;  // true = allowed cell, false = hole
  int W, H;
};

// Transform both board and mask together
static TransformedBoard applySymmetryBoardAndMask(
  const std::vector<int>& board,
  const std::vector<bool>& mask,
  int W, int H,
  SymmetryOp op)
{
  int newW = W, newH = H;
  // Ops that swap axes
  if (op == ROT_90 || op == ROT_270 || op == REFLECT_D1 || op == REFLECT_D2) {
    newW = H;
    newH = W;
  }

  std::vector<int>  newBoard(newW * newH, -1);
  std::vector<bool> newMask(newW * newH, false);

  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      int nx, ny;
      transformCoord(x, y, W, H, op, nx, ny);
      newBoard[ny * newW + nx] = board[y * W + x];
      newMask [ny * newW + nx] = mask [y * W + x];
    }
  }
  return { std::move(newBoard), std::move(newMask), newW, newH };
}

// Serialize board to string for comparison
std::string serializeBoard(const std::vector<int>& board)
{
  std::string s;
  s.reserve(board.size() * 3);
  for (int v : board) {
    if (v < 0) s += ".";
    else s += char('A' + v); // piece 0 -> 'A', etc.
  }
  return s;
}

// Serialize ONLY allowed (mask == true) cells, row-major in the transformed space.
// This makes holes irrelevant to the canonical ordering.
static std::string serializeAllowedCells(const std::vector<int>& board,
                                         const std::vector<bool>& mask,
                                         int W, int H)
{
  // Pre-size reserve to number of allowed cells (rough heuristic)
  size_t allowed = 0;
  for (bool b : mask) if (b) ++allowed;

  std::string s;
  s.reserve(allowed);

  for (int idx = 0; idx < W * H; ++idx) {
    if (!mask[idx]) continue; // skip holes entirely
    int v = board[idx];
    // By construction of an exact cover, every allowed cell should be occupied.
    // But just in case, keep a distinct char for "uncovered" (should not happen):
    if (v < 0) s += '_';
    else {
      // Map piece id to a stable character sequence.
      // For safety beyond 26 pieces, encode as two chars if needed.
      if (v >= 0 && v < 26) {
        s += char('A' + v);
      } else {
        // Fallback: encode as '#' followed by byte (still stable & comparable)
        s += '#';
        s += char(1 + (v % 250)); // simple bounded byte
      }
    }
  }
  return s;
}

std::string normalizePieceIDs(const std::vector<int>& board, const std::vector<bool>& mask) {
  std::unordered_map<int,int> remap;
  int nextID = 0;
  std::string s;
  for (size_t i = 0; i < board.size(); ++i) {
    if (!mask[i]) continue;
    int v = board[i];
    if (v < 0) s += '_';
    else {
      if (!remap.count(v)) remap[v] = nextID++;
      s += char('A' + remap[v]);
    }
  }
  return s;
}

// Compute canonical form (lexicographically smallest among all symmetries)
std::string canonicalForm(const std::vector<int>& board, int W, int H)
{
  static const SymmetryOp ops[] = {
    ROT_0, ROT_90, ROT_180, ROT_270,
    REFLECT_X, REFLECT_Y, REFLECT_D1, REFLECT_D2
  };

  std::string best;
  for (SymmetryOp op : ops)
  {
    const auto transformed = applySymmetry(board, W, H, op);
    const auto serialized  = serializeBoard(transformed);
    if (best.empty() || serialized < best) best = serialized;
  }
  return best;
}

// Canonical form: lexicographically smallest serialization across all 8 symmetries
std::string canonicalFormWithMask(const std::vector<int>& board,
                                  const std::vector<bool>& mask,
                                  int W, int H)
{
  static const SymmetryOp ops[] = {
    ROT_0, ROT_90, ROT_180, ROT_270,
    REFLECT_X, REFLECT_Y, REFLECT_D1, REFLECT_D2
  };

  std::string best;
  for (SymmetryOp op : ops) {
    const TransformedBoard tb = applySymmetryBoardAndMask(board, mask, W, H, op);
    // std::string ser = serializeAllowedCells(tb.board, tb.mask, tb.W, tb.H);
    std::string ser = normalizePieceIDs(tb.board, tb.mask);
    if (best.empty() || ser < best) {
      best = std::move(ser);
    }
  }
  return best;
}

// Returns true if this is a new symmetry class
bool isNewSolution(const std::vector<int>& board, int boardWidth, int boardHeight)
{
  const std::string cf = canonicalForm(board, boardWidth, boardHeight);
  const auto [it, inserted] = uniqueSolutions.insert(cf);
  return inserted;
}

// Public: returns true if this solution is a NEW symmetry class (w.r.t. the mask)
// We rotate/reflect both the filled board and the mask.
//             During serialization we ignore holes (we only serialize cells where mask==true after the transform).
//             Canonicalization is thus based solely on the occupied, valid cells, so the 2×2 central hole can’t cause spurious differences.

bool isNewSolutionWithMask(const std::vector<int>& board,
                           const std::vector<bool>& mask,
                           int W, int H)
{
  std::string cf = canonicalFormWithMask(board, mask, W, H);
  auto [it, inserted] = uniqueSolutions.insert(std::move(cf));
  return inserted;
}


// Debug harness for pentomino tiling pipeline

// Debug harness for pentomino tiling pipeline with coverage check

void debugPipeline(const std::vector<Piece>& basePieces, int boardW, int boardH) {
  std::cout << "=== DEBUG HARNESS START ===\n";

  // 1. Generate transforms
  std::vector<std::vector<Shape>> allTransforms;
  for (size_t pid = 0; pid < basePieces.size(); ++pid) {
    auto tforms = generateTransforms(basePieces[pid].shape);
    std::cout << "Piece " << pid << " has " << tforms.size() << " unique transforms\n";
    allTransforms.push_back(tforms);
  }

  // 2. Placement generation
  size_t totalPlacements = 0;
  size_t totalCellsCovered = 0;
  std::vector<std::vector<std::vector<int>>> placementsPerPiece(basePieces.size());

  for (size_t pid = 0; pid < allTransforms.size(); ++pid) {
    const auto& tforms = allTransforms[pid];
    size_t piecePlacements = 0;

    for (size_t tid = 0; tid < tforms.size(); ++tid) {
      const auto& shape = tforms[tid];

      // Find bounding box
      int maxx = 0, maxy = 0;
      for (auto& c : shape) {
        if (c.x > maxx) maxx = c.x;
        if (c.y > maxy) maxy = c.y;
      }

      // Try all positions on board
      for (int ox = 0; ox + maxx < boardW; ++ox) {
        for (int oy = 0; oy + maxy < boardH; ++oy) {
          std::vector<int> cells;
          cells.reserve(shape.size());

          for (auto& c : shape) {
            int x = ox + c.x;
            int y = oy + c.y;
            int idx = y * boardW + x; // row-major
            cells.push_back(idx);
          }

          placementsPerPiece[pid].push_back(cells);
          totalPlacements++;
          totalCellsCovered += cells.size();
          piecePlacements++;
        }
      }

      std::cout << "  Piece " << pid
                << " transform " << tid
                << " -> " << piecePlacements << " placements so far\n";
    }

    std::cout << "Piece " << pid << " total placements: " << piecePlacements << "\n";
  }

  // 3. Coverage summary
  std::cout << "All pieces: " << totalPlacements << " placements total\n";
  std::cout << "Placements cover " << totalCellsCovered << " cell positions total\n";

  // 4. Print sample placements
  for (size_t pid = 0; pid < placementsPerPiece.size(); ++pid) {
    if (!placementsPerPiece[pid].empty()) {
      std::cout << "Sample placement for piece " << pid << ": ";
      for (auto idx : placementsPerPiece[pid][0]) {
        std::cout << idx << " ";
      }
      std::cout << "\n";
      break; // only show one piece for now
    }
  }

  // 5. Check that every board cell is covered
  std::vector<int> cellCoverage(boardW * boardH, 0);
  for (size_t pid = 0; pid < placementsPerPiece.size(); ++pid) {
    for (auto& placement : placementsPerPiece[pid]) {
      for (int idx : placement) {
        if (idx >= 0 && idx < (int)cellCoverage.size()) {
          cellCoverage[idx]++;
        }
      }
    }
  }

  bool allCovered = true;
  for (int idx = 0; idx < boardW * boardH; ++idx) {
    if (cellCoverage[idx] == 0) {
      int x = idx % boardW;
      int y = idx / boardW;
      std::cout << "WARNING: Board cell (" << x << "," << y
                << ") idx=" << idx << " is NEVER covered by any placement!\n";
      allCovered = false;
    }
  }

  if (allCovered) {
    std::cout << "OK: All " << (boardW * boardH) << " cells are covered by at least one placement\n";
  }

  std::cout << "=== DEBUG HARNESS END ===\n";
}



}


std::atomic<uint64_t> g_nodesVisited{0};
std::atomic<uint64_t> g_solutionsFound{0};
std::atomic<bool> g_stopFlag{false};
std::mutex csv_mutex;

// Reporter thread: prints progress every interval seconds
void reporterThreadFunc(int intervalSec)
{
  using namespace std::chrono_literals;

  const auto t0 = std::chrono::steady_clock::now();
  while (!g_stopFlag.load())
  {
    std::this_thread::sleep_for(std::chrono::seconds(intervalSec));
    const uint64_t nodes = g_nodesVisited.load();
    const uint64_t sols = g_solutionsFound.load();
    const auto t1 = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double>(t1 - t0).count();

    std::cerr << "[Progress] Time " << std::fixed << std::setprecision(1) << elapsed
              << "s, Nodes visited: " << nodes << ", Solutions found: " << sols << "\n";
  }
}

int main(int argc, char* argv[])
{
  int maxSolutions = 0; // 0 = unlimited
  int progressInterval = 0;
  bool uniqueSolutions = false;
  bool print = false;
  bool saveSVG = false;
  bool saveVideo = false;


  std::string csvFilename;
  HeuristicMode heuristic = HeuristicMode::LeastFilled;

  CLI::App app{"Pentomino solver"};

  app.add_option("--progress-interval", progressInterval, "Progress report interval in seconds (0 = none)");
  app.add_option("--max-solutions", maxSolutions, "Maximum number of solutions to find (0 = unlimited)");
  app.add_flag("--unique-solutions", uniqueSolutions, "Only output unique solutions");
  app.add_flag("--print", print, "Print solutions to terminal");
  app.add_flag("--svg", saveSVG, "Save solutions as SVG files");
  app.add_flag("--video", saveVideo, "Save all boards for video creation");
  app.add_option("--csv", csvFilename, "Save solutions in CSV format to given filename");
  app.add_option("--heuristic", heuristic, "Heuristic to use: none | least-filled")->transform(
    CLI::CheckedTransformer(std::map<std::string, HeuristicMode>{
                              {"none", HeuristicMode::None},
                              {"least-filled", HeuristicMode::LeastFilled}
                            },
                            CLI::ignore_case));

  // Board options:
  int boardWidth = 8;
  int boardHeight = 8;
  std::vector<std::pair<int,int>> holeCoords;
  std::filesystem::path maskFile;

  app.add_option("--board-width", boardWidth, "Width of the board");
  app.add_option("--board-height", boardHeight, "Height of the board");
  app.add_option("--board-mask", maskFile, "Path to a text file containing the board mask");
  app.add_option("--hole", holeCoords, "Specify a hole as x,y coordinate")->delimiter(',');

  // Piece options:
  std::string predefinedSetStr;
  std::filesystem::path piecesFile, boardFile;

  app.add_option("--pieces", predefinedSetStr, "Use predefined set: tetrominoes, pentominoes, hexominoes");
  app.add_option("--pieces-file", piecesFile, "File with piece coordinates and colors");
  app.add_option("--pieces-board", boardFile, "File with board-number representation of pieces");

  // Video-specific options:
  int videoWidth = 480;
  int videoHeight = 480;
  int videoFPS = 10;
  std::filesystem::path videoFilename = "output.mp4";

  app.add_option("--video-width", videoWidth, "Width of video frames")->needs("--video");
  app.add_option("--video-height", videoHeight, "Height of video frames")->needs("--video");
  app.add_option("--video-fps", videoFPS, "Frames per second for video")->needs("--video");
  app.add_option("--video-file", videoFilename, "Video output filename")->needs("--video");

  CLI11_PARSE(app, argc, argv);

  if (saveVideo) {
    std::cout << "Video output enabled:\n"
              << "  Filename: " << videoFilename << "\n"
              << "  Frame size: " << videoWidth << "x" << videoHeight << "\n"
              << "  FPS: " << videoFPS << "\n";
  }

  std::vector<Piece> pieces;
  std::vector<bool> boardMask(boardWidth * boardHeight, true);

  if (!boardFile.empty()) {
    const auto result = loadPiecesAndMaskFromBoardFile(boardFile);
    pieces = result.pieces;
    boardMask = result.boardMask;
    boardWidth = result.boardWidth;
    boardHeight = result.boardHeight;
  }
  else
  {
    // this path requires setting width, height first
    if (!maskFile.empty())
    {
      std::ifstream in(maskFile);
      if (!in) {
        std::cerr << "Failed to open mask file: " << maskFile << "\n";
        return 1;
      }

      std::string line;
      int row = 0;
      while (std::getline(in, line) && row < boardHeight) {
        for (int col = 0; col < boardWidth && col < (int)line.size(); ++col) {
          const char c = line[col];
          if (c == '0') {
            boardMask[row * boardWidth + col] = false; // hole
          } else {
            boardMask[row * boardWidth + col] = true; // filled
          }
        }
        ++row;
      }
    }

    // Also apply --hole options
    for (const auto [x,y] : holeCoords) {
      if (x >= 0 && x < boardWidth && y >= 0 && y < boardHeight) {
        boardMask[y * boardWidth + x] = false;
      }
    }

    if (!predefinedSetStr.empty()) {
      PredefinedSet set = PredefinedSet::Tetrominoes;
      if (predefinedSetStr=="tetrominoes") set=PredefinedSet::Tetrominoes;
      else if (predefinedSetStr=="pentominoes") set=PredefinedSet::Pentominoes;
      else if (predefinedSetStr=="hexominoes") set=PredefinedSet::Hexominoes;
      else if (predefinedSetStr=="iq") set=PredefinedSet::IQ;
      else throw std::runtime_error("Unknown predefined set: "+predefinedSetStr);
      pieces = loadPredefinedPieces(set);
    }
    else if (!piecesFile.empty()) {
      pieces = loadPiecesFromFile(piecesFile);
    }
    else {
      throw std::runtime_error("No piece input specified!");
    }
  }

  std::cout << "Loaded " << pieces.size() << " pieces.\n";
  for (size_t i=0; i<pieces.size(); ++i) {
    std::cout << "Piece " << i << " (" << pieces[i].color << "):";
    for (auto &c : pieces[i].shape) std::cout << " (" << c.x << "," << c.y << ")";
    std::cout << "\n";
  }

  std::cout << "Board mask:\n";
  for (int y = 0; y < boardHeight; ++y) {
    for (int x = 0; x < boardWidth; ++x) {
      std::cout << (boardMask[y * boardWidth + x] ? '1' : '0');
    }
    std::cout << "\n";
  }

  debugPipeline(pieces, boardWidth, boardHeight);

  // return 0;

  const std::vector<Placement> placements = enumeratePlacements(
    pieces, boardWidth, boardHeight, boardMask);


  std::vector<std::string> colors;
  for (const auto& p : pieces) {
    colors.emplace_back(p.color);
  }

  DLX dlx;
  dlx.setup(placements, boardMask, boardWidth, boardHeight, static_cast<int>(pieces.size()));
  dlx.setHeuristic(heuristic);
  dlx.p_nodesVisited = &g_nodesVisited;
  dlx.p_solutionsFound = &g_solutionsFound;
  dlx.p_stopFlag = &g_stopFlag;

  std::ofstream csvOut;
  bool csvEnabled = !csvFilename.empty();

  if (csvEnabled) {
    csvOut.open(csvFilename);
    if (!csvOut) {
      std::cerr << "Failed to open CSV file '" << csvFilename << "' for writing.\n";
      return 1;
    }
    csvOut << "SolutionID,NodeVisited,PlacementID,PieceID,Cells\n";
  }

  // Run reporter thread
  std::thread reporterThread;
  if (progressInterval > 0) {
    reporterThread = std::thread(reporterThreadFunc, progressInterval);
  }

  std::vector<int> board(boardWidth * boardHeight, -1); // board for printing solutions
  std::vector<std::vector<int>> allBoards; // all solution boards
  std::size_t solutionCounter = 0;
  std::mutex printMutex;

  dlx.handleSolution = [&](const std::vector<int>& solutionRows)
  {
    bool reportSolution = true;

    if (uniqueSolutions) {
      // Symmetry filter that respects holes
      reportSolution = isNewSolutionWithMask(board, boardMask, boardWidth, boardHeight);

      if (reportSolution) {
        // This is the first time we see this symmetry class
        ++solutionCounter;
      }
    }
    else {
      ++solutionCounter;
    }

    if (print || saveSVG || saveVideo)
    {
      std::fill(board.begin(), board.end(), -1);

      for (int r : solutionRows) {
        const Placement& pl = placements[static_cast<size_t>(r)];
        for (int c : pl.cells) {
          board[static_cast<size_t>(c)] = pl.pieceID;
        }
      }
    }

    if (print && reportSolution) {
      std::lock_guard<std::mutex> lock(printMutex);
      std::cout << "Solution #" << solutionCounter << ":\n";
      // printBoardTerminal(board, boardWidth, boardHeight, colors, true);
    }

    if (saveSVG && reportSolution) {
      std::ostringstream svgfn;
      svgfn << "solution_" << solutionCounter << ".svg";
      writeSolutionSVG(board, boardWidth, boardHeight, svgfn.str(), colors);
    }

    if (saveVideo && reportSolution) {
      allBoards.emplace_back(board);
    }

    if (csvEnabled && reportSolution) {
      std::lock_guard<std::mutex> lock(csv_mutex);
      for (int r : solutionRows)
      {
        const Placement &pl = placements[static_cast<size_t>(r)];
        csvOut << solutionCounter << ","
               << g_nodesVisited.load() << ","
               << r << ","
               << pl.pieceID << ",";

        for (size_t ci=0; ci<pl.cells.size(); ++ci)
        {
          csvOut << pl.cells[ci];
          if (ci+1 < pl.cells.size()) csvOut << ";";
        }
        csvOut << "\n";
      }
      csvOut.flush();
    }

    if (maxSolutions > 0 && solutionCounter >= maxSolutions) {
      g_stopFlag.store(true);
    }
  };


  dlx.search();
  // dlx.searchWithDebug();
  // debugDLX(dlx, placements, boardWidth, boardHeight);

  // Finish reporter thread
  g_stopFlag.store(true);
  if (reporterThread.joinable()) {
    reporterThread.join();
  }

  std::cout << "Search finished. Total nodes visited: " << g_nodesVisited.load()
            << ", solutions found: " << solutionCounter << "\n";

  if (saveVideo)
  {
    std::vector<std::vector<uint8_t>> videoFrames;
    for (const auto& board : allBoards) {
      videoFrames.emplace_back(generateVideoFrame(
        boardWidth, boardHeight, videoWidth, videoHeight, board, colors));
    }

    createAndSaveVideo(videoFilename.c_str(), videoWidth, videoHeight, videoFPS, videoFrames);
  }

  return 0;
}
