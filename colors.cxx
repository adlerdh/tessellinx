#include "colors.h"

#include <cmath>
#include <limits>
#include <sstream>
#include <tuple>
#include <vector>

namespace
{
bool parseHex(const std::string& hex, int& r, int& g, int& b)
{
  std::string s = hex;
  if (s.size() && s[0] == '#') s = s.substr(1);

  if (s.size() == 6)
  {
    r = std::stoi(s.substr(0,2), nullptr, 16);
    g = std::stoi(s.substr(2,2), nullptr, 16);
    b = std::stoi(s.substr(4,2), nullptr, 16);
    return true;
  }
  else if (s.size() == 3) { // shorthand like #FAB -> #FFAA BB
    r = std::stoi(std::string(2, s[0]), nullptr, 16);
    g = std::stoi(std::string(2, s[1]), nullptr, 16);
    b = std::stoi(std::string(2, s[2]), nullptr, 16);
    return true;
  }

  return false;
}

double srgbChannelToLinear(double c)
{
  c /= 255.0;
  if (c <= 0.04045) return c / 12.92;
  return std::pow((c + 0.055) / 1.055, 2.4);
}

std::tuple<double, double, double> rgbToLab(int R, int G, int B)
{
  // linearize
  const double r = srgbChannelToLinear(R);
  const double g = srgbChannelToLinear(G);
  const double b = srgbChannelToLinear(B);

  // sRGB D65 conversion to XYZ
  const double X = r * 0.4124564 + g * 0.3575761 + b * 0.1804375;
  const double Y = r * 0.2126729 + g * 0.7151522 + b * 0.0721750;
  const double Z = r * 0.0193339 + g * 0.1191920 + b * 0.9503041;

  // Normalize by reference white D65
  const double Xn = 0.95047;
  const double Yn = 1.00000;
  const double Zn = 1.08883;

  auto f = [](double t) -> double {
    const double delta = 6.0 / 29.0;
    const double delta3 = delta * delta * delta; // 0.008856...
    if (t > delta3) return std::cbrt(t);
    return (t / (3 * delta * delta)) + (4.0 / 29.0);
  };

  const double fx = f(X / Xn);
  const double fy = f(Y / Yn);
  const double fz = f(Z / Zn);

  const double L = 116.0 * fy - 16.0;
  const double a = 500.0 * (fx - fy);
  const double bb = 200.0 * (fy - fz);

  return {L, a, bb};
}

double labDist2(const std::tuple<double,double,double>& A,
                 const std::tuple<double,double,double>& B)
{
  double dL = std::get<0>(A) - std::get<0>(B);
  double da = std::get<1>(A) - std::get<1>(B);
  double db = std::get<2>(A) - std::get<2>(B);
  return dL*dL + da*da + db*db;
}
}

AnsiColor hexToAnsi256Lab(const std::string& hex, bool background)
{
  int r_in, g_in, b_in;

  if (!parseHex(hex, r_in, g_in, b_in)) {
    return {0, background ? "\033[48;5;0m" : "\033[38;5;0m"};
  }

  // Build xterm 256-color palette
  std::vector<std::tuple<int,int,int>> palette(256);

  const int base16[16][3] = {
    {0,0,0},       {128,0,0},   {0,128,0},   {128,128,0},
    {0,0,128},     {128,0,128}, {0,128,128}, {192,192,192},
    {128,128,128}, {255,0,0},   {0,255,0},   {255,255,0},
    {0,0,255},     {255,0,255}, {0,255,255}, {255,255,255}
  };

  for (int i = 0; i < 16; ++i) {
    palette[i] = { base16[i][0], base16[i][1], base16[i][2] };
  }

  const int cube[6] = {0, 95, 135, 175, 215, 255};
  int idx = 16;

  for (int rr = 0; rr < 6; ++rr) {
    for (int gg = 0; gg < 6; ++gg) {
      for (int bb = 0; bb < 6; ++bb) {
        palette[idx++] = { cube[rr], cube[gg], cube[bb] };
      }
    }
  }

  for (int i = 0; i < 24; ++i) {
    int gray = 8 + 10 * i;
    palette[232 + i] = { gray, gray, gray };
  }

  std::vector<std::tuple<double,double,double>> palLab(256);
  for (int i = 0; i < 256; ++i) {
    int pr = std::get<0>(palette[i]);
    int pg = std::get<1>(palette[i]);
    int pb = std::get<2>(palette[i]);
    palLab[i] = rgbToLab(pr, pg, pb);
  }

  auto inLab = rgbToLab(r_in, g_in, b_in);

  double bestDist = std::numeric_limits<double>::infinity();
  int bestIndex = 0;

  for (int i = 0; i < 256; ++i) {
    const double d = labDist2(inLab, palLab[i]);
    if (d < bestDist) {
      bestDist = d;
      bestIndex = i;
    }
  }

  std::ostringstream esc;
  esc << "\033[" << (background ? "48" : "38") << ";5;" << bestIndex << "m";
  return { bestIndex, esc.str() };
}

RGB hexToRGB(const std::string& hex)
{
  std::string cleanHex = hex;

  if (!cleanHex.empty() && cleanHex[0] == '#') {
    cleanHex = cleanHex.substr(1);
  }

  if (cleanHex.size() != 6) {
    throw std::invalid_argument("HEX color must be 6 characters long");
  }

  unsigned int r, g, b;
  std::istringstream(cleanHex.substr(0, 2)) >> std::hex >> r;
  std::istringstream(cleanHex.substr(2, 2)) >> std::hex >> g;
  std::istringstream(cleanHex.substr(4, 2)) >> std::hex >> b;

  return RGB{static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)};
}
