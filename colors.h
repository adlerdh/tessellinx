#pragma once

#include <string>

struct AnsiColor
{
  int code;
  std::string escape; // foreground escape \033[38;5;Xm
};

AnsiColor hexToAnsi256Lab(const std::string& hex, bool background = false);

struct RGB {
  uint8_t r, g, b;
};

RGB hexToRGB(const std::string& hex);
