#include "Kizuri/Assets/MeshImporter.h"
#include "Kizuri/Assets/Guid.h"
#include "Kizuri/Assets/TexCodec.h"
#include "Kizuri/Assets/TextureImporter.h"
#include "Kizuri/Assets/BasisuDecode.h"
#include "Kizuri/Log.h"
#include <cgltf.h>
#include <meshoptimizer.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <system_error>
namespace Kizuri {
namespace {
int Base64Value(char c) {
  if (c >= 'A' && c <= 'Z') {
    return c - 'A';
  }
  if (c >= 'a' && c <= 'z') {
    return c - 'a' + 26;
  }
  if (c >= '0' && c <= '9') {
    return c - '0' + 52;
  }
  if (c == '+' || c == '-') {
    return 62;
  }
  if (c == '/' || c == '_') {
    return 63;
  }
  return -1;
}
bool Base64Decode(const std::string& in, std::vector<unsigned char>& out) {
  out.clear();
  int bits = 0;
  int acc = 0;
  for (size_t i = 0; i < in.size(); ++i) {
    char c = in[i];
    if (c == '=' || c == ' ' || c == '\n' || c == '\r' || c == '\t') {
      continue;
    }
    int v = Base64Value(c);
    if (v < 0) {
      return false;
    }
    acc = (acc << 6) | v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<unsigned char>((acc >> bits) & 0xFF));
    }
  }
  return true;
}
bool ResolveGltfImage(const cgltf_image* img, const std::string& glbPath, std::vector<unsigned char>& bytes, std::string& nameHint) {
  bytes.clear();
  nameHint = "gltf_image";
  if (img == nullptr) {
    return false;
  }
  if (img->name != nullptr && img->name[0] != '\0') {
    nameHint = img->name;
  }
  if (img->uri != nullptr && img->uri[0] != '\0') {
    std::string uri(img->uri);
    if (uri.compare(0, 5, "data:") == 0) {
      size_t comma = uri.find(',');
      if (comma == std::string::npos) {
        return false;
      }
      return Base64Decode(uri.substr(comma + 1), bytes) && !bytes.empty();
    }
    std::filesystem::path glb(glbPath);
    std::filesystem::path full = glb.parent_path() / uri;
    nameHint = full.filename().string();
    FILE* fp = std::fopen(full.string().c_str(), "rb");
    if (fp == nullptr) {
      return false;
    }
    std::fseek(fp, 0, SEEK_END);
    long sz = std::ftell(fp);
    std::fseek(fp, 0, SEEK_SET);
    if (sz <= 0) {
      std::fclose(fp);
      return false;
    }
    bytes.resize(static_cast<size_t>(sz));
    size_t n = std::fread(bytes.data(), 1, bytes.size(), fp);
    std::fclose(fp);
    return n == bytes.size();
  }
  if (img->buffer_view != nullptr) {
    const unsigned char* base = static_cast<const unsigned char*>(cgltf_buffer_view_data(img->buffer_view));
    size_t sz = img->buffer_view->size;
    if (base == nullptr) {
      return false;
    }
    bytes.assign(base, base + sz);
    return !bytes.empty();
  }
  return false;
}
bool ReadFloatComp(const unsigned char* p, cgltf_component_type ct, bool normalized, float& v) {
  switch (ct) {
    case cgltf_component_type_r_32f: {
      float f = 0.0f;
      std::memcpy(&f, p, 4);
      v = f;
      return true;
    }
    case cgltf_component_type_r_32u: {
      uint32_t u = 0;
      std::memcpy(&u, p, 4);
      v = normalized ? static_cast<float>(u) / 4294967295.0f : static_cast<float>(u);
      return true;
    }
    case cgltf_component_type_r_16u: {
      uint16_t u = 0;
      std::memcpy(&u, p, 2);
      v = normalized ? static_cast<float>(u) / 65535.0f : static_cast<float>(u);
      return true;
    }
    case cgltf_component_type_r_8u: {
      v = normalized ? static_cast<float>(*p) / 255.0f : static_cast<float>(*p);
      return true;
    }
    case cgltf_component_type_r_16: {
      int16_t s = 0;
      std::memcpy(&s, p, 2);
      float f = static_cast<float>(s) / 32767.0f;
      v = normalized ? (f < -1.0f ? -1.0f : (f > 1.0f ? 1.0f : f)) : static_cast<float>(s);
      return true;
    }
    case cgltf_component_type_r_8: {
      int8_t s = 0;
      std::memcpy(&s, p, 1);
      float f = static_cast<float>(s) / 127.0f;
      v = normalized ? (f < -1.0f ? -1.0f : (f > 1.0f ? 1.0f : f)) : static_cast<float>(s);
      return true;
    }
    default:
      return false;
  }
}
bool ReadVec3(const cgltf_accessor* acc, size_t idx, float* v) {
  if (acc == nullptr || acc->type != cgltf_type_vec3) {
    return false;
  }
  if (idx >= acc->count || acc->buffer_view == nullptr) {
    return false;
  }
  const unsigned char* base = static_cast<const unsigned char*>(cgltf_buffer_view_data(acc->buffer_view));
  if (base == nullptr) {
    return false;
  }
  size_t stride = acc->stride != 0 ? acc->stride : 12;
  const unsigned char* e = base + acc->offset + idx * stride;
  size_t comp = 0;
  switch (acc->component_type) {
    case cgltf_component_type_r_32f: comp = 4; break;
    case cgltf_component_type_r_16u: case cgltf_component_type_r_16: comp = 2; break;
    case cgltf_component_type_r_8u: case cgltf_component_type_r_8: comp = 1; break;
    default: return false;
  }
  for (int c = 0; c < 3; ++c) {
    if (!ReadFloatComp(e + c * comp, acc->component_type, acc->normalized != 0, v[c])) {
      return false;
    }
  }
  return true;
}
bool ReadVec2(const cgltf_accessor* acc, size_t idx, float* v) {
  if (acc == nullptr || acc->type != cgltf_type_vec2) {
    return false;
  }
  if (idx >= acc->count || acc->buffer_view == nullptr) {
    return false;
  }
  const unsigned char* base = static_cast<const unsigned char*>(cgltf_buffer_view_data(acc->buffer_view));
  if (base == nullptr) {
    return false;
  }
  size_t stride = acc->stride != 0 ? acc->stride : 8;
  const unsigned char* e = base + acc->offset + idx * stride;
  size_t comp = 0;
  switch (acc->component_type) {
    case cgltf_component_type_r_32f: comp = 4; break;
    case cgltf_component_type_r_16u: case cgltf_component_type_r_16: comp = 2; break;
    case cgltf_component_type_r_8u: case cgltf_component_type_r_8: comp = 1; break;
    default: return false;
  }
  for (int c = 0; c < 2; ++c) {
    if (!ReadFloatComp(e + c * comp, acc->component_type, acc->normalized != 0, v[c])) {
      return false;
    }
  }
  return true;
}
bool ReadVec4(const cgltf_accessor* acc, size_t idx, float* v) {
  if (acc == nullptr || acc->type != cgltf_type_vec4) {
    return false;
  }
  if (idx >= acc->count || acc->buffer_view == nullptr) {
    return false;
  }
  const unsigned char* base = static_cast<const unsigned char*>(cgltf_buffer_view_data(acc->buffer_view));
  if (base == nullptr) {
    return false;
  }
  size_t stride = acc->stride != 0 ? acc->stride : 16;
  const unsigned char* e = base + acc->offset + idx * stride;
  size_t comp = 0;
  switch (acc->component_type) {
    case cgltf_component_type_r_32f: comp = 4; break;
    case cgltf_component_type_r_16u: case cgltf_component_type_r_16: comp = 2; break;
    case cgltf_component_type_r_8u: case cgltf_component_type_r_8: comp = 1; break;
    default: return false;
  }
  for (int c = 0; c < 4; ++c) {
    if (!ReadFloatComp(e + c * comp, acc->component_type, acc->normalized != 0, v[c])) {
      return false;
    }
  }
  return true;
}
bool ReadIndex(const cgltf_accessor* acc, size_t idx, uint32_t& v) {
  if (acc == nullptr || acc->type != cgltf_type_scalar || acc->buffer_view == nullptr) {
    return false;
  }
  if (idx >= acc->count) {
    return false;
  }
  const unsigned char* base = static_cast<const unsigned char*>(cgltf_buffer_view_data(acc->buffer_view));
  if (base == nullptr) {
    return false;
  }
  size_t off = acc->offset + idx * acc->stride;
  if (acc->component_type == cgltf_component_type_r_16u) {
    v = *reinterpret_cast<const uint16_t*>(base + off);
    return true;
  }
  if (acc->component_type == cgltf_component_type_r_32u) {
    v = *reinterpret_cast<const uint32_t*>(base + off);
    return true;
  }
  return false;
}
bool ImportEmbeddedTexture(const cgltf_texture* ctex, const std::string& glbPath, uint64_t hash, int& texCounter, TexKind kind, std::string& guidOut) {
  guidOut.clear();
  if (ctex == nullptr) {
    return false;
  }
  bool isBasis = ctex->has_basisu != 0 && ctex->basisu_image != nullptr;
  const cgltf_image* img = isBasis ? ctex->basisu_image : ctex->image;
  if (img == nullptr) {
    return false;
  }
  std::vector<unsigned char> imgBytes;
  std::string imgName;
  if (!ResolveGltfImage(img, glbPath, imgBytes, imgName)) {
    return false;
  }
  std::filesystem::path glbFile(glbPath);
  std::string texName = glbFile.stem().string() + "_tex" + std::to_string(texCounter++) + ".kztex";
  std::string texOut = (glbFile.parent_path() / texName).string();
  std::string keepTex;
  TextureAssetData probe;
  if (DecodeTextureFile(texOut, probe) && !probe.guid.empty()) {
    keepTex = probe.guid;
  }
  TextureAssetData tdata;
  if (isBasis) {
    std::vector<unsigned char> rgba;
    int w = 0;
    int h = 0;
    if (!DecodeBasisu(imgBytes.data(), imgBytes.size(), rgba, w, h)) {
      return false;
    }
    imgBytes.clear();
    imgBytes.shrink_to_fit();
    if (!CompressTexturePixels(rgba.data(), w, h, keepTex, tdata, imgName, kind, glbPath, hash)) {
      return false;
    }
  } else {
    if (!ImportTextureMemory(imgBytes.data(), imgBytes.size(), keepTex, tdata, imgName, kind, glbPath, hash)) {
      return false;
    }
    imgBytes.clear();
    imgBytes.shrink_to_fit();
  }
  if (!EncodeTextureFile(tdata, texOut)) {
    return false;
  }
  guidOut = tdata.guid;
  return true;
}
bool DecodeGltfImageRgba(const cgltf_texture* ctex, const std::string& glbPath, std::vector<unsigned char>& rgba, int& w, int& h) {
  rgba.clear();
  w = 0;
  h = 0;
  if (ctex == nullptr) {
    return false;
  }
  bool isBasis = ctex->has_basisu != 0 && ctex->basisu_image != nullptr;
  const cgltf_image* img = isBasis ? ctex->basisu_image : ctex->image;
  if (img == nullptr) {
    return false;
  }
  std::vector<unsigned char> bytes;
  std::string imgName;
  if (!ResolveGltfImage(img, glbPath, bytes, imgName)) {
    return false;
  }
  if (isBasis) {
    return DecodeBasisu(bytes.data(), bytes.size(), rgba, w, h);
  }
  return DecodeTextureRgba(bytes.data(), bytes.size(), rgba, w, h);
}
bool ImportOrmTextures(const cgltf_material* mat, const std::string& glbPath, uint64_t hash, int& texCounter, const MeshMaterialData& md, std::string& guidOut) {
  guidOut.clear();
  if (mat == nullptr) {
    return false;
  }
  const cgltf_texture_view* mrView = nullptr;
  if (mat->has_pbr_metallic_roughness && mat->pbr_metallic_roughness.metallic_roughness_texture.texture != nullptr) {
    mrView = &mat->pbr_metallic_roughness.metallic_roughness_texture;
  }
  const cgltf_texture_view* occView = nullptr;
  if (mat->occlusion_texture.texture != nullptr) {
    occView = &mat->occlusion_texture;
  }
  if (mrView == nullptr && occView == nullptr) {
    return false;
  }
  std::vector<unsigned char> mrPx;
  int mrW = 0;
  int mrH = 0;
  bool hasMr = mrView != nullptr && DecodeGltfImageRgba(mrView->texture, glbPath, mrPx, mrW, mrH);
  std::vector<unsigned char> ocPx;
  int ocW = 0;
  int ocH = 0;
  bool hasOc = occView != nullptr && DecodeGltfImageRgba(occView->texture, glbPath, ocPx, ocW, ocH);
  if (!hasMr && !hasOc) {
    return false;
  }
  int W = hasMr ? mrW : ocW;
  int H = hasMr ? mrH : ocH;
  if (W <= 0 || H <= 0) {
    return false;
  }
  float occStrength = 1.0f;
  if (hasOc && occView->scale != 0.0f) {
    occStrength = occView->scale;
  }
  std::vector<unsigned char> orm(static_cast<size_t>(W) * static_cast<size_t>(H) * 4);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      size_t d = (static_cast<size_t>(y) * static_cast<size_t>(W) + static_cast<size_t>(x)) * 4;
      float r = 255.0f;
      float g = md.roughness * 255.0f;
      float b = md.metallic * 255.0f;
      if (hasOc) {
        int sx = x * ocW / W;
        int sy = y * ocH / H;
        float occR = static_cast<float>(ocPx[(static_cast<size_t>(sy) * static_cast<size_t>(ocW) + static_cast<size_t>(sx)) * 4 + 0]);
        r = 255.0f * (1.0f + occStrength * (occR / 255.0f - 1.0f));
      }
      if (hasMr) {
        int sx = x * mrW / W;
        int sy = y * mrH / H;
        size_t s = (static_cast<size_t>(sy) * static_cast<size_t>(mrW) + static_cast<size_t>(sx)) * 4;
        g = md.roughness * static_cast<float>(mrPx[s + 1]);
        b = md.metallic * static_cast<float>(mrPx[s + 2]);
      }
      if (r < 0.0f) {
        r = 0.0f;
      } else if (r > 255.0f) {
        r = 255.0f;
      }
      if (g < 0.0f) {
        g = 0.0f;
      } else if (g > 255.0f) {
        g = 255.0f;
      }
      if (b < 0.0f) {
        b = 0.0f;
      } else if (b > 255.0f) {
        b = 255.0f;
      }
      orm[d + 0] = static_cast<unsigned char>(r + 0.5f);
      orm[d + 1] = static_cast<unsigned char>(g + 0.5f);
      orm[d + 2] = static_cast<unsigned char>(b + 0.5f);
      orm[d + 3] = 255;
    }
  }
  std::filesystem::path glbFile(glbPath);
  std::string texName = glbFile.stem().string() + "_tex" + std::to_string(texCounter++) + ".kztex";
  std::string texOut = (glbFile.parent_path() / texName).string();
  std::string keepTex;
  TextureAssetData probe;
  if (DecodeTextureFile(texOut, probe) && !probe.guid.empty()) {
    keepTex = probe.guid;
  }
  TextureAssetData tdata;
  if (!CompressTexturePixels(orm.data(), W, H, keepTex, tdata, "orm", TexKind::Orm, glbPath, hash)) {
    return false;
  }
  orm.clear();
  orm.shrink_to_fit();
  if (!EncodeTextureFile(tdata, texOut)) {
    return false;
  }
  guidOut = tdata.guid;
  return true;
}
float NewtonSqrt(float x) {
  if (x <= 0.0f) {
    return 0.0f;
  }
  float r = x < 1.0f ? 1.0f : x;
  for (int i = 0; i < 6; ++i) {
    r = 0.5f * (r + x / r);
  }
  return r;
}
void InvertNormalMatrix(const float m[16], float nm[9]) {
  float r[9] = { m[0], m[4], m[8], m[1], m[5], m[9], m[2], m[6], m[10] };
  float det = r[0] * (r[4] * r[8] - r[5] * r[7]) - r[1] * (r[3] * r[8] - r[5] * r[6]) + r[2] * (r[3] * r[7] - r[4] * r[6]);
  if (det > -1e-12f && det < 1e-12f) {
    nm[0] = 1.0f; nm[1] = 0.0f; nm[2] = 0.0f;
    nm[3] = 0.0f; nm[4] = 1.0f; nm[5] = 0.0f;
    nm[6] = 0.0f; nm[7] = 0.0f; nm[8] = 1.0f;
    return;
  }
  float inv = 1.0f / det;
  nm[0] = (r[4] * r[8] - r[5] * r[7]) * inv;
  nm[1] = (r[2] * r[7] - r[1] * r[8]) * inv;
  nm[2] = (r[1] * r[5] - r[2] * r[4]) * inv;
  nm[3] = (r[5] * r[6] - r[3] * r[8]) * inv;
  nm[4] = (r[0] * r[8] - r[2] * r[6]) * inv;
  nm[5] = (r[2] * r[3] - r[0] * r[5]) * inv;
  nm[6] = (r[3] * r[7] - r[4] * r[6]) * inv;
  nm[7] = (r[1] * r[6] - r[0] * r[7]) * inv;
  nm[8] = (r[0] * r[4] - r[1] * r[3]) * inv;
}
void ReadTextureXform(const cgltf_texture_view& view, MeshMaterialData& md, int slot) {
  if (slot < 0 || slot > 4) {
    return;
  }
  MeshUvXform& x = md.xforms[slot];
  x.offsetU = 0.0f;
  x.offsetV = 0.0f;
  x.scaleU = 1.0f;
  x.scaleV = 1.0f;
  x.rotation = 0.0f;
  x.uvSet = view.texcoord >= 0 && view.texcoord <= 8 ? view.texcoord : 0;
  if (view.has_transform == 0) {
    return;
  }
  x.offsetU = view.transform.offset[0];
  x.offsetV = view.transform.offset[1];
  x.scaleU = view.transform.scale[0];
  x.scaleV = view.transform.scale[1];
  x.rotation = view.transform.rotation;
  if (view.transform.has_texcoord != 0 && view.transform.texcoord >= 0 && view.transform.texcoord <= 8) {
    x.uvSet = view.transform.texcoord;
  }
}
void ParseMaterial(const cgltf_material* cmat, const std::string& glbPath, uint64_t hash, int& texCounter, MeshMaterialData& md, LogStore* log) {
  md.name = cmat->name != nullptr ? cmat->name : "Material";
  md.doubleSided = cmat->double_sided != 0;
  if (cmat->alpha_mode == cgltf_alpha_mode_mask) {
    md.alphaMode = 1;
  } else if (cmat->alpha_mode == cgltf_alpha_mode_blend) {
    md.alphaMode = 2;
  } else {
    md.alphaMode = 0;
  }
  md.alphaCutoff = cmat->alpha_cutoff;
  md.emissive[0] = cmat->emissive_factor[0];
  md.emissive[1] = cmat->emissive_factor[1];
  md.emissive[2] = cmat->emissive_factor[2];
  md.emissiveStrength = cmat->has_emissive_strength != 0 ? cmat->emissive_strength.emissive_strength : 1.0f;
  md.unlit = cmat->unlit != 0;
  if (cmat->has_pbr_specular_glossiness != 0) {
    const cgltf_pbr_specular_glossiness& sg = cmat->pbr_specular_glossiness;
    md.albedo[0] = sg.diffuse_factor[0];
    md.albedo[1] = sg.diffuse_factor[1];
    md.albedo[2] = sg.diffuse_factor[2];
    float gloss = sg.glossiness_factor;
    if (gloss < 0.0f) {
      gloss = 0.0f;
    }
    if (gloss > 1.0f) {
      gloss = 1.0f;
    }
    md.roughness = 1.0f - gloss;
    md.metallic = 0.0f;
    ImportEmbeddedTexture(sg.diffuse_texture.texture, glbPath, hash, texCounter, TexKind::Color, md.albedoTexGuid);
    ImportEmbeddedTexture(cmat->normal_texture.texture, glbPath, hash, texCounter, TexKind::Normal, md.normalTexGuid);
    ReadTextureXform(sg.diffuse_texture, md, 0);
    ReadTextureXform(cmat->normal_texture, md, 1);
    const cgltf_texture* sgTex = sg.specular_glossiness_texture.texture;
    if (sgTex != nullptr) {
      std::vector<unsigned char> rgba;
      int w = 0;
      int h = 0;
      if (DecodeGltfImageRgba(sgTex, glbPath, rgba, w, h)) {
        double lumSum = 0.0;
        size_t lumCount = static_cast<size_t>(w) * static_cast<size_t>(h);
        for (size_t li = 0; li < lumCount; ++li) {
          lumSum += (static_cast<double>(rgba[li * 4 + 0]) + static_cast<double>(rgba[li * 4 + 1]) + static_cast<double>(rgba[li * 4 + 2])) / (3.0 * 255.0);
        }
        float avgLum = lumCount > 0 ? static_cast<float>(lumSum / static_cast<double>(lumCount)) : 0.0f;
        if (avgLum > 0.15f) {
          md.metallic = (avgLum - 0.04f) / 0.96f;
          if (md.metallic < 0.0f) {
            md.metallic = 0.0f;
          }
          if (md.metallic > 1.0f) {
            md.metallic = 1.0f;
          }
          if (log != nullptr) {
            log->Add(LogLevel::Warning, "spec-gloss metallic inferred from specular texture");
          }
        }
        std::vector<unsigned char> orm(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
        for (int y = 0; y < h; ++y) {
          for (int x = 0; x < w; ++x) {
            size_t s = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 4;
            orm[s + 0] = 255;
            orm[s + 1] = static_cast<unsigned char>(md.roughness * 255.0f + 0.5f);
            orm[s + 2] = static_cast<unsigned char>(md.metallic * 255.0f + 0.5f);
            orm[s + 3] = 255;
          }
        }
        std::filesystem::path glbFile(glbPath);
        std::string texName = glbFile.stem().string() + "_tex" + std::to_string(texCounter++) + ".kztex";
        std::string texOut = (glbFile.parent_path() / texName).string();
        TextureAssetData tdata;
        if (CompressTexturePixels(orm.data(), w, h, "", tdata, "orm", TexKind::Orm, glbPath, hash) && EncodeTextureFile(tdata, texOut)) {
          md.ormTexGuid = tdata.guid;
        }
      }
    }
  } else if (cmat->has_pbr_metallic_roughness != 0) {
    const cgltf_pbr_metallic_roughness& mr = cmat->pbr_metallic_roughness;
    md.albedo[0] = mr.base_color_factor[0];
    md.albedo[1] = mr.base_color_factor[1];
    md.albedo[2] = mr.base_color_factor[2];
    md.metallic = mr.metallic_factor;
    md.roughness = mr.roughness_factor;
    ImportEmbeddedTexture(mr.base_color_texture.texture, glbPath, hash, texCounter, TexKind::Color, md.albedoTexGuid);
    ImportEmbeddedTexture(cmat->normal_texture.texture, glbPath, hash, texCounter, TexKind::Normal, md.normalTexGuid);
    ImportOrmTextures(cmat, glbPath, hash, texCounter, md, md.ormTexGuid);
    ReadTextureXform(mr.base_color_texture, md, 0);
    ReadTextureXform(cmat->normal_texture, md, 1);
    ReadTextureXform(mr.metallic_roughness_texture, md, 2);
  }
  ImportEmbeddedTexture(cmat->emissive_texture.texture, glbPath, hash, texCounter, TexKind::Color, md.emissiveTexGuid);
  ReadTextureXform(cmat->emissive_texture, md, 4);
  if (cmat->has_ior != 0) {
    md.ior = cmat->ior.ior;
  }
  if (cmat->has_specular != 0) {
    md.specularColor[0] = cmat->specular.specular_color_factor[0];
    md.specularColor[1] = cmat->specular.specular_color_factor[1];
    md.specularColor[2] = cmat->specular.specular_color_factor[2];
    md.specularStrength = cmat->specular.specular_factor;
    ImportEmbeddedTexture(cmat->specular.specular_texture.texture, glbPath, hash, texCounter, TexKind::Orm, md.specularTexGuid);
    ImportEmbeddedTexture(cmat->specular.specular_color_texture.texture, glbPath, hash, texCounter, TexKind::Color, md.specularColorTexGuid);
  }
  if (cmat->has_clearcoat != 0) {
    md.clearcoat = cmat->clearcoat.clearcoat_factor;
    md.clearcoatRoughness = cmat->clearcoat.clearcoat_roughness_factor;
    ImportEmbeddedTexture(cmat->clearcoat.clearcoat_texture.texture, glbPath, hash, texCounter, TexKind::Orm, md.clearcoatTexGuid);
    ImportEmbeddedTexture(cmat->clearcoat.clearcoat_roughness_texture.texture, glbPath, hash, texCounter, TexKind::Orm, md.clearcoatRoughnessTexGuid);
    ImportEmbeddedTexture(cmat->clearcoat.clearcoat_normal_texture.texture, glbPath, hash, texCounter, TexKind::Normal, md.clearcoatNormalTexGuid);
  }
  if (cmat->has_sheen != 0) {
    md.sheenColor[0] = cmat->sheen.sheen_color_factor[0];
    md.sheenColor[1] = cmat->sheen.sheen_color_factor[1];
    md.sheenColor[2] = cmat->sheen.sheen_color_factor[2];
    md.sheenRoughness = cmat->sheen.sheen_roughness_factor;
    ImportEmbeddedTexture(cmat->sheen.sheen_color_texture.texture, glbPath, hash, texCounter, TexKind::Color, md.sheenColorTexGuid);
    ImportEmbeddedTexture(cmat->sheen.sheen_roughness_texture.texture, glbPath, hash, texCounter, TexKind::Orm, md.sheenRoughnessTexGuid);
  }
  if (cmat->has_transmission != 0) {
    md.transmission = cmat->transmission.transmission_factor;
    ImportEmbeddedTexture(cmat->transmission.transmission_texture.texture, glbPath, hash, texCounter, TexKind::Orm, md.transmissionTexGuid);
  }
  if (cmat->has_volume != 0) {
    md.thickness = cmat->volume.thickness_factor;
    md.attenuationColor[0] = cmat->volume.attenuation_color[0];
    md.attenuationColor[1] = cmat->volume.attenuation_color[1];
    md.attenuationColor[2] = cmat->volume.attenuation_color[2];
    md.attenuationDistance = cmat->volume.attenuation_distance;
    ImportEmbeddedTexture(cmat->volume.thickness_texture.texture, glbPath, hash, texCounter, TexKind::Orm, md.thicknessTexGuid);
  }
  if (cmat->has_iridescence != 0) {
    md.iridescence = cmat->iridescence.iridescence_factor;
    md.iridescenceIor = cmat->iridescence.iridescence_ior;
    md.iridescenceThicknessMin = cmat->iridescence.iridescence_thickness_min;
    md.iridescenceThicknessMax = cmat->iridescence.iridescence_thickness_max;
    ImportEmbeddedTexture(cmat->iridescence.iridescence_texture.texture, glbPath, hash, texCounter, TexKind::Orm, md.iridescenceTexGuid);
    ImportEmbeddedTexture(cmat->iridescence.iridescence_thickness_texture.texture, glbPath, hash, texCounter, TexKind::Orm, md.iridescenceThicknessTexGuid);
  }
  if (cmat->has_diffuse_transmission != 0) {
    if (log != nullptr) {
      log->Add(LogLevel::Warning, "diffuse transmission folded into transmission lobe");
    }
    float dt = cmat->diffuse_transmission.diffuse_transmission_factor;
    if (dt > md.transmission) {
      md.transmission = dt;
    }
    if (md.transmissionTexGuid.empty()) {
      ImportEmbeddedTexture(cmat->diffuse_transmission.diffuse_transmission_texture.texture, glbPath, hash, texCounter, TexKind::Orm, md.transmissionTexGuid);
    }
  }
  if (cmat->has_anisotropy != 0) {
    md.anisotropy = cmat->anisotropy.anisotropy_strength;
    md.anisotropyRotation = cmat->anisotropy.anisotropy_rotation;
    ImportEmbeddedTexture(cmat->anisotropy.anisotropy_texture.texture, glbPath, hash, texCounter, TexKind::Orm, md.anisotropyTexGuid);
  }
  if (cmat->has_dispersion != 0) {
    md.dispersion = cmat->dispersion.dispersion;
  }
}
uint32_t ParseMaterialCached(cgltf_data* data, const cgltf_material* cmat, const std::string& glbPath, uint64_t hash, int& texCounter, MeshAssetData& out, std::vector<long>& matMap, LogStore* log) {
  long gi = static_cast<long>(cmat - data->materials);
  if (gi >= 0 && static_cast<size_t>(gi) < matMap.size() && matMap[static_cast<size_t>(gi)] >= 0) {
    return static_cast<uint32_t>(matMap[static_cast<size_t>(gi)]);
  }
  MeshMaterialData md;
  ParseMaterial(cmat, glbPath, hash, texCounter, md, log);
  uint32_t idx = static_cast<uint32_t>(out.materials.size());
  out.materials.push_back(md);
  if (gi >= 0 && static_cast<size_t>(gi) < matMap.size()) {
    matMap[static_cast<size_t>(gi)] = static_cast<long>(idx);
  }
  return idx;
}
void ComposeTRS(const float t[3], const float q[4], const float s[3], float out[16]) {
  float x = q[0];
  float y = q[1];
  float z = q[2];
  float w = q[3];
  float xx = x * x;
  float yy = y * y;
  float zz = z * z;
  float xy = x * y;
  float xz = x * z;
  float yz = y * z;
  float wx = w * x;
  float wy = w * y;
  float wz = w * z;
  out[0] = (1.0f - 2.0f * (yy + zz)) * s[0];
  out[1] = (2.0f * (xy + wz)) * s[0];
  out[2] = (2.0f * (xz - wy)) * s[0];
  out[3] = 0.0f;
  out[4] = (2.0f * (xy - wz)) * s[1];
  out[5] = (1.0f - 2.0f * (xx + zz)) * s[1];
  out[6] = (2.0f * (yz + wx)) * s[1];
  out[7] = 0.0f;
  out[8] = (2.0f * (xz + wy)) * s[2];
  out[9] = (2.0f * (yz - wx)) * s[2];
  out[10] = (1.0f - 2.0f * (xx + yy)) * s[2];
  out[11] = 0.0f;
  out[12] = t[0];
  out[13] = t[1];
  out[14] = t[2];
  out[15] = 1.0f;
}
void MulMat4(const float a[16], const float b[16], float out[16]) {
  float r[16];
  for (int c = 0; c < 4; ++c) {
    for (int rr = 0; rr < 4; ++rr) {
      r[c * 4 + rr] = a[0 * 4 + rr] * b[c * 4 + 0] + a[1 * 4 + rr] * b[c * 4 + 1] + a[2 * 4 + rr] * b[c * 4 + 2] + a[3 * 4 + rr] * b[c * 4 + 3];
    }
  }
  for (int k = 0; k < 16; ++k) {
    out[k] = r[k];
  }
}
}
bool ImportGltfMesh(const std::string& glbPath, const std::string& keepGuid, MeshAssetData& out, LogStore* log) {
  out = MeshAssetData();
  if (glbPath.empty()) {
    return false;
  }
  uint64_t hash = 0;
  if (!Fnv1a64File(glbPath, hash)) {
    return false;
  }
  cgltf_options options;
  std::memset(&options, 0, sizeof(options));
  cgltf_data* data = nullptr;
  if (cgltf_parse_file(&options, glbPath.c_str(), &data) != cgltf_result_success) {
    return false;
  }
  bool ok = false;
  int texCounter = 0;
  bool allTangent = true;
  std::vector<long> matMap(data->materials_count, -1);
  if (cgltf_load_buffers(&options, data, glbPath.c_str()) == cgltf_result_success && data->meshes_count > 0) {
    ok = true;
    std::vector<std::vector<unsigned char>> meshoptBlobs;
    meshoptBlobs.reserve(data->buffer_views_count);
    for (size_t bi = 0; bi < data->buffer_views_count && ok; ++bi) {
      cgltf_buffer_view& bv = data->buffer_views[bi];
      if (bv.has_meshopt_compression == 0) {
        continue;
      }
      const cgltf_meshopt_compression& mc = bv.meshopt_compression;
      if (mc.buffer == nullptr || mc.buffer->data == nullptr || mc.count == 0 || mc.stride == 0) {
        ok = false;
        break;
      }
      size_t srcOff = mc.offset;
      if (srcOff + mc.size > mc.buffer->size) {
        ok = false;
        break;
      }
      const unsigned char* src = static_cast<const unsigned char*>(mc.buffer->data) + srcOff;
      std::vector<unsigned char> dst(mc.count * mc.stride, 0);
      int rc = -1;
      if (mc.mode == cgltf_meshopt_compression_mode_attributes) {
        rc = meshopt_decodeVertexBuffer(dst.data(), mc.count, mc.stride, src, mc.size);
        if (rc == 0) {
          if (mc.filter == cgltf_meshopt_compression_filter_octahedral) {
            meshopt_decodeFilterOct(dst.data(), mc.count, mc.stride);
          } else if (mc.filter == cgltf_meshopt_compression_filter_quaternion) {
            meshopt_decodeFilterQuat(dst.data(), mc.count, mc.stride);
          } else if (mc.filter == cgltf_meshopt_compression_filter_exponential) {
            meshopt_decodeFilterExp(dst.data(), mc.count, mc.stride);
          }
        }
      } else if (mc.mode == cgltf_meshopt_compression_mode_triangles) {
        if (mc.stride != 2 && mc.stride != 4) {
          ok = false;
          break;
        }
        rc = meshopt_decodeIndexBuffer(dst.data(), mc.count, mc.stride, src, mc.size);
      } else if (mc.mode == cgltf_meshopt_compression_mode_indices) {
        if (mc.stride != 2 && mc.stride != 4) {
          ok = false;
          break;
        }
        rc = meshopt_decodeIndexSequence(dst.data(), mc.count, mc.stride, src, mc.size);
      } else {
        ok = false;
        break;
      }
      if (rc != 0) {
        ok = false;
        break;
      }
      meshoptBlobs.push_back(dst);
      bv.data = meshoptBlobs.back().data();
      bv.size = meshoptBlobs.back().size();
      bv.stride = mc.stride;
    }
    for (size_t ai = 0; ai < data->accessors_count && ok; ++ai) {
      cgltf_accessor& acc = data->accessors[ai];
      if (acc.buffer_view != nullptr && acc.buffer_view->data != nullptr && acc.stride == 0) {
        acc.stride = acc.buffer_view->stride;
      }
    }
    for (size_t vi = 0; vi < data->variants_count; ++vi) {
      out.variantNames.push_back(data->variants[vi].name != nullptr ? data->variants[vi].name : "");
    }
    struct MeshInstance {
      size_t meshIndex;
      float m[16];
    };
    std::vector<MeshInstance> instances;
    std::vector<char> meshReferenced(data->meshes_count, 0);
    for (size_t ni = 0; ni < data->nodes_count && ok; ++ni) {
      const cgltf_node& node = data->nodes[ni];
      if (node.mesh == nullptr || node.mesh < data->meshes || node.mesh >= data->meshes + data->meshes_count) {
        continue;
      }
      MeshInstance inst;
      inst.meshIndex = static_cast<size_t>(node.mesh - data->meshes);
      cgltf_node_transform_world(&node, inst.m);
      meshReferenced[inst.meshIndex] = 1;
      instances.push_back(inst);
    }
    for (size_t mi = 0; mi < data->meshes_count && ok; ++mi) {
      if (meshReferenced[mi] != 0) {
        continue;
      }
      MeshInstance inst;
      inst.meshIndex = mi;
      for (int k = 0; k < 16; ++k) {
        inst.m[k] = 0.0f;
      }
      inst.m[0] = 1.0f;
      inst.m[5] = 1.0f;
      inst.m[10] = 1.0f;
      inst.m[15] = 1.0f;
      instances.push_back(inst);
    }
    for (size_t ni = 0; ni < data->nodes_count && ok; ++ni) {
      const cgltf_node& node = data->nodes[ni];
      if (node.has_mesh_gpu_instancing == 0 || node.mesh == nullptr) {
        continue;
      }
      const cgltf_accessor* tAcc = nullptr;
      const cgltf_accessor* rAcc = nullptr;
      const cgltf_accessor* sAcc = nullptr;
      for (size_t ai = 0; ai < node.mesh_gpu_instancing.attributes_count; ++ai) {
        const char* an = node.mesh_gpu_instancing.attributes[ai].name;
        if (std::strcmp(an, "TRANSLATION") == 0) {
          tAcc = node.mesh_gpu_instancing.attributes[ai].data;
        } else if (std::strcmp(an, "ROTATION") == 0) {
          rAcc = node.mesh_gpu_instancing.attributes[ai].data;
        } else if (std::strcmp(an, "SCALE") == 0) {
          sAcc = node.mesh_gpu_instancing.attributes[ai].data;
        }
      }
      size_t n = 0;
      if (tAcc != nullptr) {
        n = tAcc->count;
      }
      if (rAcc != nullptr && (n == 0 || rAcc->count < n)) {
        n = rAcc->count;
      }
      if (sAcc != nullptr && (n == 0 || sAcc->count < n)) {
        n = sAcc->count;
      }
      if (n == 0) {
        continue;
      }
      float nodeM[16];
      cgltf_node_transform_world(&node, nodeM);
      for (size_t gi2 = 0; gi2 < n; ++gi2) {
        float t[3] = { 0, 0, 0 };
        float q[4] = { 0, 0, 0, 1 };
        float s[3] = { 1, 1, 1 };
        if (tAcc != nullptr && !ReadVec3(tAcc, gi2, t)) {
          ok = false;
          break;
        }
        if (rAcc != nullptr && !ReadVec4(rAcc, gi2, q)) {
          ok = false;
          break;
        }
        if (sAcc != nullptr && !ReadVec3(sAcc, gi2, s)) {
          ok = false;
          break;
        }
        float local[16];
        ComposeTRS(t, q, s, local);
        MeshInstance inst;
        inst.meshIndex = static_cast<size_t>(node.mesh - data->meshes);
        MulMat4(nodeM, local, inst.m);
        instances.push_back(inst);
      }
    }
    for (size_t ni = 0; ni < data->nodes_count && ok; ++ni) {
      const cgltf_node& node = data->nodes[ni];
      if (node.light == nullptr) {
        continue;
      }
      const cgltf_light* cl = node.light;
      MeshLightData ld;
      ld.name = cl->name != nullptr ? cl->name : (node.name != nullptr ? node.name : "Light");
      if (cl->type == cgltf_light_type_spot) {
        ld.type = 1;
      } else if (cl->type == cgltf_light_type_directional) {
        ld.type = 2;
      } else {
        ld.type = 0;
      }
      ld.color[0] = cl->color[0];
      ld.color[1] = cl->color[1];
      ld.color[2] = cl->color[2];
      ld.intensity = cl->intensity;
      ld.range = cl->range;
      ld.innerCone = cl->spot_inner_cone_angle;
      ld.outerCone = cl->spot_outer_cone_angle;
      cgltf_node_transform_world(&node, ld.matrix);
      out.lights.push_back(ld);
    }
    for (size_t ii = 0; ii < instances.size() && ok; ++ii) {
      const cgltf_mesh& mesh = data->meshes[instances[ii].meshIndex];
      float nm[9];
      InvertNormalMatrix(instances[ii].m, nm);
      const float* nodeM = instances[ii].m;
      for (size_t pi = 0; pi < mesh.primitives_count && ok; ++pi) {
        const cgltf_primitive& prim = mesh.primitives[pi];
        if (prim.type != cgltf_primitive_type_triangles) {
          continue;
        }
        if (prim.has_draco_mesh_compression != 0) {
          if (log != nullptr) {
            log->Add(LogLevel::Warning, "draco primitive skipped; decoder arrives in Lote 2");
          }
          continue;
        }
        const cgltf_accessor* posAcc = nullptr;
        const cgltf_accessor* nrmAcc = nullptr;
        const cgltf_accessor* uvAcc = nullptr;
        const cgltf_accessor* tanAcc = nullptr;
        const cgltf_accessor* uv2Acc = nullptr;
        for (size_t ai = 0; ai < prim.attributes_count; ++ai) {
          if (std::strcmp(prim.attributes[ai].name, "POSITION") == 0) {
            posAcc = prim.attributes[ai].data;
          } else if (std::strcmp(prim.attributes[ai].name, "NORMAL") == 0) {
            nrmAcc = prim.attributes[ai].data;
          } else if (std::strcmp(prim.attributes[ai].name, "TEXCOORD_0") == 0) {
            uvAcc = prim.attributes[ai].data;
          } else if (std::strcmp(prim.attributes[ai].name, "TANGENT") == 0) {
            tanAcc = prim.attributes[ai].data;
          } else if (std::strcmp(prim.attributes[ai].name, "TEXCOORD_1") == 0) {
            uv2Acc = prim.attributes[ai].data;
          }
        }
        if (posAcc == nullptr || posAcc->count == 0) {
          continue;
        }
        std::vector<float> primPos;
        std::vector<float> primNrm;
        std::vector<float> primUv;
        std::vector<float> primTan;
        primPos.reserve(posAcc->count * 3);
        primNrm.reserve(posAcc->count * 3);
        primUv.reserve(posAcc->count * 2);
        primTan.reserve(posAcc->count * 4);
        for (size_t vi = 0; vi < posAcc->count && ok; ++vi) {
          float p[3] = { 0, 0, 0 };
          float n[3] = { 0, 1, 0 };
          float uv[2] = { 0, 0 };
          float tg[4] = { 0, 0, 0, 1 };
          if (!ReadVec3(posAcc, vi, p)) {
            ok = false;
            break;
          }
          if (nrmAcc != nullptr && !ReadVec3(nrmAcc, vi, n)) {
            ok = false;
            break;
          }
          if (uvAcc != nullptr && !ReadVec2(uvAcc, vi, uv)) {
            ok = false;
            break;
          }
          if (tanAcc != nullptr) {
            if (!ReadVec4(tanAcc, vi, tg)) {
              ok = false;
              break;
            }
          } else {
            allTangent = false;
          }
          float wp0 = nodeM[0] * p[0] + nodeM[4] * p[1] + nodeM[8] * p[2] + nodeM[12];
          float wp1 = nodeM[1] * p[0] + nodeM[5] * p[1] + nodeM[9] * p[2] + nodeM[13];
          float wp2 = nodeM[2] * p[0] + nodeM[6] * p[1] + nodeM[10] * p[2] + nodeM[14];
          float wn0 = nm[0] * n[0] + nm[1] * n[1] + nm[2] * n[2];
          float wn1 = nm[3] * n[0] + nm[4] * n[1] + nm[5] * n[2];
          float wn2 = nm[6] * n[0] + nm[7] * n[1] + nm[8] * n[2];
          float nl = NewtonSqrt(wn0 * wn0 + wn1 * wn1 + wn2 * wn2);
          if (nl > 1e-12f) {
            wn0 /= nl;
            wn1 /= nl;
            wn2 /= nl;
          }
          primPos.push_back(wp0);
          primPos.push_back(wp1);
          primPos.push_back(-wp2);
          primNrm.push_back(wn0);
          primNrm.push_back(wn1);
          primNrm.push_back(-wn2);
          primUv.push_back(uv[0]);
          primUv.push_back(uv[1]);
          primTan.push_back(tg[0]);
          primTan.push_back(tg[1]);
          primTan.push_back(-tg[2]);
          primTan.push_back(-tg[3]);
        }
        if (!ok) {
          break;
        }
        std::vector<uint32_t> primIdx;
        if (prim.indices != nullptr) {
          if (prim.indices->count % 3 != 0) {
            ok = false;
            break;
          }
          primIdx.reserve(prim.indices->count);
          for (size_t ii = 0; ii < prim.indices->count; ++ii) {
            uint32_t idx = 0;
            if (!ReadIndex(prim.indices, ii, idx)) {
              ok = false;
              break;
            }
            primIdx.push_back(idx);
          }
        } else {
          if (posAcc->count % 3 != 0) {
            ok = false;
            break;
          }
          primIdx.reserve(posAcc->count);
          for (size_t vi = 0; vi < posAcc->count; ++vi) {
            primIdx.push_back(static_cast<uint32_t>(vi));
          }
        }
        if (!ok) {
          break;
        }
        std::vector<float> primUv2;
        std::vector<float> usePos = primPos;
        std::vector<float> useNrm = primNrm;
        std::vector<float> useUv = primUv;
        std::vector<float> useTan = primTan;
        std::vector<uint32_t> useIdx = primIdx;
        if (uv2Acc != nullptr) {
          primUv2.reserve(posAcc->count * 2);
          for (size_t vi = 0; vi < posAcc->count && ok; ++vi) {
            float uvv[2] = { 0, 0 };
            if (!ReadVec2(uv2Acc, vi, uvv)) {
              ok = false;
              break;
            }
            primUv2.push_back(uvv[0]);
            primUv2.push_back(uvv[1]);
          }
          if (!ok) {
            break;
          }
        } else {
          std::vector<float> uPos;
          std::vector<float> uNrm;
          std::vector<float> uUv;
          std::vector<float> uTan;
          std::vector<uint32_t> uIdx;
          if (UnwrapPrim(primPos, primNrm, primUv, primTan, primIdx, uPos, uNrm, uUv, uTan, uIdx, primUv2)) {
            usePos = uPos;
            useNrm = uNrm;
            useUv = uUv;
            useTan = uTan;
            useIdx = uIdx;
          } else {
            primUv2.assign(posAcc->count * 2, 0.0f);
          }
        }
        uint32_t baseVertex = static_cast<uint32_t>(out.positions.size() / 3);
        if (useIdx.size() % 3 != 0) {
          ok = false;
          break;
        }
        uint32_t partIdx = static_cast<uint32_t>(out.parts.size());
        out.positions.insert(out.positions.end(), usePos.begin(), usePos.end());
        out.normals.insert(out.normals.end(), useNrm.begin(), useNrm.end());
        out.uvs.insert(out.uvs.end(), useUv.begin(), useUv.end());
        out.tangents.insert(out.tangents.end(), useTan.begin(), useTan.end());
        out.uv2s.insert(out.uv2s.end(), primUv2.begin(), primUv2.end());
        out.indices.reserve(out.indices.size() + useIdx.size());
        for (size_t ii = 0; ii < useIdx.size(); ii += 3) {
          out.indices.push_back(baseVertex + useIdx[ii]);
          out.indices.push_back(baseVertex + useIdx[ii + 2]);
          out.indices.push_back(baseVertex + useIdx[ii + 1]);
        }
        uint32_t matIndex = 0;
        if (prim.material != nullptr && prim.material >= data->materials && prim.material < data->materials + data->materials_count) {
          matIndex = ParseMaterialCached(data, prim.material, glbPath, hash, texCounter, out, matMap, log);
        } else if (out.materials.empty()) {
          MeshMaterialData md;
          md.name = "Default";
          out.materials.push_back(md);
        }
        for (size_t mi2 = 0; mi2 < prim.mappings_count && !useIdx.empty(); ++mi2) {
          const cgltf_material_mapping& mp = prim.mappings[mi2];
          if (mp.material == nullptr || mp.material < data->materials || mp.material >= data->materials + data->materials_count) {
            continue;
          }
          uint32_t mapped = ParseMaterialCached(data, mp.material, glbPath, hash, texCounter, out, matMap, log);
          MeshVariantAssign va;
          va.part = partIdx;
          va.variant = static_cast<uint32_t>(mp.variant);
          va.material = mapped;
          out.variantAssigns.push_back(va);
        }
        MeshPartData part;
        part.indexOffset = static_cast<uint32_t>(out.indices.size()) - static_cast<uint32_t>(useIdx.size());
        part.material = matIndex;
        part.indexCount = static_cast<uint32_t>(useIdx.size());
        if (part.indexCount > 0) {
          out.parts.push_back(part);
        }
      }
    }
    if (ok && data->skins_count > 0 && log != nullptr) {
      log->Add(LogLevel::Warning, "Skinned mesh imported in bind pose; skeleton arrives in Fase 8");
    }
  }
  for (size_t bi = 0; bi < data->buffer_views_count; ++bi) {
    data->buffer_views[bi].data = nullptr;
  }
  cgltf_free(data);
  if (!ok || out.positions.empty() || out.indices.empty() || out.parts.empty()) {
    out = MeshAssetData();
    return false;
  }
  if (!allTangent && !GenerateTangents(out.positions, out.normals, out.uvs, out.indices, out.tangents)) {
    out = MeshAssetData();
    return false;
  }
  if (out.materials.empty()) {
    MeshMaterialData md;
    md.name = "Default";
    md.albedo[0] = 1.0f;
    md.albedo[1] = 1.0f;
    md.albedo[2] = 1.0f;
    md.metallic = 0.0f;
    md.roughness = 0.5f;
    out.materials.push_back(md);
    for (size_t i = 0; i < out.parts.size(); ++i) {
      out.parts[i].material = 0;
    }
  }
  out.guid = keepGuid.empty() ? GenerateGuidString() : keepGuid;
  out.hasSource = true;
  out.sourcePath = glbPath;
  out.sourceHash = hash;
  out.sourceTimestamp = 0;
  std::error_code tec;
  std::filesystem::file_time_type mtime = std::filesystem::last_write_time(glbPath, tec);
  if (!tec) {
    out.sourceTimestamp = static_cast<int64_t>(mtime.time_since_epoch().count());
  }
  ComputeAABB(out.positions, out.aabbMin, out.aabbMax);
  return true;
}
}
