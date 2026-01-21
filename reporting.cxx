#include "reporting.h"
#include "colors.h"

#include <fstream>
#include <iostream>

void printBoardTerminal(
  const std::vector<int>& board,
  int boardWidth, int boardHeight,
  const std::vector<std::string>& pieceColors,
  bool color)
{
  const std::string termReset = "\033[0m";
  const std::string fgBlack = "\033[38;5;0m"; // black foreground in 256-color
  const std::string fgWhite = "\033[38;5;15m";

  for (int y = 0; y < boardHeight; ++y)
  {
    for (int x = 0; x < boardWidth; ++x)
    {
      const int idx = y * boardWidth + x;
      const int pid = board[idx];

      if (pid < 0)
      {
        std::cout << "  ";
      } else
      {
        if (color) {
          const AnsiColor c = hexToAnsi256Lab(pieceColors.at(pid), true);
          std::cout << fgBlack << c.escape << "  " << /*static_cast<char>('A' + pid) <<*/ termReset;
        } else {
          std::cout << static_cast<char>('A' + pid) << " ";
        }
      }
    }
    std::cout << "\n";
  }
  std::cout << "\n";
}

std::vector<uint8_t> generateVideoFrame(
  int boardWidth, int boardHeight,
  int frameWidth, int frameHeight,
  const std::vector<int>& board,
  const std::vector<std::string>& pieceColors)
{
  std::vector<uint8_t> rgbData(frameWidth * frameHeight * 3);

  const int W = frameWidth / boardWidth;
  const int H = frameHeight / boardHeight;

  for (int y = 0; y < boardHeight; ++y) {
    for (int x = 0; x < boardWidth; ++x)
    {
      const int pid = board[y * boardWidth + x];

      const RGB color = (pid >= 0) ? hexToRGB(pieceColors.at(pid)) : RGB{0,0,0};

      for (int j = 0; j < H; ++j)
      {
        for (int i = 0; i < W; ++i)
        {
          const int idx = (H * y + j) * frameWidth + (W * x + i);
          rgbData[3 * idx + 0] = color.r;
          rgbData[3 * idx + 1] = color.g;
          rgbData[3 * idx + 2] = color.b;
        }
      }
    }
  }

  return rgbData;
}


void writeSolutionSVG(
  const std::vector<int>& board,
  int boardWidth, int boardHeight,
  const std::string& filename,
  const std::vector<std::string>& pieceColors,
  int cellSize)
{
  const int width = boardWidth * cellSize;
  const int height = boardHeight * cellSize;

  std::ofstream out(filename);
  if (!out) {
    std::cerr << "Failed to open " << filename << " for writing\n";
    return;
  }

  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
      << "width=\"" << width << "\" height=\"" << height << "\" "
      << "viewBox=\"0 0 " << width << " " << height << "\">\n";

  out << "<rect width=\"100%\" height=\"100%\" fill=\"#ffffff\"/>\n";

  for (int y = 0; y < boardHeight; ++y) {
    for (int x = 0; x < boardWidth; ++x)
    {
      int idx = y * boardWidth + x;
      int pid = board[idx];
      int rx = x * cellSize;
      int ry = y * cellSize;

      if (pid < 0)
      {
        out << "<rect x=\"" << rx << "\" y=\"" << ry << "\" width=\"" << cellSize
            << "\" height=\"" << cellSize << "\" fill=\"#ffffff\" stroke=\"#000000\" stroke-width=\"1\"/>\n";
      }
      else
      {
        const std::string color = pieceColors[static_cast<size_t>(pid) % pieceColors.size()];
        out << "<rect x=\"" << rx << "\" y=\"" << ry << "\" width=\"" << cellSize
            << "\" height=\"" << cellSize << "\" fill=\"" << color << "\" stroke=\"#000000\" stroke-width=\"1\"/>\n";

        out << "<text x=\"" << (rx + cellSize/2) << "\" y=\"" << (ry + cellSize/2 + cellSize/8)
            << "\" font-family=\"Arial\" font-size=\"" << (cellSize/2)
            << "\" fill=\"#000000\" text-anchor=\"middle\" alignment-baseline=\"middle\">"
            << static_cast<char>('A' + pid) << "</text>\n";
      }
    }
  }

  out << "</svg>\n";
  out.close();
}
