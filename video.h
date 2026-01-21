#include <vector>

std::vector<std::vector<uint8_t>> generate_frames(int width, int height, int num_frames);

// Write video given RGB frames (24-bit) as vector
int createAndSaveVideo(const char* filename,
                       int width,
                       int height,
                       int fps,
                       const std::vector<std::vector<uint8_t>>& frames);
