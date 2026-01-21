#pragma once

#include <string>
#include <vector>

void printBoardTerminal(
  const std::vector<int>& board,
  int boardWidth, int boardHeight,
  const std::vector<std::string>& pieceColors,
  bool color = true);

std::vector<uint8_t> generateVideoFrame(
  int boardWidth, int boardHeight,
  int frameWidth, int frameHeight,
  const std::vector<int>& board,
  const std::vector<std::string>& pieceColors);

void writeSolutionSVG(
  const std::vector<int>& board,
  int boardWidth, int boardHeight,
  const std::string& filename,
  const std::vector<std::string>& pieceColors,
  int cellSize = 64);
