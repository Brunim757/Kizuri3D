#include "Kizuri/Assets/DracoDecode.h"
#include <draco/compression/decode.h>
#include <draco/mesh/mesh.h>
#include <memory>
#include <utility>
namespace Kizuri {
namespace {
bool ReadAttFloat3(const draco::PointAttribute* att, uint32_t point, float* v) {
  if (att == nullptr || att->num_components() < 3) {
    return false;
  }
  float tmp[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
  if (!att->ConvertValue<float, 3>(att->mapped_index(draco::PointIndex(point)), tmp)) {
    return false;
  }
  v[0] = tmp[0];
  v[1] = tmp[1];
  v[2] = tmp[2];
  return true;
}
bool ReadAttFloat2(const draco::PointAttribute* att, uint32_t point, float* v) {
  if (att == nullptr || att->num_components() < 2) {
    return false;
  }
  float tmp[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
  if (!att->ConvertValue<float, 2>(att->mapped_index(draco::PointIndex(point)), tmp)) {
    return false;
  }
  v[0] = tmp[0];
  v[1] = tmp[1];
  return true;
}
}
bool DecodeDracoMesh(const void* bytes, size_t size, int posAttId, int nrmAttId, int uvAttId, DracoMeshData& out) {
  out = DracoMeshData();
  if (bytes == nullptr || size == 0 || posAttId < 0) {
    return false;
  }
  draco::DecoderBuffer buf;
  buf.Init(static_cast<const char*>(bytes), size);
  draco::Decoder dec;
  auto res = dec.DecodeMeshFromBuffer(&buf);
  if (!res.ok()) {
    return false;
  }
  std::unique_ptr<draco::Mesh> mesh = std::move(res).value();
  if (mesh == nullptr || mesh->num_faces() == 0) {
    return false;
  }
  const draco::PointAttribute* posAtt = mesh->GetAttributeByUniqueId(static_cast<uint32_t>(posAttId));
  if (posAtt == nullptr) {
    return false;
  }
  const draco::PointAttribute* nrmAtt = nrmAttId >= 0 ? mesh->GetAttributeByUniqueId(static_cast<uint32_t>(nrmAttId)) : nullptr;
  const draco::PointAttribute* uvAtt = uvAttId >= 0 ? mesh->GetAttributeByUniqueId(static_cast<uint32_t>(uvAttId)) : nullptr;
  uint32_t n = mesh->num_points();
  if (n == 0) {
    return false;
  }
  out.positions.resize(static_cast<size_t>(n) * 3, 0.0f);
  out.normals.resize(static_cast<size_t>(n) * 3, 0.0f);
  out.uvs.resize(static_cast<size_t>(n) * 2, 0.0f);
  for (uint32_t i = 0; i < n; ++i) {
    if (!ReadAttFloat3(posAtt, i, &out.positions[static_cast<size_t>(i) * 3])) {
      out = DracoMeshData();
      return false;
    }
    if (nrmAtt != nullptr) {
      ReadAttFloat3(nrmAtt, i, &out.normals[static_cast<size_t>(i) * 3]);
    }
    if (uvAtt != nullptr) {
      ReadAttFloat2(uvAtt, i, &out.uvs[static_cast<size_t>(i) * 2]);
    }
  }
  if (nrmAtt == nullptr) {
    for (uint32_t i = 0; i < n; ++i) {
      out.normals[static_cast<size_t>(i) * 3 + 1] = 1.0f;
    }
  }
  out.indices.reserve(static_cast<size_t>(mesh->num_faces()) * 3);
  for (uint32_t f = 0; f < mesh->num_faces(); ++f) {
    const draco::Mesh::Face& face = mesh->face(draco::FaceIndex(f));
    out.indices.push_back(face[0].value());
    out.indices.push_back(face[1].value());
    out.indices.push_back(face[2].value());
  }
  return true;
}
}
