#include "Kizuri/SceneSerializer.h"
#include "Kizuri/ComponentRegistry.h"
#include "Kizuri/Reflection.h"
#include "Kizuri/Log.h"
#include <cstdio>
#include <cstring>
#include <unordered_map>
namespace Kizuri {
namespace {
std::string EscapeName(const std::string& name) {
  std::string out;
  for (size_t i = 0; i < name.size(); ++i) {
    char c = name[i];
    if (c == '\\' || c == '"') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}
bool UnescapeName(const std::string& in, std::string& out) {
  out.clear();
  for (size_t i = 0; i < in.size(); ++i) {
    char c = in[i];
    if (c == '\\') {
      ++i;
      if (i >= in.size()) {
        return false;
      }
      out.push_back(in[i]);
    } else {
      out.push_back(c);
    }
  }
  return true;
}
bool ParseQuoted(const std::string& line, size_t& pos, std::string& out) {
  while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
    ++pos;
  }
  if (pos >= line.size() || line[pos] != '"') {
    return false;
  }
  ++pos;
  std::string raw;
  while (pos < line.size() && line[pos] != '"') {
    if (line[pos] == '\\') {
      raw.push_back('\\');
      ++pos;
      if (pos >= line.size()) {
        return false;
      }
    }
    raw.push_back(line[pos]);
    ++pos;
  }
  if (pos >= line.size()) {
    return false;
  }
  ++pos;
  return UnescapeName(raw, out);
}
bool ParseFloats(const std::string& line, size_t& pos, float* out, int count) {
  for (int i = 0; i < count; ++i) {
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
      ++pos;
    }
    if (pos >= line.size()) {
      return false;
    }
    int consumed = 0;
    if (std::sscanf(line.c_str() + pos, "%g%n", &out[i], &consumed) != 1 || consumed <= 0) {
      return false;
    }
    pos += static_cast<size_t>(consumed);
  }
  return true;
}
bool ParseInt(const std::string& line, size_t& pos, long& out) {
  while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
    ++pos;
  }
  if (pos >= line.size()) {
    return false;
  }
  int consumed = 0;
  if (std::sscanf(line.c_str() + pos, "%ld%n", &out, &consumed) != 1 || consumed <= 0) {
    return false;
  }
  pos += static_cast<size_t>(consumed);
  return true;
}
}
static void WriteMesh(std::FILE* fp, const Entity* e) {
  if (!e->hasMesh) {
    return;
  }
        std::fprintf(fp, "MESH \"%s\"\n", EscapeName(e->meshGuid).c_str());
}

static void WriteLight(std::FILE* fp, const Entity* e) {
  if (!e->hasLight) {
    return;
  }
        const LightData& l = e->light;
        std::fprintf(fp, "LIGHT %d %.9g %.9g %.9g %.9g", l.type, l.color[0], l.color[1], l.color[2], l.intensity);
        if (l.type == static_cast<int>(LightType::Point) || l.type == static_cast<int>(LightType::Spot) || l.type == static_cast<int>(LightType::Rect)) {
          std::fprintf(fp, " %.9g", l.radius);
        }
        if (l.type == static_cast<int>(LightType::Spot)) {
          std::fprintf(fp, " %.9g %.9g", l.spotAngle, l.falloff);
        }
        if (l.castShadow) {
          std::fprintf(fp, " %d %.9g %d %.9g", 1, l.lightSize, l.shadowSize, l.softness);
        }
        if (l.type == static_cast<int>(LightType::Directional) && l.castShadow) {
          std::fprintf(fp, " %d %.9g", l.cascades, l.lambda);
        }
        std::fprintf(fp, " LINK %d %d AREA %.9g %.9g\n", l.linkMask, l.layer, l.areaW, l.areaH);
        if (!l.cookieGuid.empty()) {
          std::fprintf(fp, "LIGHTCOOKIE \"%s\"\n", EscapeName(l.cookieGuid).c_str());
        }
}

