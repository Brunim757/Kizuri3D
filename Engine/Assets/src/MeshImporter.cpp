#include "Kizuri/Assets/MeshImporter.h"
#include "Kizuri/Assets/Guid.h"
#include "Kizuri/Assets/TexCodec.h"
#include "Kizuri/Assets/TextureImporter.h"
#include "Kizuri/Log.h"
#include <cgltf.h>
#include <stb_image.h>
#include <math.h>
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
  if (img->buffer_view != nullptr && img->buffer_view->buffer != nullptr) {
    const unsigned char* base = static_cast<const unsigned char*>(img->buffer_view->buffer->data);
    size_t off = img->buffer_view->offset;
    size_t sz = img->buffer_view->size;
    bytes.assign(base + off, base + off + sz);
    return !bytes.empty();
  }
  return false;
}
bool ReadVec3(const cgltf_accessor* acc, size_t idx, float* v) {
  if (acc == nullptr || acc->type != cgltf_type_vec3 || acc->component_type != cgltf_component_type_r_32f) {
    return false;
  }
  if (idx >= acc->count || acc->buffer_view == nullptr || acc->buffer_view->buffer == nullptr) {
    return false;
  }
  const unsigned char* base = static_cast<const unsigned char*>(acc->buffer_view->buffer->data);
  const float* f = reinterpret_cast<const float*>(base + acc->buffer_view->offset + acc->offset + idx * acc->stride);
  v[0] = f[0];
  v[1] = f[1];
  v[2] = f[2];
  return true;
}
bool ReadVec2(const cgltf_accessor* acc, size_t idx, float* v) {
  if (acc == nullptr || acc->type != cgltf_type_vec2 || acc->component_type != cgltf_component_type_r_32f) {
    return false;
  }
  if (idx >= acc->count || acc->buffer_view == nullptr || acc->buffer_view->buffer == nullptr) {
    return false;
  }
  const unsigned char* base = static_cast<const unsigned char*>(acc->buffer_view->buffer->data);
  const float* f = reinterpret_cast<const float*>(base + acc->buffer_view->offset + acc->offset + idx * acc->stride);
  v[0] = f[0];
  v[1] = f[1];
  return true;
}
bool ReadVec4(const cgltf_accessor* acc, size_t idx, float* v) {
  if (acc == nullptr || acc->type != cgltf_type_vec4 || acc->component_type != cgltf_component_type_r_32f) {
    return false;
  }
  if (idx >= acc->count || acc->buffer_view == nullptr || acc->buffer_view->buffer == nullptr) {
    return false;
  }
  const unsigned char* base = static_cast<const unsigned char*>(acc->buffer_view->buffer->data);
  const float* f = reinterpret_cast<const float*>(base + acc->buffer_view->offset + acc->offset + idx * acc->stride);
  v[0] = f[0];
  v[1] = f[1];
  v[2] = f[2];
  v[3] = f[3];
  return true;
}
bool ReadIndex(const cgltf_accessor* acc, size_t idx, uint32_t& v) {
  if (acc == nullptr || acc->type != cgltf_type_scalar || acc->buffer_view == nullptr || acc->buffer_view->buffer == nullptr) {
    return false;
  }
  if (idx >= acc->count) {
    return false;
  }
  const unsigned char* base = static_cast<const unsigned char*>(acc->buffer_view->buffer->data);
  size_t off = acc->buffer_view->offset + acc->offset + idx * acc->stride;
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
  if (ctex == nullptr || ctex->image == nullptr) {
    return false;
  }
  std::vector<unsigned char> imgBytes;
  std::string imgName;
  if (!ResolveGltfImage(ctex->image, glbPath, imgBytes, imgName)) {
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
  if (!ImportTextureMemory(imgBytes.data(), imgBytes.size(), keepTex, tdata, imgName, kind, glbPath, hash)) {
    return false;
  }
  imgBytes.clear();
  imgBytes.shrink_to_fit();
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
  if (ctex == nullptr || ctex->image == nullptr) {
    return false;
  }
  std::vector<unsigned char> bytes;
  std::string imgName;
  if (!ResolveGltfImage(ctex->image, glbPath, bytes, imgName)) {
    return false;
  }
  int comp = 0;
  unsigned char* px = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &w, &h, &comp, 4);
  if (px == nullptr || w <= 0 || h <= 0) {
    return false;
  }
  rgba.assign(px, px + static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
  stbi_image_free(px);
  return true;
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
        const cgltf_accessor* posAcc = nullptr;
        const cgltf_accessor* nrmAcc = nullptr;
        const cgltf_accessor* uvAcc = nullptr;
        const cgltf_accessor* tanAcc = nullptr;
        for (size_t ai = 0; ai < prim.attributes_count; ++ai) {
          if (std::strcmp(prim.attributes[ai].name, "POSITION") == 0) {
            posAcc = prim.attributes[ai].data;
          } else if (std::strcmp(prim.attributes[ai].name, "NORMAL") == 0) {
            nrmAcc = prim.attributes[ai].data;
          } else if (std::strcmp(prim.attributes[ai].name, "TEXCOORD_0") == 0) {
            uvAcc = prim.attributes[ai].data;
          } else if (std::strcmp(prim.attributes[ai].name, "TANGENT") == 0) {
            tanAcc = prim.attributes[ai].data;
          }
        }
        if (posAcc == nullptr || posAcc->count == 0) {
          continue;
        }
        uint32_t baseVertex = static_cast<uint32_t>(out.positions.size() / 3);
        out.positions.reserve(out.positions.size() + posAcc->count * 3);
        out.normals.reserve(out.normals.size() + posAcc->count * 3);
        out.uvs.reserve(out.uvs.size() + posAcc->count * 2);
        out.tangents.reserve(out.tangents.size() + posAcc->count * 4);
        if (prim.indices != nullptr) {
          out.indices.reserve(out.indices.size() + prim.indices->count);
        } else {
          out.indices.reserve(out.indices.size() + posAcc->count);
        }
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
          float nl = sqrtf(wn0 * wn0 + wn1 * wn1 + wn2 * wn2);
          if (nl > 1e-12f) {
            wn0 /= nl;
            wn1 /= nl;
            wn2 /= nl;
          }
          out.positions.push_back(wp0);
          out.positions.push_back(wp1);
          out.positions.push_back(-wp2);
          out.normals.push_back(wn0);
          out.normals.push_back(wn1);
          out.normals.push_back(-wn2);
          out.uvs.push_back(uv[0]);
          out.uvs.push_back(uv[1]);
          out.tangents.push_back(tg[0]);
          out.tangents.push_back(tg[1]);
          out.tangents.push_back(-tg[2]);
          out.tangents.push_back(-tg[3]);
        }
        if (!ok) {
          break;
        }
        uint32_t matIndex = 0;
        if (prim.material != nullptr && prim.material >= data->materials && prim.material < data->materials + data->materials_count) {
          long gi = static_cast<long>(prim.material - data->materials);
          if (matMap[static_cast<size_t>(gi)] < 0) {
            MeshMaterialData md;
            md.name = prim.material->name != nullptr ? prim.material->name : "Material";
            md.albedo[0] = 1.0f;
            md.albedo[1] = 1.0f;
            md.albedo[2] = 1.0f;
            md.metallic = 0.0f;
            md.roughness = 0.5f;
            md.albedoTexGuid = "";
            if (prim.material->has_pbr_metallic_roughness) {
              md.albedo[0] = prim.material->pbr_metallic_roughness.base_color_factor[0];
              md.albedo[1] = prim.material->pbr_metallic_roughness.base_color_factor[1];
              md.albedo[2] = prim.material->pbr_metallic_roughness.base_color_factor[2];
              md.metallic = prim.material->pbr_metallic_roughness.metallic_factor;
              md.roughness = prim.material->pbr_metallic_roughness.roughness_factor;
              ImportEmbeddedTexture(prim.material->pbr_metallic_roughness.base_color_texture.texture, glbPath, hash, texCounter, TexKind::Color, md.albedoTexGuid);
              ImportEmbeddedTexture(prim.material->normal_texture.texture, glbPath, hash, texCounter, TexKind::Normal, md.normalTexGuid);
              ImportOrmTextures(prim.material, glbPath, hash, texCounter, md.ormTexGuid);
            }
            matMap[static_cast<size_t>(gi)] = static_cast<long>(out.materials.size());
            out.materials.push_back(md);
          }
          matIndex = static_cast<uint32_t>(matMap[static_cast<size_t>(gi)]);
        } else if (out.materials.empty()) {
          MeshMaterialData md;
          md.name = "Default";
          md.albedo[0] = 1.0f;
          md.albedo[1] = 1.0f;
          md.albedo[2] = 1.0f;
          md.metallic = 0.0f;
          md.roughness = 0.5f;
          out.materials.push_back(md);
        }
        MeshPartData part;
        part.indexOffset = static_cast<uint32_t>(out.indices.size());
        part.material = matIndex;
        if (prim.indices != nullptr) {
          if (prim.indices->count % 3 != 0) {
            ok = false;
            break;
          }
          for (size_t ii = 0; ii < prim.indices->count; ii += 3) {
            uint32_t i0 = 0;
            uint32_t i1 = 0;
            uint32_t i2 = 0;
            if (!ReadIndex(prim.indices, ii, i0) || !ReadIndex(prim.indices, ii + 1, i1) || !ReadIndex(prim.indices, ii + 2, i2)) {
              ok = false;
              break;
            }
            out.indices.push_back(baseVertex + i0);
            out.indices.push_back(baseVertex + i2);
            out.indices.push_back(baseVertex + i1);
          }
        } else {
          uint32_t vc = static_cast<uint32_t>(posAcc->count);
          if (vc % 3 != 0) {
            ok = false;
            break;
          }
          for (uint32_t vi = 0; vi < vc; vi += 3) {
            out.indices.push_back(baseVertex + vi);
            out.indices.push_back(baseVertex + vi + 2);
            out.indices.push_back(baseVertex + vi + 1);
          }
        }
        part.indexCount = static_cast<uint32_t>(out.indices.size()) - part.indexOffset;
        if (part.indexCount > 0) {
          out.parts.push_back(part);
        }
      }
    }
    if (ok && data->skins_count > 0 && log != nullptr) {
      log->Add(LogLevel::Warning, "Skinned mesh imported in bind pose; skeleton arrives in Fase 8");
    }
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
