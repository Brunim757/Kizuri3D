#include "Kizuri/Assets/WebpDecode.h"
#include <webp/decode.h>
#include <stdint.h>
namespace Kizuri {
bool DecodeWebp(const void* bytes, size_t size, std::vector<unsigned char>& rgba, int& w, int& h) {
  rgba.clear();
  w = 0;
  h = 0;
  if (bytes == nullptr || size < 12) {
    return false;
  }
  int width = 0;
  int height = 0;
  if (WebPGetInfo(static_cast<const uint8_t*>(bytes), size, &width, &height) == 0) {
    return false;
  }
  if (width <= 0 || height <= 0 || width > 16384 || height > 16384) {
    return false;
  }
  std::vector<unsigned char> out(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
  if (WebPDecodeRGBAInto(static_cast<const uint8_t*>(bytes), size, out.data(), out.size(), width * 4) == nullptr) {
    return false;
  }
  rgba = out;
  w = width;
  h = height;
  return true;
}
}
