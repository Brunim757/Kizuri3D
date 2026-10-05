#include "Kizuri/Assets/BasisuDecode.h"
#include "basisu_transcoder.h"
namespace Kizuri {
namespace {
struct BasisuInit {
  BasisuInit() {
    basist::basisu_transcoder_init();
  }
};
}
bool DecodeBasisu(const void* bytes, size_t size, std::vector<unsigned char>& rgba, int& w, int& h) {
  rgba.clear();
  w = 0;
  h = 0;
  if (bytes == nullptr || size < 64 || size > 0xFFFFFFFFu) {
    return false;
  }
  static BasisuInit g_init;
  (void)g_init;
  uint32_t ds = static_cast<uint32_t>(size);
  basist::basisu_transcoder dec;
  if (!dec.start_transcoding(bytes, ds)) {
    return false;
  }
  uint32_t ow = 0;
  uint32_t oh = 0;
  uint32_t blocks = 0;
  if (!dec.get_image_level_desc(bytes, ds, 0, 0, ow, oh, blocks)) {
    return false;
  }
  if (ow == 0 || oh == 0 || ow > 16384 || oh > 16384) {
    return false;
  }
  std::vector<unsigned char> out(static_cast<size_t>(ow) * static_cast<size_t>(oh) * 4);
  if (!dec.transcode_image_level(bytes, ds, 0, 0, out.data(), ow * oh, basist::transcoder_texture_format::cTFRGBA32)) {
    return false;
  }
  rgba = out;
  w = static_cast<int>(ow);
  h = static_cast<int>(oh);
  return true;
}
}
