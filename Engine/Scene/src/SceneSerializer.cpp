#include "Kizuri/SceneSerializer.h"
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
bool SaveSceneToFile(const Scene& scene, const std::string& path) {
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
    if (e->hasMesh) {
      std::fprintf(fp, "MESH \"%s\"\n", EscapeName(e->meshGuid).c_str());
    }
    if (e->hasLight) {
      const LightData& l = e->light;
      std::fprintf(fp, "LIGHT %d %.9g %.9g %.9g %.9g", l.type, l.color[0], l.color[1], l.color[2], l.intensity);
      if (l.type == static_cast<int>(LightType::Point) || l.type == static_cast<int>(LightType::Spot)) {
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
      std::fprintf(fp, "\n");
    }
    if (e->hasMaterial) {
      const MaterialData& m = e->material;
      std::fprintf(fp, "MATERIAL %.9g %.9g %.9g %.9g %.9g \"%s\" \"%s\" \"%s\" \"%s\"\n", m.albedo[0], m.albedo[1], m.albedo[2], m.metallic, m.roughness, EscapeName(m.albedoTex).c_str(), EscapeName(m.normalTex).c_str(), EscapeName(m.ormTex).c_str(), EscapeName(m.heightTex).c_str());
      std::fprintf(fp, "MATERIALX %.9g %.9g %.9g %.9g %.9g %d %d %d %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"\n", m.alpha, m.emissive[0], m.emissive[1], m.emissive[2], m.emissiveStrength, m.unlit ? 1 : 0, m.doubleSided ? 1 : 0, m.alphaMode, m.alphaCutoff, m.ior, m.hasSpecular ? 1 : 0, m.specularColor[0], m.specularColor[1], m.specularColor[2], m.specularStrength, m.clearcoat, m.clearcoatRoughness, m.sheenColor[0], m.sheenColor[1], m.sheenColor[2], m.sheenRoughness, m.anisotropy, m.anisotropyRotation, m.iridescence, m.iridescenceIor, m.iridescenceThicknessMin, m.iridescenceThicknessMax, m.transmission, m.thickness, m.attenuationColor[0], m.attenuationColor[1], m.attenuationColor[2], m.attenuationDistance, m.dispersion, EscapeName(m.emissiveTex).c_str(), EscapeName(m.specularTex).c_str(), EscapeName(m.specularColorTex).c_str(), EscapeName(m.clearcoatTex).c_str(), EscapeName(m.clearcoatRoughnessTex).c_str(), EscapeName(m.clearcoatNormalTex).c_str(), EscapeName(m.sheenColorTex).c_str(), EscapeName(m.sheenRoughnessTex).c_str(), EscapeName(m.anisotropyTex).c_str(), EscapeName(m.iridescenceTex).c_str(), EscapeName(m.iridescenceThicknessTex).c_str(), EscapeName(m.transmissionTex).c_str(), EscapeName(m.thicknessTex).c_str());
      std::fprintf(fp, "MATERIALXF %.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %d\n", m.xforms[0].offsetU, m.xforms[0].offsetV, m.xforms[0].scaleU, m.xforms[0].scaleV, m.xforms[0].rotation, m.xforms[0].uvSet, m.xforms[1].offsetU, m.xforms[1].offsetV, m.xforms[1].scaleU, m.xforms[1].scaleV, m.xforms[1].rotation, m.xforms[1].uvSet, m.xforms[2].offsetU, m.xforms[2].offsetV, m.xforms[2].scaleU, m.xforms[2].scaleV, m.xforms[2].rotation, m.xforms[2].uvSet, m.xforms[3].offsetU, m.xforms[3].offsetV, m.xforms[3].scaleU, m.xforms[3].scaleV, m.xforms[3].rotation, m.xforms[3].uvSet, m.xforms[4].offsetU, m.xforms[4].offsetV, m.xforms[4].scaleU, m.xforms[4].scaleV, m.xforms[4].rotation, m.xforms[4].uvSet);
      if (e->variantIndex >= 0) {
        std::fprintf(fp, "VARIANT %d\n", e->variantIndex);
      }
    }
    if (e->hasDecal) {
      const DecalData& d = e->decal;
      std::fprintf(fp, "DECAL %.9g %.9g %.9g %.9g \"%s\"\n", d.size[0], d.size[1], d.size[2], d.life, EscapeName(d.texGuid).c_str());
    }
    if (e->hasLod) {
      const LodData& l = e->lod;
      std::fprintf(fp, "LOD %.9g %.9g \"%s\"\n", l.nearDist, l.farDist, EscapeName(l.farGuid).c_str());
    }
    if (e->hasLightmap) {
      const LightmapData& l = e->lightmap;
      std::fprintf(fp, "LIGHTMAP %.9g \"%s\"\n", l.intensity, EscapeName(l.texGuid).c_str());
    }
    if (e->hasProbe) {
      const ProbeData& p = e->probe;
      std::fprintf(fp, "PROBE %.9g %.9g %.9g %.9g\n", p.size[0], p.size[1], p.size[2], p.intensity);
    }
  }
  std::fclose(fp);
  return true;
}
bool LoadSceneFromFile(Scene& scene, const std::string& path, LogStore* log) {
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
  struct PendingEntity {
    std::string name;
    long parentIdx;
    Transform transform;
    std::string meshGuid;
    bool hasMesh;
    LightData light;
    bool hasLight;
    MaterialData material;
    bool hasMaterial;
    DecalData decal;
    bool hasDecal;
    LodData lod;
    bool hasLod;
    LightmapData lightmap;
    bool hasLightmap;
    ProbeData probe;
    bool hasProbe;
  };
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
    if (s.compare(0, 7, "ENTITY ") == 0) {
      PendingEntity pe;
      MakeIdentityTransform(pe.transform);
      MakeDefaultLight(pe.light);
      MakeDefaultMaterial(pe.material);
      MakeDefaultDecal(pe.decal);
      MakeDefaultLod(pe.lod);
      MakeDefaultLightmap(pe.lightmap);
      MakeDefaultProbe(pe.probe);
      pe.hasMesh = false;
      pe.hasLight = false;
      pe.hasMaterial = false;
      pe.hasDecal = false;
      pe.hasLod = false;
      pe.hasLightmap = false;
      pe.hasProbe = false;
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
      if (field == "MESH") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        std::string guid;
        if (!ParseQuoted(s, pos, guid)) {
          failed = true;
          break;
        }
        current->meshGuid = guid;
        current->hasMesh = true;
        continue;
      }
      if (field == "LIGHT") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        float b[5] = { 0.0f, 1.0f, 1.0f, 1.0f, 3.0f };
        if (!ParseFloats(s, pos, b, 5)) {
          failed = true;
          break;
        }
        MakeDefaultLight(current->light);
        current->light.type = static_cast<int>(b[0]);
        if (current->light.type < 0 || current->light.type > 2) {
          current->light.type = 0;
        }
        current->light.color[0] = b[1];
        current->light.color[1] = b[2];
        current->light.color[2] = b[3];
        current->light.intensity = b[4];
        if (current->light.type == static_cast<int>(LightType::Point) || current->light.type == static_cast<int>(LightType::Spot)) {
          float r[1] = { 10.0f };
          if (!ParseFloats(s, pos, r, 1)) {
            failed = true;
            break;
          }
          current->light.radius = r[0];
        }
        if (current->light.type == static_cast<int>(LightType::Spot)) {
          float sf[2] = { 45.0f, 0.1f };
          if (!ParseFloats(s, pos, sf, 2)) {
            failed = true;
            break;
          }
          current->light.spotAngle = sf[0];
          current->light.falloff = sf[1];
        }
        float sg[4] = { 0.0f, 0.3f, 1024.0f, 0.3f };
        if (ParseFloats(s, pos, sg, 4)) {
          current->light.castShadow = sg[0] > 0.5f;
          current->light.lightSize = sg[1];
          current->light.shadowSize = static_cast<int>(sg[2]);
          if (current->light.shadowSize != 512 && current->light.shadowSize != 1024 && current->light.shadowSize != 2048) {
            current->light.shadowSize = 1024;
          }
          current->light.softness = sg[3];
          if (current->light.softness < 0.0f) {
            current->light.softness = 0.0f;
          }
          if (current->light.softness > 2.0f) {
            current->light.softness = 2.0f;
          }
        }
        if (current->light.type == static_cast<int>(LightType::Directional)) {
          float cl[2] = { 3.0f, 0.5f };
          if (ParseFloats(s, pos, cl, 2)) {
            current->light.cascades = static_cast<int>(cl[0]);
            if (current->light.cascades < 2) {
              current->light.cascades = 2;
            }
            if (current->light.cascades > 4) {
              current->light.cascades = 4;
            }
            current->light.lambda = cl[1];
          }
        }
        current->hasLight = true;
        continue;
      }
      if (field == "MATERIAL") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        float mv[5] = { 1.0f, 1.0f, 1.0f, 0.0f, 0.5f };
        if (!ParseFloats(s, pos, mv, 5)) {
          failed = true;
          break;
        }
        MakeDefaultMaterial(current->material);
        current->material.albedo[0] = mv[0];
        current->material.albedo[1] = mv[1];
        current->material.albedo[2] = mv[2];
        current->material.metallic = mv[3];
        current->material.roughness = mv[4];
        std::string slot;
        if (!ParseQuoted(s, pos, slot)) {
          failed = true;
          break;
        }
        current->material.albedoTex = slot;
        if (!ParseQuoted(s, pos, slot)) {
          failed = true;
          break;
        }
        current->material.normalTex = slot;
        if (!ParseQuoted(s, pos, slot)) {
          failed = true;
          break;
        }
        current->material.ormTex = slot;
        std::string height;
        if (ParseQuoted(s, pos, height)) {
          current->material.heightTex = height;
        }
        current->hasMaterial = true;
        continue;
      }
      if (field == "MATERIALX") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        float mv[34] = { 1.0f };
        for (int mi = 1; mi < 34; ++mi) {
          mv[mi] = 0.0f;
        }
        if (!ParseFloats(s, pos, mv, 34)) {
          failed = true;
          break;
        }
        current->material.alpha = mv[0];
        current->material.emissive[0] = mv[1];
        current->material.emissive[1] = mv[2];
        current->material.emissive[2] = mv[3];
        current->material.emissiveStrength = mv[4];
        current->material.unlit = mv[5] > 0.5f;
        current->material.doubleSided = mv[6] > 0.5f;
        current->material.alphaMode = static_cast<int>(mv[7]);
        current->material.alphaCutoff = mv[8];
        current->material.ior = mv[9];
        current->material.hasSpecular = mv[10] > 0.5f;
        current->material.specularColor[0] = mv[11];
        current->material.specularColor[1] = mv[12];
        current->material.specularColor[2] = mv[13];
        current->material.specularStrength = mv[14];
        current->material.clearcoat = mv[15];
        current->material.clearcoatRoughness = mv[16];
        current->material.sheenColor[0] = mv[17];
        current->material.sheenColor[1] = mv[18];
        current->material.sheenColor[2] = mv[19];
        current->material.sheenRoughness = mv[20];
        current->material.anisotropy = mv[21];
        current->material.anisotropyRotation = mv[22];
        current->material.iridescence = mv[23];
        current->material.iridescenceIor = mv[24];
        current->material.iridescenceThicknessMin = mv[25];
        current->material.iridescenceThicknessMax = mv[26];
        current->material.transmission = mv[27];
        current->material.thickness = mv[28];
        current->material.attenuationColor[0] = mv[29];
        current->material.attenuationColor[1] = mv[30];
        current->material.attenuationColor[2] = mv[31];
        current->material.attenuationDistance = mv[32];
        current->material.dispersion = mv[33];
        std::string slots[13];
        for (int si = 0; si < 13; ++si) {
          if (!ParseQuoted(s, pos, slots[si])) {
            failed = true;
            break;
          }
        }
        if (failed) {
          break;
        }
        current->material.emissiveTex = slots[0];
        current->material.specularTex = slots[1];
        current->material.specularColorTex = slots[2];
        current->material.clearcoatTex = slots[3];
        current->material.clearcoatRoughnessTex = slots[4];
        current->material.clearcoatNormalTex = slots[5];
        current->material.sheenColorTex = slots[6];
        current->material.sheenRoughnessTex = slots[7];
        current->material.anisotropyTex = slots[8];
        current->material.iridescenceTex = slots[9];
        current->material.iridescenceThicknessTex = slots[10];
        current->material.transmissionTex = slots[11];
        current->material.thicknessTex = slots[12];
        continue;
      }
      if (field == "MATERIALXF") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        float mv[30] = { 0.0f };
        mv[2] = 1.0f;
        mv[3] = 1.0f;
        if (!ParseFloats(s, pos, mv, 30)) {
          failed = true;
          break;
        }
        for (int si = 0; si < 5; ++si) {
          current->material.xforms[si].offsetU = mv[si * 6 + 0];
          current->material.xforms[si].offsetV = mv[si * 6 + 1];
          current->material.xforms[si].scaleU = mv[si * 6 + 2];
          current->material.xforms[si].scaleV = mv[si * 6 + 3];
          current->material.xforms[si].rotation = mv[si * 6 + 4];
          current->material.xforms[si].uvSet = static_cast<int>(mv[si * 6 + 5]);
        }
        continue;
      }
      if (field == "VARIANT") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        long vv = -1;
        if (!ParseInt(s, pos, vv)) {
          failed = true;
          break;
        }
        current->variantIndex = static_cast<int>(vv);
        continue;
      }
      if (field == "DECAL") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        float dv[4] = { 1.0f, 1.0f, 1.0f, 0.0f };
        if (!ParseFloats(s, pos, dv, 4)) {
          failed = true;
          break;
        }
        MakeDefaultDecal(current->decal);
        current->decal.size[0] = dv[0];
        current->decal.size[1] = dv[1];
        current->decal.size[2] = dv[2];
        current->decal.life = dv[3];
        std::string slot;
        if (!ParseQuoted(s, pos, slot)) {
          failed = true;
          break;
        }
        current->decal.texGuid = slot;
        current->hasDecal = true;
        continue;
      }
      if (field == "LOD") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        float lv[2] = { 15.0f, 40.0f };
        if (!ParseFloats(s, pos, lv, 2)) {
          failed = true;
          break;
        }
        MakeDefaultLod(current->lod);
        current->lod.nearDist = lv[0];
        current->lod.farDist = lv[1];
        std::string slot;
        if (!ParseQuoted(s, pos, slot)) {
          failed = true;
          break;
        }
        current->lod.farGuid = slot;
        current->hasLod = true;
        continue;
      }
      if (field == "LIGHTMAP") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        float lv[1] = { 1.0f };
        if (!ParseFloats(s, pos, lv, 1)) {
          failed = true;
          break;
        }
        MakeDefaultLightmap(current->lightmap);
        current->lightmap.intensity = lv[0];
        std::string slot;
        if (!ParseQuoted(s, pos, slot)) {
          failed = true;
          break;
        }
        current->lightmap.texGuid = slot;
        current->hasLightmap = true;
        continue;
      }
      if (field == "PROBE") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        float pv[4] = { 5.0f, 5.0f, 5.0f, 1.0f };
        if (!ParseFloats(s, pos, pv, 4)) {
          failed = true;
          break;
        }
        MakeDefaultProbe(current->probe);
        current->probe.size[0] = pv[0];
        current->probe.size[1] = pv[1];
        current->probe.size[2] = pv[2];
        current->probe.intensity = pv[3];
        current->hasProbe = true;
        continue;
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
      createdEntity->decal = pending[i].decal;
      createdEntity->hasDecal = pending[i].hasDecal;
      createdEntity->lod = pending[i].lod;
      createdEntity->hasLod = pending[i].hasLod;
      createdEntity->lightmap = pending[i].lightmap;
      createdEntity->hasLightmap = pending[i].hasLightmap;
      createdEntity->probe = pending[i].probe;
      createdEntity->hasProbe = pending[i].hasProbe;
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