static void WriteMaterial(std::FILE* fp, const Entity* e) {
  if (!e->hasMaterial) {
    return;
  }
        const MaterialData& m = e->material;
        std::fprintf(fp, "MATERIAL %.9g %.9g %.9g %.9g %.9g \"%s\" \"%s\" \"%s\" \"%s\"\n", m.albedo[0], m.albedo[1], m.albedo[2], m.metallic, m.roughness, EscapeName(m.albedoTex).c_str(), EscapeName(m.normalTex).c_str(), EscapeName(m.ormTex).c_str(), EscapeName(m.heightTex).c_str());
        std::fprintf(fp, "MATERIALX %.9g %.9g %.9g %.9g %.9g %d %d %d %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"\n", m.alpha, m.emissive[0], m.emissive[1], m.emissive[2], m.emissiveStrength, m.unlit ? 1 : 0, m.doubleSided ? 1 : 0, m.alphaMode, m.alphaCutoff, m.ior, m.hasSpecular ? 1 : 0, m.specularColor[0], m.specularColor[1], m.specularColor[2], m.specularStrength, m.clearcoat, m.clearcoatRoughness, m.sheenColor[0], m.sheenColor[1], m.sheenColor[2], m.sheenRoughness, m.anisotropy, m.anisotropyRotation, m.iridescence, m.iridescenceIor, m.iridescenceThicknessMin, m.iridescenceThicknessMax, m.transmission, m.thickness, m.attenuationColor[0], m.attenuationColor[1], m.attenuationColor[2], m.attenuationDistance, m.dispersion, EscapeName(m.emissiveTex).c_str(), EscapeName(m.specularTex).c_str(), EscapeName(m.specularColorTex).c_str(), EscapeName(m.clearcoatTex).c_str(), EscapeName(m.clearcoatRoughnessTex).c_str(), EscapeName(m.clearcoatNormalTex).c_str(), EscapeName(m.sheenColorTex).c_str(), EscapeName(m.sheenRoughnessTex).c_str(), EscapeName(m.anisotropyTex).c_str(), EscapeName(m.iridescenceTex).c_str(), EscapeName(m.iridescenceThicknessTex).c_str(), EscapeName(m.transmissionTex).c_str(), EscapeName(m.thicknessTex).c_str());
        std::fprintf(fp, "MATERIALXF %.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %d\n", m.xforms[0].offsetU, m.xforms[0].offsetV, m.xforms[0].scaleU, m.xforms[0].scaleV, m.xforms[0].rotation, m.xforms[0].uvSet, m.xforms[1].offsetU, m.xforms[1].offsetV, m.xforms[1].scaleU, m.xforms[1].scaleV, m.xforms[1].rotation, m.xforms[1].uvSet, m.xforms[2].offsetU, m.xforms[2].offsetV, m.xforms[2].scaleU, m.xforms[2].scaleV, m.xforms[2].rotation, m.xforms[2].uvSet, m.xforms[3].offsetU, m.xforms[3].offsetV, m.xforms[3].scaleU, m.xforms[3].scaleV, m.xforms[3].rotation, m.xforms[3].uvSet, m.xforms[4].offsetU, m.xforms[4].offsetV, m.xforms[4].scaleU, m.xforms[4].scaleV, m.xforms[4].rotation, m.xforms[4].uvSet);
        std::fprintf(fp, "MATERIALLINK %d\n", m.linkMask);
        std::fprintf(fp, "MATDETAIL \"%s\" \"%s\" %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %d\n", EscapeName(m.detailAlbedoTex).c_str(), EscapeName(m.detailNormalTex).c_str(), m.detailTiling, m.layerWeight, m.layerAlbedo[0], m.layerAlbedo[1], m.layerAlbedo[2], m.layerRoughness, m.layerMetallic, m.layerHeight, m.layerHeightBlend ? 1 : 0);
        std::fprintf(fp, "MATSSS %.9g %.9g %.9g %.9g\n", m.sssColor[0], m.sssColor[1], m.sssColor[2], m.sssIntensity);
        std::fprintf(fp, "MATSSSK %d\n", m.sssKind);
        std::fprintf(fp, "MATVT %d\n", m.vtAlbedo ? 1 : 0);
        std::fprintf(fp, "MATTRI %d %.9g\n", m.triplanar ? 1 : 0, m.triplanarScale);
        if (m.useNodes && !m.graph.nodes.empty()) {
          for (size_t ni = 0; ni < m.graph.nodes.size(); ++ni) {
            const MaterialNode& nd = m.graph.nodes[ni];
            std::fprintf(fp, "MATNODE %d %d %d %d %d %.9g %.9g %.9g %.9g \"%s\" %.9g %.9g\n", nd.id, nd.type, nd.inA, nd.inB, nd.inC, nd.color[0], nd.color[1], nd.color[2], nd.value, EscapeName(nd.texGuid).c_str(), nd.posX, nd.posY);
          }
          std::fprintf(fp, "MATGRAPH %d %d %d %d\n", m.graph.outAlbedo, m.graph.outRoughness, m.graph.outMetallic, m.graph.outEmissive);
        }
        if (e->variantIndex >= 0) {
          std::fprintf(fp, "VARIANT %d\n", e->variantIndex);
        }
}

static void WriteDecal(std::FILE* fp, const Entity* e) {
  if (!e->hasDecal) {
    return;
  }
        const DecalData& d = e->decal;
        std::fprintf(fp, "DECAL %.9g %.9g %.9g %.9g \"%s\"\n", d.size[0], d.size[1], d.size[2], d.life, EscapeName(d.texGuid).c_str());
        std::fprintf(fp, "DECALMAT %d %.9g %.9g\n", d.matMask, d.matRough, d.matMetal);
}

static void WriteLod(std::FILE* fp, const Entity* e) {
  if (!e->hasLod) {
    return;
  }
        const LodData& l = e->lod;
        std::fprintf(fp, "LOD %.9g %.9g \"%s\"\n", l.nearDist, l.farDist, EscapeName(l.farGuid).c_str());
}

static void WriteLightmap(std::FILE* fp, const Entity* e) {
  if (!e->hasLightmap) {
    return;
  }
        const LightmapData& l = e->lightmap;
        std::fprintf(fp, "LIGHTMAP %.9g \"%s\"\n", l.intensity, EscapeName(l.texGuid).c_str());
}

static void WriteRigidbody(std::FILE* fp, const Entity* e) {
  if (!e->hasRigidbody) {
    return;
  }
        const RigidbodyData& r = e->rigidbody;
        std::fprintf(fp, "RIGIDBODY %d %.9g %d %.9g %.9g %.9g %.9g %.9g\n", r.motionType, r.mass, r.colliderType, r.extents[0], r.extents[1], r.extents[2], r.friction, r.restitution);
}

static void WriteAudioSource(std::FILE* fp, const Entity* e) {
  if (!e->hasAudioSource) {
    return;
  }
        const AudioSourceData& a = e->audioSource;
        std::fprintf(fp, "AUDIOSOURCE \"%s\" %.9g %.9g %.9g %.9g %d %d %d %d %d\n", EscapeName(a.clipPath).c_str(), a.volume, a.minDistance, a.maxDistance, a.rolloff, a.rolloffModel, a.loop ? 1 : 0, a.spatial ? 1 : 0, a.playing ? 1 : 0, a.bus);
}

static void WriteProbe(std::FILE* fp, const Entity* e) {
  if (!e->hasProbe) {
    return;
  }
        const ProbeData& p = e->probe;
        std::fprintf(fp, "PROBE %.9g %.9g %.9g %.9g\n", p.size[0], p.size[1], p.size[2], p.intensity);
}

static void WritePost(std::FILE* fp, const Entity* e) {
  if (!e->hasPost) {
    return;
  }
        const PostData& p = e->post;
        std::fprintf(fp, "POSTVOL %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g\n", p.size[0], p.size[1], p.size[2], p.exposure, p.toneMap, p.contrast, p.saturation, p.bloom);
}

static void WriteFog(std::FILE* fp, const Entity* e) {
  if (!e->hasFog) {
    return;
  }
        const FogData& f = e->fog;
        std::fprintf(fp, "FOGVOL %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n", f.size[0], f.size[1], f.size[2], f.density, f.color[0], f.color[1], f.color[2]);
}


bool SaveSceneToFile(const Scene& scene, const std::string& path) {
  RegisterSceneComponentSerializers();
  RegisterCoreTypes();
  const StructDesc* tdesc = TypeRegistry::Instance().Find("Transform");
  if (tdesc == nullptr) {
    return false;
  }
  FILE* fp = std::fopen(path.c_str(), "wb");
  if (fp == nullptr) {
    return false;
  }
  std::fprintf(fp, "KZSCENE 1\n");
  {
    double ox = 0.0;
    double oy = 0.0;
    double oz = 0.0;
    scene.Origin().GetOrigin(ox, oy, oz);
    std::fprintf(fp, "ORIGIN %.9g %.9g %.9g\n", ox, oy, oz);
  }
  std::vector<EntityId> all = scene.All();
  std::unordered_map<uint32_t, long> indexOf;
  for (long i = 0; i < static_cast<long>(all.size()); ++i) {
    indexOf[all[i].index] = i;
  }
  for (size_t i = 0; i < all.size(); ++i) {
    const Entity* e = scene.Get(all[i]);
    if (e == nullptr) {
      continue;
    }
    long parentIdx = -1;
    if (e->parent.IsValid()) {
      auto it = indexOf.find(e->parent.index);
      if (it != indexOf.end()) {
        const Entity* p = scene.Get(e->parent);
        if (p != nullptr && p->id == e->parent) {
          parentIdx = it->second;
        }
      }
    }
    std::fprintf(fp, "ENTITY \"%s\" %ld\n", EscapeName(e->name).c_str(), parentIdx);
    const unsigned char* base = reinterpret_cast<const unsigned char*>(&e->transform);
    for (size_t f = 0; f < tdesc->fields.size(); ++f) {
      const FieldDesc& fd = tdesc->fields[f];
      if (fd.kind == FieldKind::Float3) {
        const float* v = reinterpret_cast<const float*>(base + fd.offset);
        std::fprintf(fp, "%s %.9g %.9g %.9g\n", fd.name.c_str(), v[0], v[1], v[2]);
      } else if (fd.kind == FieldKind::Float) {
        const float* v = reinterpret_cast<const float*>(base + fd.offset);
        std::fprintf(fp, "%s %.9g\n", fd.name.c_str(), v[0]);
      }
    }
    {
      const ComponentRegistry& reg = ComponentRegistry::Instance();
      for (size_t ci = 0; ci < reg.Count(); ++ci) {
        const ComponentTypeDesc* desc = reg.Get(static_cast<ComponentTypeId>(ci));
        if (desc == nullptr || desc->write == nullptr) {
          continue;
        }
        desc->write(fp, e);
      }
    }
  }
  std::fclose(fp);
  return true;
}

struct PendingEntity {
  std::string name;
  long parentIdx;    Transform transform;
  std::string meshGuid;
  bool hasMesh;
  LightData light;
  bool hasLight;
  MaterialData material;
  bool hasMaterial;
  int variantIndex;
  std::vector<PartMaterial> partMaterials;
  DecalData decal;
  bool hasDecal;
  LodData lod;
  bool hasLod;
  LightmapData lightmap;
  bool hasLightmap;
  RigidbodyData rigidbody;
  bool hasRigidbody;
  AudioSourceData audioSource;
  bool hasAudioSource;
  ProbeData probe;
  bool hasProbe;
  PostData post;
  bool hasPost;
  FogData fog;
  bool hasFog;
  bool isStatic;
};

static bool ReadMesh(const std::string& field, const std::string& rest, void* entity) {
  PendingEntity* e = static_cast<PendingEntity*>(entity);
  if (field == "MESH") {
          size_t pos = 0;
          std::string guid;
          if (!ParseQuoted(rest, pos, guid)) {
            return false;
          }
          e->meshGuid = guid;
          e->hasMesh = true;
          continue;
  }
  return false;
}

static bool ReadLight(const std::string& field, const std::string& rest, void* entity) {
  PendingEntity* e = static_cast<PendingEntity*>(entity);
  if (field == "LIGHT") {
          size_t pos = 0;
          float b[5] = { 0.0f, 1.0f, 1.0f, 1.0f, 3.0f };
          if (!ParseFloats(rest, pos, b, 5)) {
            return false;
          }
          MakeDefaultLight(e->light);
          e->light.type = static_cast<int>(b[0]);
          if (e->light.type < 0 || e->light.type > 3) {
            e->light.type = 0;
          }
          e->light.color[0] = b[1];
          e->light.color[1] = b[2];
          e->light.color[2] = b[3];
          e->light.intensity = b[4];
          if (e->light.type == static_cast<int>(LightType::Point) || e->light.type == static_cast<int>(LightType::Spot) || e->light.type == static_cast<int>(LightType::Rect)) {
            float r[1] = { 10.0f };
            if (!ParseFloats(rest, pos, r, 1)) {
            return false;
            }
            e->light.radius = r[0];
          }
          if (e->light.type == static_cast<int>(LightType::Spot)) {
            float sf[2] = { 45.0f, 0.1f };
            if (!ParseFloats(rest, pos, sf, 2)) {
            return false;
            }
            e->light.spotAngle = sf[0];
            e->light.falloff = sf[1];
          }
          float sg[4] = { 0.0f, 0.3f, 1024.0f, 0.3f };
          if (ParseFloats(rest, pos, sg, 4)) {
            e->light.castShadow = sg[0] > 0.5f;
            e->light.lightSize = sg[1];
            e->light.shadowSize = static_cast<int>(sg[2]);
            if (e->light.shadowSize != 512 && e->light.shadowSize != 1024 && e->light.shadowSize != 2048) {
              e->light.shadowSize = 1024;
            }
            e->light.softness = sg[3];
            if (e->light.softness < 0.0f) {
              e->light.softness = 0.0f;
            }
            if (e->light.softness > 2.0f) {
              e->light.softness = 2.0f;
            }
          }
          if (e->light.type == static_cast<int>(LightType::Directional)) {
            float cl[2] = { 3.0f, 0.5f };
            if (ParseFloats(rest, pos, cl, 2)) {
              e->light.cascades = static_cast<int>(cl[0]);
              if (e->light.cascades < 2) {
                e->light.cascades = 2;
              }
              if (e->light.cascades > 4) {
                e->light.cascades = 4;
              }
              e->light.lambda = cl[1];
            }
          }
          {
            const char* tail = rest.c_str() + (pos <= rest.size() ? pos : rest.size());
            const char* lk = std::strstr(tail, "LINK");
            if (lk != nullptr) {
              int lm = 255;
              int ly = 1;
              if (std::sscanf(lk, "LINK %d %d", &lm, &ly) == 2) {
                e->light.linkMask = lm;
                e->light.layer = ly;
              }
            }
            const char* ar = std::strstr(tail, "AREA");
            if (ar != nullptr) {
              float aw = 2.0f;
              float ah = 1.0f;
              if (std::sscanf(ar, "AREA %f %f", &aw, &ah) == 2) {
                e->light.areaW = aw;
                e->light.areaH = ah;
              }
            }
          }
          e->hasLight = true;
          continue;
  }

  if (field == "LIGHTCOOKIE") {
          size_t pos = 0;
          std::string guid;
          if (!ParseQuoted(rest, pos, guid)) {
            return false;
          }
          e->light.cookieGuid = guid;
          continue;
  }
  return false;
}

static bool ReadMaterial(const std::string& field, const std::string& rest, void* entity) {
  PendingEntity* e = static_cast<PendingEntity*>(entity);
  if (field == "MATERIAL") {
          size_t pos = 0;
          float mv[5] = { 1.0f, 1.0f, 1.0f, 0.0f, 0.5f };
          if (!ParseFloats(rest, pos, mv, 5)) {
            return false;
          }
          MakeDefaultMaterial(e->material);
          e->material.albedo[0] = mv[0];
          e->material.albedo[1] = mv[1];
          e->material.albedo[2] = mv[2];
          e->material.metallic = mv[3];
          e->material.roughness = mv[4];
          std::string slot;
          if (!ParseQuoted(rest, pos, slot)) {
            return false;
          }
          e->material.albedoTex = slot;
          if (!ParseQuoted(rest, pos, slot)) {
            return false;
          }
          e->material.normalTex = slot;
          if (!ParseQuoted(rest, pos, slot)) {
            return false;
          }
          e->material.ormTex = slot;
          std::string height;
          if (ParseQuoted(rest, pos, height)) {
            e->material.heightTex = height;
          }
          e->hasMaterial = true;
          continue;
  }

  if (field == "MATERIALX") {
          size_t pos = 0;
          float mv[34] = { 1.0f };
          for (int mi = 1; mi < 34; ++mi) {
            mv[mi] = 0.0f;
          }
          if (!ParseFloats(rest, pos, mv, 34)) {
            return false;
          }
          e->material.alpha = mv[0];
          e->material.emissive[0] = mv[1];
          e->material.emissive[1] = mv[2];
          e->material.emissive[2] = mv[3];
          e->material.emissiveStrength = mv[4];
          e->material.unlit = mv[5] > 0.5f;
          e->material.doubleSided = mv[6] > 0.5f;
          e->material.alphaMode = static_cast<int>(mv[7]);
          e->material.alphaCutoff = mv[8];
          e->material.ior = mv[9];
          e->material.hasSpecular = mv[10] > 0.5f;
          e->material.specularColor[0] = mv[11];
          e->material.specularColor[1] = mv[12];
          e->material.specularColor[2] = mv[13];
          e->material.specularStrength = mv[14];
          e->material.clearcoat = mv[15];
          e->material.clearcoatRoughness = mv[16];
          e->material.sheenColor[0] = mv[17];
          e->material.sheenColor[1] = mv[18];
          e->material.sheenColor[2] = mv[19];
          e->material.sheenRoughness = mv[20];
          e->material.anisotropy = mv[21];
          e->material.anisotropyRotation = mv[22];
          e->material.iridescence = mv[23];
          e->material.iridescenceIor = mv[24];
          e->material.iridescenceThicknessMin = mv[25];
          e->material.iridescenceThicknessMax = mv[26];
          e->material.transmission = mv[27];
          e->material.thickness = mv[28];
          e->material.attenuationColor[0] = mv[29];
          e->material.attenuationColor[1] = mv[30];
          e->material.attenuationColor[2] = mv[31];
          e->material.attenuationDistance = mv[32];
          e->material.dispersion = mv[33];
          std::string slots[13];
          for (int si = 0; si < 13; ++si) {
            if (!ParseQuoted(rest, pos, slots[si])) {
            return false;
            }
          }
          e->material.emissiveTex = slots[0];
          e->material.specularTex = slots[1];
          e->material.specularColorTex = slots[2];
          e->material.clearcoatTex = slots[3];
          e->material.clearcoatRoughnessTex = slots[4];
          e->material.clearcoatNormalTex = slots[5];
          e->material.sheenColorTex = slots[6];
          e->material.sheenRoughnessTex = slots[7];
          e->material.anisotropyTex = slots[8];
          e->material.iridescenceTex = slots[9];
          e->material.iridescenceThicknessTex = slots[10];
          e->material.transmissionTex = slots[11];
          e->material.thicknessTex = slots[12];
          continue;
  }

  if (field == "MATERIALXF") {
          size_t pos = 0;
          float mv[30] = { 0.0f };
          mv[2] = 1.0f;
          mv[3] = 1.0f;
          if (!ParseFloats(rest, pos, mv, 30)) {
            return false;
          }
          for (int si = 0; si < 5; ++si) {
            e->material.xforms[si].offsetU = mv[si * 6 + 0];
            e->material.xforms[si].offsetV = mv[si * 6 + 1];
            e->material.xforms[si].scaleU = mv[si * 6 + 2];
            e->material.xforms[si].scaleV = mv[si * 6 + 3];
            e->material.xforms[si].rotation = mv[si * 6 + 4];
            e->material.xforms[si].uvSet = static_cast<int>(mv[si * 6 + 5]);
          }
          continue;
  }

  if (field == "MATERIALLINK") {
          size_t pos = 0;
          long lv = 255;
          if (ParseInt(rest, pos, lv)) {
            if (lv < 0) {
              lv = 0;
            }
            if (lv > 255) {
              lv = 255;
            }
            e->material.linkMask = static_cast<int>(lv);
          }
          continue;
  }

  if (field == "MATDETAIL") {
          size_t pos = 0;
          std::string d0;
          std::string d1;
          if (ParseQuoted(rest, pos, d0) && ParseQuoted(rest, pos, d1)) {
            e->material.detailAlbedoTex = d0;
            e->material.detailNormalTex = d1;
            float dv[9] = { 8.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.8f, 0.0f, 0.5f, 0.0f };
            if (ParseFloats(rest, pos, dv, 9)) {
              e->material.detailTiling = dv[0];
              e->material.layerWeight = dv[1];
              e->material.layerAlbedo[0] = dv[2];
              e->material.layerAlbedo[1] = dv[3];
              e->material.layerAlbedo[2] = dv[4];
              e->material.layerRoughness = dv[5];
              e->material.layerMetallic = dv[6];
              e->material.layerHeight = dv[7];
              e->material.layerHeightBlend = dv[8] > 0.5f;
            }
          }
          continue;
  }

  if (field == "MATSSS") {
          size_t pos = 0;
          float dv[4] = { 0.85f, 0.35f, 0.25f, 0.0f };
          if (ParseFloats(rest, pos, dv, 4)) {
            e->material.sssColor[0] = dv[0];
            e->material.sssColor[1] = dv[1];
            e->material.sssColor[2] = dv[2];
            e->material.sssIntensity = dv[3];
          }
          continue;
  }

  if (field == "MATSSSK") {
          size_t pos = 0;
          long vv = 0;
          if (!ParseInt(rest, pos, vv)) {
            return false;
          }
          e->material.sssKind = vv < 0 ? 0 : (vv > 3 ? 3 : static_cast<int>(vv));
          continue;
  }

  if (field == "MATVT") {
          size_t pos = 0;
          long vv = 0;
          if (!ParseInt(rest, pos, vv)) {
            return false;
          }
          e->material.vtAlbedo = vv != 0;
          continue;
  }

  if (field == "MATTRI") {
          size_t pos = 0;
          float dv[2] = { 0.0f, 0.25f };
          if (!ParseFloats(rest, pos, dv, 2)) {
            return false;
          }
          e->material.triplanar = dv[0] > 0.5f;
          e->material.triplanarScale = dv[1] < 0.001f ? 0.001f : dv[1];
          continue;
  }

  if (field == "MATNODE") {
          size_t pos = 0;
          long iv[5] = { -1, 0, -1, -1, -1 };
          for (int k = 0; k < 5; ++k) {
            if (!ParseInt(rest, pos, iv[k])) {
            return false;
            }
          }
          float dv[6] = { 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f };
          if (!ParseFloats(rest, pos, dv, 6)) {
            return false;
          }
          std::string tguid;
          if (!ParseQuoted(rest, pos, tguid)) {
            return false;
          }
          float pv[2] = { 0.0f, 0.0f };
          if (!ParseFloats(rest, pos, pv, 2)) {
            return false;
          }
          MaterialNode nd;
          nd.id = static_cast<int>(iv[0]);
          nd.type = static_cast<int>(iv[1]) < 0 ? 0 : (static_cast<int>(iv[1]) > 5 ? 5 : static_cast<int>(iv[1]));
          nd.inA = static_cast<int>(iv[2]);
          nd.inB = static_cast<int>(iv[3]);
          nd.inC = static_cast<int>(iv[4]);
          nd.color[0] = dv[0];
          nd.color[1] = dv[1];
          nd.color[2] = dv[2];
          nd.value = dv[3];
          nd.texGuid = tguid;
          nd.posX = pv[0];
          nd.posY = pv[1];
          e->material.graph.nodes.push_back(nd);
          if (nd.id >= e->material.graph.nextId) {
            e->material.graph.nextId = nd.id + 1;
          }
          e->material.useNodes = true;
          continue;
  }

  if (field == "MATGRAPH") {
          size_t pos = 0;
          long ov[4] = { -1, -1, -1, -1 };
          for (int k = 0; k < 4; ++k) {
            if (!ParseInt(rest, pos, ov[k])) {
            return false;
            }
          }
          e->material.graph.outAlbedo = static_cast<int>(ov[0]);
          e->material.graph.outRoughness = static_cast<int>(ov[1]);
          e->material.graph.outMetallic = static_cast<int>(ov[2]);
          e->material.graph.outEmissive = static_cast<int>(ov[3]);
          e->material.useNodes = true;
          continue;
  }

  if (field == "VARIANT") {
          size_t pos = 0;
          long vv = -1;
          if (!ParseInt(rest, pos, vv)) {
            return false;
          }
          e->variantIndex = static_cast<int>(vv);
          continue;
  }

  if (field == "PARTMATDTL") {
          size_t pos = 0;
          long pv = -1;
          if (!ParseInt(rest, pos, pv) || pv < 0) {
            return false;
          }
          PartMaterial* pm = nullptr;
          for (size_t pi = 0; pi < e->partMaterials.size(); ++pi) {
            if (e->partMaterials[pi].part == static_cast<uint32_t>(pv)) {
              pm = &e->partMaterials[pi];
            return false;
            }
          }
          if (pm == nullptr) {
            PartMaterial fresh;
            fresh.part = static_cast<uint32_t>(pv);
            MakeDefaultMaterial(fresh.material);
            e->partMaterials.push_back(fresh);
            pm = &e->partMaterials.back();
          }
          std::string d0;
          std::string d1;
          if (ParseQuoted(rest, pos, d0) && ParseQuoted(rest, pos, d1)) {
            pm->material.detailAlbedoTex = d0;
            pm->material.detailNormalTex = d1;
            float dv[9] = { 8.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.8f, 0.0f, 0.5f, 0.0f };
            if (ParseFloats(rest, pos, dv, 9)) {
              pm->material.detailTiling = dv[0];
              pm->material.layerWeight = dv[1];
              pm->material.layerAlbedo[0] = dv[2];
              pm->material.layerAlbedo[1] = dv[3];
              pm->material.layerAlbedo[2] = dv[4];
              pm->material.layerRoughness = dv[5];
              pm->material.layerMetallic = dv[6];
              pm->material.layerHeight = dv[7];
              pm->material.layerHeightBlend = dv[8] > 0.5f;
            }
          }
          continue;
  }

  if (field == "PARTMATSSS") {
          size_t pos = 0;
          long pv = -1;
          if (!ParseInt(rest, pos, pv) || pv < 0) {
            return false;
          }
          PartMaterial* pm = nullptr;
          for (size_t pi = 0; pi < e->partMaterials.size(); ++pi) {
            if (e->partMaterials[pi].part == static_cast<uint32_t>(pv)) {
              pm = &e->partMaterials[pi];
            return false;
            }
          }
          if (pm == nullptr) {
            PartMaterial fresh;
            fresh.part = static_cast<uint32_t>(pv);
            MakeDefaultMaterial(fresh.material);
            e->partMaterials.push_back(fresh);
            pm = &e->partMaterials.back();
          }
          float dv[4] = { 0.85f, 0.35f, 0.25f, 0.0f };
          if (ParseFloats(rest, pos, dv, 4)) {
            pm->material.sssColor[0] = dv[0];
            pm->material.sssColor[1] = dv[1];
            pm->material.sssColor[2] = dv[2];
            pm->material.sssIntensity = dv[3];
          }
          continue;
  }

  if (field == "PARTMATSSSK") {
          size_t pos = 0;
          long pv = -1;
          if (!ParseInt(rest, pos, pv) || pv < 0) {
            return false;
          }
          PartMaterial* pm = nullptr;
          for (size_t pi = 0; pi < e->partMaterials.size(); ++pi) {
            if (e->partMaterials[pi].part == static_cast<uint32_t>(pv)) {
              pm = &e->partMaterials[pi];
            return false;
            }
          }
          if (pm == nullptr) {
            PartMaterial fresh;
            fresh.part = static_cast<uint32_t>(pv);
            MakeDefaultMaterial(fresh.material);
            e->partMaterials.push_back(fresh);
            pm = &e->partMaterials.back();
          }
          long vv = 0;
          if (!ParseInt(rest, pos, vv)) {
            return false;
          }
          pm->material.sssKind = vv < 0 ? 0 : (vv > 3 ? 3 : static_cast<int>(vv));
          continue;
  }

  if (field == "PARTMATVT") {
          size_t pos = 0;
          long pv = -1;
          if (!ParseInt(rest, pos, pv) || pv < 0) {
            return false;
          }
          PartMaterial* pm = nullptr;
          for (size_t pi = 0; pi < e->partMaterials.size(); ++pi) {
            if (e->partMaterials[pi].part == static_cast<uint32_t>(pv)) {
              pm = &e->partMaterials[pi];
            return false;
            }
          }
          if (pm == nullptr) {
            PartMaterial fresh;
            fresh.part = static_cast<uint32_t>(pv);
            MakeDefaultMaterial(fresh.material);
            e->partMaterials.push_back(fresh);
            pm = &e->partMaterials.back();
          }
          long vv = 0;
          if (!ParseInt(rest, pos, vv)) {
            return false;
          }
          pm->material.vtAlbedo = vv != 0;
          continue;
  }
  return false;
}

static bool ReadDecal(const std::string& field, const std::string& rest, void* entity) {
  PendingEntity* e = static_cast<PendingEntity*>(entity);
  if (field == "DECAL") {
          size_t pos = 0;
          float dv[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
          if (!ParseFloats(rest, pos, dv, 4)) {
            return false;
          }
          MakeDefaultDecal(e->decal);
          e->decal.size[0] = dv[0];
          e->decal.size[1] = dv[1];
          e->decal.size[2] = dv[2];
          e->decal.life = dv[3];
          std::string slot;
          if (!ParseQuoted(rest, pos, slot)) {
            return false;
          }
          e->decal.texGuid = slot;
          e->hasDecal = true;
          continue;
  }

  if (field == "DECALMAT") {
          size_t pos = 0;
          float dv[3] = { 1.0f, 0.5f, 0.0f };
          if (ParseFloats(rest, pos, dv, 3)) {
            e->decal.matMask = static_cast<int>(dv[0]);
            if (e->decal.matMask < 0 || e->decal.matMask > 7) {
              e->decal.matMask = 1;
            }
            e->decal.matRough = dv[1];
            e->decal.matMetal = dv[2];
          }
          continue;
  }
  return false;
}

static bool ReadLod(const std::string& field, const std::string& rest, void* entity) {
  PendingEntity* e = static_cast<PendingEntity*>(entity);
  if (field == "LOD") {
          size_t pos = 0;
          float lv[2] = { 15.0f, 40.0f };
          if (!ParseFloats(rest, pos, lv, 2)) {
            return false;
          }
          MakeDefaultLod(e->lod);
          e->lod.nearDist = lv[0];
          e->lod.farDist = lv[1];
          std::string slot;
          if (!ParseQuoted(rest, pos, slot)) {
            return false;
          }
          e->lod.farGuid = slot;
          e->hasLod = true;
          continue;
  }
  return false;
}

static bool ReadLightmap(const std::string& field, const std::string& rest, void* entity) {
  PendingEntity* e = static_cast<PendingEntity*>(entity);
  if (field == "LIGHTMAP") {
          size_t pos = 0;
          float lv[1] = { 1.0f };
          if (!ParseFloats(rest, pos, lv, 1)) {
            return false;
          }
          MakeDefaultLightmap(e->lightmap);
          e->lightmap.intensity = lv[0];
          std::string slot;
          if (!ParseQuoted(rest, pos, slot)) {
            return false;
          }
          e->lightmap.texGuid = slot;
          e->hasLightmap = true;
          continue;
  }
  return false;
}

static bool ReadRigidbody(const std::string& field, const std::string& rest, void* entity) {
  PendingEntity* e = static_cast<PendingEntity*>(entity);
  if (field == "RIGIDBODY") {
          size_t pos = 0;
          float rv[8] = { 2.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f };
          if (!ParseFloats(rest, pos, rv, 8)) {
            return false;
          }
          MakeDefaultRigidbody(e->rigidbody);
          int mt = static_cast<int>(rv[0]);
          e->rigidbody.motionType = mt < 0 ? 0 : (mt > 2 ? 2 : mt);
          e->rigidbody.mass = rv[1] < 0.01f ? 1.0f : rv[1];
          int ct = static_cast<int>(rv[2]);
          e->rigidbody.colliderType = ct < 0 ? 0 : (ct > 3 ? 3 : ct);
          e->rigidbody.extents[0] = rv[3] < 0.01f ? 0.5f : rv[3];
          e->rigidbody.extents[1] = rv[4] < 0.01f ? 0.5f : rv[4];
          e->rigidbody.extents[2] = rv[5] < 0.01f ? 0.5f : rv[5];
          e->rigidbody.friction = rv[6] < 0.0f ? 0.0f : rv[6];
          e->rigidbody.restitution = rv[7] < 0.0f ? 0.0f : rv[7];
          e->hasRigidbody = true;
          continue;
  }
  return false;
}

static bool ReadAudioSource(const std::string& field, const std::string& rest, void* entity) {
  PendingEntity* e = static_cast<PendingEntity*>(entity);
  if (field == "AUDIOSOURCE") {
          size_t pos = 0;
          std::string clip;
          if (!ParseQuoted(rest, pos, clip)) {
            return false;
          }
          float av[9] = { 1.0f, 1.0f, 20.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f };
          if (!ParseFloats(rest, pos, av, 9)) {
            return false;
          }
          MakeDefaultAudioSource(e->audioSource);
          e->audioSource.clipPath = clip;
          e->audioSource.volume = av[0];
          e->audioSource.minDistance = av[1] < 0.01f ? 0.01f : av[1];
          e->audioSource.maxDistance = av[2] > e->audioSource.minDistance ? av[2] : e->audioSource.minDistance + 0.01f;
          e->audioSource.rolloff = av[3] < 0.01f ? 0.01f : av[3];
          const int model = static_cast<int>(av[4]);
          e->audioSource.rolloffModel = model < 0 ? 0 : (model > 2 ? 2 : model);
          e->audioSource.loop = av[5] != 0.0f;
          e->audioSource.spatial = av[6] != 0.0f;
          e->audioSource.playing = av[7] != 0.0f;
          e->audioSource.bus = static_cast<int>(av[8]) < 0 ? 0 : static_cast<int>(av[8]);
          e->hasAudioSource = true;
          continue;
  }
  return false;
}

static bool ReadProbe(const std::string& field, const std::string& rest, void* entity) {
  PendingEntity* e = static_cast<PendingEntity*>(entity);
  if (field == "PROBE") {
          size_t pos = 0;
          float pv[4] = { 5.0f, 5.0f, 5.0f, 1.0f };
          if (!ParseFloats(rest, pos, pv, 4)) {
            return false;
          }
          MakeDefaultProbe(e->probe);
          e->probe.size[0] = pv[0];
          e->probe.size[1] = pv[1];
          e->probe.size[2] = pv[2];
          e->probe.intensity = pv[3];
          e->hasProbe = true;
          continue;
  }
  return false;
}

static bool ReadPost(const std::string& field, const std::string& rest, void* entity) {
  PendingEntity* e = static_cast<PendingEntity*>(entity);
  if (field == "POSTVOL") {
          size_t pos = 0;
          float pv[8] = { 10.0f, 5.0f, 10.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.8f };
          if (!ParseFloats(rest, pos, pv, 8)) {
            return false;
          }
          MakeDefaultPost(e->post);
          e->post.size[0] = pv[0];
          e->post.size[1] = pv[1];
          e->post.size[2] = pv[2];
          e->post.exposure = pv[3];
          e->post.toneMap = static_cast<int>(pv[4]);
          if (e->post.toneMap < 0 || e->post.toneMap > 3) {
            e->post.toneMap = 0;
          }
          e->post.contrast = pv[5];
          e->post.saturation = pv[6];
          e->post.bloom = pv[7];
          e->hasPost = true;
          continue;
  }
  return false;
}

static bool ReadFog(const std::string& field, const std::string& rest, void* entity) {
  PendingEntity* e = static_cast<PendingEntity*>(entity);
  if (field == "FOGVOL") {
          size_t pos = 0;
          float fv[7] = { 12.0f, 4.0f, 12.0f, 0.02f, 0.6f, 0.65f, 0.7f };
          if (!ParseFloats(rest, pos, fv, 7)) {
            return false;
          }
          MakeDefaultFog(e->fog);
          e->fog.size[0] = fv[0];
          e->fog.size[1] = fv[1];
          e->fog.size[2] = fv[2];
          e->fog.density = fv[3];
          e->fog.color[0] = fv[4];
          e->fog.color[1] = fv[5];
          e->fog.color[2] = fv[6];
          e->hasFog = true;
          continue;
  }
  return false;
}


void RegisterSceneComponentSerializers() {
  ComponentRegistry& reg = ComponentRegistry::Instance();
  reg.SetSerializer(ComponentRegistry::kMesh, &WriteMesh, &ReadMesh);
  reg.SetSerializer(ComponentRegistry::kLight, &WriteLight, &ReadLight);
  reg.SetSerializer(ComponentRegistry::kMaterial, &WriteMaterial, &ReadMaterial);
  reg.SetSerializer(ComponentRegistry::kDecal, &WriteDecal, &ReadDecal);
  reg.SetSerializer(ComponentRegistry::kLod, &WriteLod, &ReadLod);
  reg.SetSerializer(ComponentRegistry::kLightmap, &WriteLightmap, &ReadLightmap);
  reg.SetSerializer(ComponentRegistry::kRigidbody, &WriteRigidbody, &ReadRigidbody);
  reg.SetSerializer(ComponentRegistry::kAudioSource, &WriteAudioSource, &ReadAudioSource);
  reg.SetSerializer(ComponentRegistry::kProbe, &WriteProbe, &ReadProbe);
  reg.SetSerializer(ComponentRegistry::kPost, &WritePost, &ReadPost);
  reg.SetSerializer(ComponentRegistry::kFog, &WriteFog, &ReadFog);
}

bool LoadSceneFromFile(Scene& scene, const std::string& path, LogStore* log) {
  RegisterSceneComponentSerializers();
  RegisterCoreTypes();
  const StructDesc* tdesc = TypeRegistry::Instance().Find("Transform");
  if (tdesc == nullptr) {
    return false;
  }
  FILE* fp = std::fopen(path.c_str(), "rb");
  if (fp == nullptr) {
    if (log != nullptr) {
      log->Add(LogLevel::Error, std::string("Cannot open scene file: ") + path);
    }
    return false;
  }
  char line[1024];
  if (std::fgets(line, sizeof(line), fp) == nullptr) {
    std::fclose(fp);
    return false;
  }
  if (std::strncmp(line, "KZSCENE 1", 9) != 0) {
    std::fclose(fp);
    if (log != nullptr) {
      log->Add(LogLevel::Error, "Bad scene header");
    }
    return false;
  }
  std::vector<PendingEntity> pending;
  PendingEntity* current = nullptr;
  bool failed = false;
  while (std::fgets(line, sizeof(line), fp) != nullptr && !failed) {
    std::string s(line);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
      s.pop_back();
    }
    if (s.empty()) {
      continue;
    }
    if (s.compare(0, 7, "ORIGIN ") == 0) {
      size_t pos = 7;
      float ov[3] = { 0.0f, 0.0f, 0.0f };
      if (ParseFloats(s, pos, ov, 3)) {
        scene.Origin().SetOrigin(ov[0], ov[1], ov[2]);
      }
      continue;
    }
    if (s.compare(0, 7, "ENTITY ") == 0) {
      PendingEntity pe;
      MakeIdentityTransform(pe.transform);
      MakeDefaultLight(pe.light);
      MakeDefaultMaterial(pe.material);
      MakeDefaultDecal(pe.decal);
      MakeDefaultLod(pe.lod);
      MakeDefaultLightmap(pe.lightmap);
      MakeDefaultRigidbody(pe.rigidbody);
      MakeDefaultAudioSource(pe.audioSource);
      MakeDefaultProbe(pe.probe);
      MakeDefaultPost(pe.post);
      MakeDefaultFog(pe.fog);
      pe.hasMesh = false;
      pe.hasLight = false;
      pe.hasMaterial = false;
      pe.variantIndex = -1;
      pe.hasDecal = false;
      pe.hasLod = false;
      pe.hasLightmap = false;
      pe.hasRigidbody = false;
      pe.hasAudioSource = false;
      pe.hasProbe = false;
      pe.hasPost = false;
      pe.hasFog = false;
      pe.isStatic = false;
      pe.parentIdx = -1;
      size_t pos = 7;
      if (!ParseQuoted(s, pos, pe.name) || pe.name.empty()) {
        failed = true;
        break;
      }
      if (!ParseInt(s, pos, pe.parentIdx)) {
        failed = true;
        break;
      }
      pending.push_back(pe);
      current = &pending.back();
    } else if (current != nullptr) {
      size_t sp = s.find(' ');
      std::string field = (sp == std::string::npos) ? s : s.substr(0, sp);
      {
        bool handled = false;
        const ComponentRegistry& reg = ComponentRegistry::Instance();
        for (size_t ci = 0; ci < reg.Count() && !handled; ++ci) {
          const ComponentTypeDesc* desc = reg.Get(static_cast<ComponentTypeId>(ci));
          if (desc == nullptr || desc->read == nullptr) {
            continue;
          }
          handled = desc->read(field, rest, current);
        }
        if (handled) {
          continue;
        }
      }
      const FieldDesc* fd = nullptr;
      for (size_t f = 0; f < tdesc->fields.size(); ++f) {
        if (tdesc->fields[f].name == field) {
          fd = &tdesc->fields[f];
          break;
        }
      }
      if (fd == nullptr) {
        continue;
      }
      size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
      unsigned char* base = reinterpret_cast<unsigned char*>(&current->transform);
      if (fd->kind == FieldKind::Float3) {
        float v[3] = { 0, 0, 0 };
        if (!ParseFloats(s, pos, v, 3)) {
          failed = true;
          break;
        }
        float* dst = reinterpret_cast<float*>(base + fd->offset);
        dst[0] = v[0];
        dst[1] = v[1];
        dst[2] = v[2];
      } else if (fd->kind == FieldKind::Float) {
        float v[1] = { 0 };
        if (!ParseFloats(s, pos, v, 1)) {
          failed = true;
          break;
        }
        float* dst = reinterpret_cast<float*>(base + fd->offset);
        dst[0] = v[0];
      }
    } else {
      failed = true;
      break;
    }
  }
  std::fclose(fp);
  if (failed) {
    if (log != nullptr) {
      log->Add(LogLevel::Error, "Scene file is corrupt");
    }
    return false;
  }
  scene.Clear();
  std::vector<EntityId> created;
  for (size_t i = 0; i < pending.size(); ++i) {
    EntityId id = scene.CreateEntity(pending[i].name);
    if (!id.IsValid()) {
      scene.Clear();
      return false;
    }
    scene.SetTransform(id, pending[i].transform);
    Entity* createdEntity = scene.Get(id);
    if (createdEntity != nullptr) {
      createdEntity->meshGuid = pending[i].meshGuid;
      createdEntity->hasMesh = pending[i].hasMesh;
      createdEntity->light = pending[i].light;
      createdEntity->hasLight = pending[i].hasLight;
      createdEntity->material = pending[i].material;
      createdEntity->hasMaterial = pending[i].hasMaterial;
      createdEntity->variantIndex = pending[i].variantIndex;
      createdEntity->partMaterials = pending[i].partMaterials;
      createdEntity->decal = pending[i].decal;
      createdEntity->hasDecal = pending[i].hasDecal;
      createdEntity->lod = pending[i].lod;
      createdEntity->hasLod = pending[i].hasLod;
      createdEntity->lightmap = pending[i].lightmap;
      createdEntity->hasLightmap = pending[i].hasLightmap;
      createdEntity->probe = pending[i].probe;
      createdEntity->hasProbe = pending[i].hasProbe;
      createdEntity->post = pending[i].post;
      createdEntity->hasPost = pending[i].hasPost;
      createdEntity->fog = pending[i].fog;
      createdEntity->hasFog = pending[i].hasFog;
      createdEntity->rigidbody = pending[i].rigidbody;
      createdEntity->hasRigidbody = pending[i].hasRigidbody;
      createdEntity->audioSource = pending[i].audioSource;
      createdEntity->hasAudioSource = pending[i].hasAudioSource;
      createdEntity->isStatic = pending[i].isStatic;
    }
    created.push_back(id);
  }
  for (size_t i = 0; i < pending.size(); ++i) {
    if (pending[i].parentIdx >= 0 && pending[i].parentIdx < static_cast<long>(created.size())) {
      scene.SetParent(created[i], created[static_cast<size_t>(pending[i].parentIdx)]);
    }
  }
  scene.ClearDirty();
  return true;
}
}
