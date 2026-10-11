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
    if (e->hasMesh) {
      std::fprintf(fp, "MESH \"%s\"\n", EscapeName(e->meshGuid).c_str());
    }
    if (e->isStatic) {
      std::fprintf(fp, "STATIC 1\n");
    }
    if (e->hasLight) {
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
    if (e->hasMaterial) {
      const MaterialData& m = e->material;
      std::fprintf(fp, "MATERIAL %.9g %.9g %.9g %.9g %.9g \"%s\" \"%s\" \"%s\" \"%s\"\n", m.albedo[0], m.albedo[1], m.albedo[2], m.metallic, m.roughness, EscapeName(m.albedoTex).c_str(), EscapeName(m.normalTex).c_str(), EscapeName(m.ormTex).c_str(), EscapeName(m.heightTex).c_str());
      std::fprintf(fp, "MATERIALX %.9g %.9g %.9g %.9g %.9g %d %d %d %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"\n", m.alpha, m.emissive[0], m.emissive[1], m.emissive[2], m.emissiveStrength, m.unlit ? 1 : 0, m.doubleSided ? 1 : 0, m.alphaMode, m.alphaCutoff, m.ior, m.hasSpecular ? 1 : 0, m.specularColor[0], m.specularColor[1], m.specularColor[2], m.specularStrength, m.clearcoat, m.clearcoatRoughness, m.sheenColor[0], m.sheenColor[1], m.sheenColor[2], m.sheenRoughness, m.anisotropy, m.anisotropyRotation, m.iridescence, m.iridescenceIor, m.iridescenceThicknessMin, m.iridescenceThicknessMax, m.transmission, m.thickness, m.attenuationColor[0], m.attenuationColor[1], m.attenuationColor[2], m.attenuationDistance, m.dispersion, EscapeName(m.emissiveTex).c_str(), EscapeName(m.specularTex).c_str(), EscapeName(m.specularColorTex).c_str(), EscapeName(m.clearcoatTex).c_str(), EscapeName(m.clearcoatRoughnessTex).c_str(), EscapeName(m.clearcoatNormalTex).c_str(), EscapeName(m.sheenColorTex).c_str(), EscapeName(m.sheenRoughnessTex).c_str(), EscapeName(m.anisotropyTex).c_str(), EscapeName(m.iridescenceTex).c_str(), EscapeName(m.iridescenceThicknessTex).c_str(), EscapeName(m.transmissionTex).c_str(), EscapeName(m.thicknessTex).c_str());
      std::fprintf(fp, "MATERIALXF %.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %d\n", m.xforms[0].offsetU, m.xforms[0].offsetV, m.xforms[0].scaleU, m.xforms[0].scaleV, m.xforms[0].rotation, m.xforms[0].uvSet, m.xforms[1].offsetU, m.xforms[1].offsetV, m.xforms[1].scaleU, m.xforms[1].scaleV, m.xforms[1].rotation, m.xforms[1].uvSet, m.xforms[2].offsetU, m.xforms[2].offsetV, m.xforms[2].scaleU, m.xforms[2].scaleV, m.xforms[2].rotation, m.xforms[2].uvSet, m.xforms[3].offsetU, m.xforms[3].offsetV, m.xforms[3].scaleU, m.xforms[3].scaleV, m.xforms[3].rotation, m.xforms[3].uvSet, m.xforms[4].offsetU, m.xforms[4].offsetV, m.xforms[4].scaleU, m.xforms[4].scaleV, m.xforms[4].rotation, m.xforms[4].uvSet);
      std::fprintf(fp, "MATERIALLINK %d\n", m.linkMask);
      std::fprintf(fp, "MATDETAIL \"%s\" \"%s\" %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %d\n", EscapeName(m.detailAlbedoTex).c_str(), EscapeName(m.detailNormalTex).c_str(), m.detailTiling, m.layerWeight, m.layerAlbedo[0], m.layerAlbedo[1], m.layerAlbedo[2], m.layerRoughness, m.layerMetallic, m.layerHeight, m.layerHeightBlend ? 1 : 0);
      std::fprintf(fp, "MATSSS %.9g %.9g %.9g %.9g\n", m.sssColor[0], m.sssColor[1], m.sssColor[2], m.sssIntensity);
      std::fprintf(fp, "MATSSSK %d\n", m.sssKind);
      std::fprintf(fp, "MATVT %d\n", m.vtAlbedo ? 1 : 0);
      if (e->variantIndex >= 0) {
        std::fprintf(fp, "VARIANT %d\n", e->variantIndex);
      }
    }
    for (size_t pi = 0; pi < e->partMaterials.size(); ++pi) {
      const PartMaterial& pm = e->partMaterials[pi];
      std::fprintf(fp, "PARTMAT %u %.9g %.9g %.9g %.9g %.9g \"%s\" \"%s\" \"%s\" \"%s\" %.9g %.9g %.9g %.9g %.9g %d %d %d %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"\n", pm.part, pm.material.albedo[0], pm.material.albedo[1], pm.material.albedo[2], pm.material.metallic, pm.material.roughness, EscapeName(pm.material.albedoTex).c_str(), EscapeName(pm.material.normalTex).c_str(), EscapeName(pm.material.ormTex).c_str(), EscapeName(pm.material.heightTex).c_str(), pm.material.alpha, pm.material.emissive[0], pm.material.emissive[1], pm.material.emissive[2], pm.material.emissiveStrength, pm.material.unlit ? 1 : 0, pm.material.doubleSided ? 1 : 0, pm.material.alphaMode, pm.material.alphaCutoff, pm.material.ior, pm.material.hasSpecular ? 1 : 0, pm.material.specularColor[0], pm.material.specularColor[1], pm.material.specularColor[2], pm.material.specularStrength, pm.material.clearcoat, pm.material.clearcoatRoughness, pm.material.sheenColor[0], pm.material.sheenColor[1], pm.material.sheenColor[2], pm.material.sheenRoughness, pm.material.anisotropy, pm.material.anisotropyRotation, pm.material.iridescence, pm.material.iridescenceIor, pm.material.iridescenceThicknessMin, pm.material.iridescenceThicknessMax, pm.material.transmission, pm.material.thickness, pm.material.attenuationColor[0], pm.material.attenuationColor[1], pm.material.attenuationColor[2], pm.material.attenuationDistance, pm.material.dispersion, EscapeName(pm.material.emissiveTex).c_str(), EscapeName(pm.material.specularTex).c_str(), EscapeName(pm.material.specularColorTex).c_str(), EscapeName(pm.material.clearcoatTex).c_str(), EscapeName(pm.material.clearcoatRoughnessTex).c_str(), EscapeName(pm.material.clearcoatNormalTex).c_str(), EscapeName(pm.material.sheenColorTex).c_str(), EscapeName(pm.material.sheenRoughnessTex).c_str(), EscapeName(pm.material.anisotropyTex).c_str(), EscapeName(pm.material.iridescenceTex).c_str(), EscapeName(pm.material.iridescenceThicknessTex).c_str(), EscapeName(pm.material.transmissionTex).c_str(), EscapeName(pm.material.thicknessTex).c_str());
      std::fprintf(fp, "PARTMATXF %u %.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g %.9g %.9g %d\n", pm.part, pm.material.xforms[0].offsetU, pm.material.xforms[0].offsetV, pm.material.xforms[0].scaleU, pm.material.xforms[0].scaleV, pm.material.xforms[0].rotation, pm.material.xforms[0].uvSet, pm.material.xforms[1].offsetU, pm.material.xforms[1].offsetV, pm.material.xforms[1].scaleU, pm.material.xforms[1].scaleV, pm.material.xforms[1].rotation, pm.material.xforms[1].uvSet, pm.material.xforms[2].offsetU, pm.material.xforms[2].offsetV, pm.material.xforms[2].scaleU, pm.material.xforms[2].scaleV, pm.material.xforms[2].rotation, pm.material.xforms[2].uvSet, pm.material.xforms[3].offsetU, pm.material.xforms[3].offsetV, pm.material.xforms[3].scaleU, pm.material.xforms[3].scaleV, pm.material.xforms[3].rotation, pm.material.xforms[3].uvSet, pm.material.xforms[4].offsetU, pm.material.xforms[4].offsetV, pm.material.xforms[4].scaleU, pm.material.xforms[4].scaleV, pm.material.xforms[4].rotation, pm.material.xforms[4].uvSet);
      std::fprintf(fp, "PARTMATDTL %u \"%s\" \"%s\" %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %d\n", pm.part, EscapeName(pm.material.detailAlbedoTex).c_str(), EscapeName(pm.material.detailNormalTex).c_str(), pm.material.detailTiling, pm.material.layerWeight, pm.material.layerAlbedo[0], pm.material.layerAlbedo[1], pm.material.layerAlbedo[2], pm.material.layerRoughness, pm.material.layerMetallic, pm.material.layerHeight, pm.material.layerHeightBlend ? 1 : 0);
      std::fprintf(fp, "PARTMATSSS %u %.9g %.9g %.9g %.9g\n", pm.part, pm.material.sssColor[0], pm.material.sssColor[1], pm.material.sssColor[2], pm.material.sssIntensity);
      std::fprintf(fp, "PARTMATSSSK %u %d\n", pm.part, pm.material.sssKind);
      std::fprintf(fp, "PARTMATVT %u %d\n", pm.part, pm.material.vtAlbedo ? 1 : 0);
    }
    if (e->hasDecal) {
      const DecalData& d = e->decal;
      std::fprintf(fp, "DECAL %.9g %.9g %.9g %.9g \"%s\"\n", d.size[0], d.size[1], d.size[2], d.life, EscapeName(d.texGuid).c_str());
      std::fprintf(fp, "DECALMAT %d %.9g %.9g\n", d.matMask, d.matRough, d.matMetal);
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
    if (e->hasPost) {
      const PostData& p = e->post;
      std::fprintf(fp, "POSTVOL %.9g %.9g %.9g %.9g %d %.9g %.9g %.9g\n", p.size[0], p.size[1], p.size[2], p.exposure, p.toneMap, p.contrast, p.saturation, p.bloom);
    }
    if (e->hasFog) {
      const FogData& f = e->fog;
      std::fprintf(fp, "FOGVOL %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n", f.size[0], f.size[1], f.size[2], f.density, f.color[0], f.color[1], f.color[2]);
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
    ProbeData probe;
    bool hasProbe;
    PostData post;
    bool hasPost;
    FogData fog;
    bool hasFog;
    bool isStatic;
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
      if (field == "STATIC") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        long sv = 0;
        if (!ParseInt(s, pos, sv)) {
          failed = true;
          break;
        }
        current->isStatic = sv != 0;
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
        if (current->light.type < 0 || current->light.type > 3) {
          current->light.type = 0;
        }
        current->light.color[0] = b[1];
        current->light.color[1] = b[2];
        current->light.color[2] = b[3];
        current->light.intensity = b[4];
        if (current->light.type == static_cast<int>(LightType::Point) || current->light.type == static_cast<int>(LightType::Spot) || current->light.type == static_cast<int>(LightType::Rect)) {
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
        {
          const char* tail = s.c_str() + (pos <= s.size() ? pos : s.size());
          const char* lk = std::strstr(tail, "LINK");
          if (lk != nullptr) {
            int lm = 255;
            int ly = 1;
            if (std::sscanf(lk, "LINK %d %d", &lm, &ly) == 2) {
              current->light.linkMask = lm;
              current->light.layer = ly;
            }
          }
          const char* ar = std::strstr(tail, "AREA");
          if (ar != nullptr) {
            float aw = 2.0f;
            float ah = 1.0f;
            if (std::sscanf(ar, "AREA %f %f", &aw, &ah) == 2) {
              current->light.areaW = aw;
              current->light.areaH = ah;
            }
          }
        }
        current->hasLight = true;
        continue;
      }
      if (field == "LIGHTCOOKIE") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        std::string guid;
        if (!ParseQuoted(s, pos, guid)) {
          failed = true;
          break;
        }
        current->light.cookieGuid = guid;
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
      if (field == "MATERIALLINK") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        long lv = 255;
        if (ParseInt(s, pos, lv)) {
          if (lv < 0) {
            lv = 0;
          }
          if (lv > 255) {
            lv = 255;
          }
          current->material.linkMask = static_cast<int>(lv);
        }
        continue;
      }
      if (field == "MATDETAIL") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        std::string d0;
        std::string d1;
        if (ParseQuoted(s, pos, d0) && ParseQuoted(s, pos, d1)) {
          current->material.detailAlbedoTex = d0;
          current->material.detailNormalTex = d1;
          float dv[9] = { 8.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.8f, 0.0f, 0.5f, 0.0f };
          if (ParseFloats(s, pos, dv, 9)) {
            current->material.detailTiling = dv[0];
            current->material.layerWeight = dv[1];
            current->material.layerAlbedo[0] = dv[2];
            current->material.layerAlbedo[1] = dv[3];
            current->material.layerAlbedo[2] = dv[4];
            current->material.layerRoughness = dv[5];
            current->material.layerMetallic = dv[6];
            current->material.layerHeight = dv[7];
            current->material.layerHeightBlend = dv[8] > 0.5f;
          }
        }
        continue;
      }
      if (field == "MATSSS") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        float dv[4] = { 0.85f, 0.35f, 0.25f, 0.0f };
        if (ParseFloats(s, pos, dv, 4)) {
          current->material.sssColor[0] = dv[0];
          current->material.sssColor[1] = dv[1];
          current->material.sssColor[2] = dv[2];
          current->material.sssIntensity = dv[3];
        }
        continue;
      }
      if (field == "MATSSSK") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        long vv = 0;
        if (!ParseInt(s, pos, vv)) {
          failed = true;
          break;
        }
        current->material.sssKind = vv < 0 ? 0 : (vv > 3 ? 3 : static_cast<int>(vv));
        continue;
      }
      if (field == "MATVT") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        long vv = 0;
        if (!ParseInt(s, pos, vv)) {
          failed = true;
          break;
        }
        current->material.vtAlbedo = vv != 0;
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
      if (field == "PARTMAT" || field == "PARTMATXF") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        long pv = -1;
        if (!ParseInt(s, pos, pv) || pv < 0) {
          failed = true;
          break;
        }
        PartMaterial* pm = nullptr;
        for (size_t pi = 0; pi < current->partMaterials.size(); ++pi) {
          if (current->partMaterials[pi].part == static_cast<uint32_t>(pv)) {
            pm = &current->partMaterials[pi];
            break;
          }
        }
        if (pm == nullptr) {
          PartMaterial fresh;
          fresh.part = static_cast<uint32_t>(pv);
          MakeDefaultMaterial(fresh.material);
          current->partMaterials.push_back(fresh);
          pm = &current->partMaterials.back();
        }
        if (field == "PARTMAT") {
          float bv[5] = { 1.0f, 1.0f, 1.0f, 0.0f, 0.5f };
          if (!ParseFloats(s, pos, bv, 5)) {
            failed = true;
            break;
          }
          pm->material.albedo[0] = bv[0];
          pm->material.albedo[1] = bv[1];
          pm->material.albedo[2] = bv[2];
          pm->material.metallic = bv[3];
          pm->material.roughness = bv[4];
          std::string bslot;
          if (!ParseQuoted(s, pos, bslot)) {
            failed = true;
            break;
          }
          pm->material.albedoTex = bslot;
          if (!ParseQuoted(s, pos, bslot)) {
            failed = true;
            break;
          }
          pm->material.normalTex = bslot;
          if (!ParseQuoted(s, pos, bslot)) {
            failed = true;
            break;
          }
          pm->material.ormTex = bslot;
          if (!ParseQuoted(s, pos, bslot)) {
            failed = true;
            break;
          }
          pm->material.heightTex = bslot;
          float mv[34] = { 1.0f };
          for (int mi = 1; mi < 34; ++mi) {
            mv[mi] = 0.0f;
          }
          if (!ParseFloats(s, pos, mv, 34)) {
            failed = true;
            break;
          }
          pm->material.alpha = mv[0];
          pm->material.emissive[0] = mv[1];
          pm->material.emissive[1] = mv[2];
          pm->material.emissive[2] = mv[3];
          pm->material.emissiveStrength = mv[4];
          pm->material.unlit = mv[5] > 0.5f;
          pm->material.doubleSided = mv[6] > 0.5f;
          pm->material.alphaMode = static_cast<int>(mv[7]);
          pm->material.alphaCutoff = mv[8];
          pm->material.ior = mv[9];
          pm->material.hasSpecular = mv[10] > 0.5f;
          pm->material.specularColor[0] = mv[11];
          pm->material.specularColor[1] = mv[12];
          pm->material.specularColor[2] = mv[13];
          pm->material.specularStrength = mv[14];
          pm->material.clearcoat = mv[15];
          pm->material.clearcoatRoughness = mv[16];
          pm->material.sheenColor[0] = mv[17];
          pm->material.sheenColor[1] = mv[18];
          pm->material.sheenColor[2] = mv[19];
          pm->material.sheenRoughness = mv[20];
          pm->material.anisotropy = mv[21];
          pm->material.anisotropyRotation = mv[22];
          pm->material.iridescence = mv[23];
          pm->material.iridescenceIor = mv[24];
          pm->material.iridescenceThicknessMin = mv[25];
          pm->material.iridescenceThicknessMax = mv[26];
          pm->material.transmission = mv[27];
          pm->material.thickness = mv[28];
          pm->material.attenuationColor[0] = mv[29];
          pm->material.attenuationColor[1] = mv[30];
          pm->material.attenuationColor[2] = mv[31];
          pm->material.attenuationDistance = mv[32];
          pm->material.dispersion = mv[33];
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
          pm->material.emissiveTex = slots[0];
          pm->material.specularTex = slots[1];
          pm->material.specularColorTex = slots[2];
          pm->material.clearcoatTex = slots[3];
          pm->material.clearcoatRoughnessTex = slots[4];
          pm->material.clearcoatNormalTex = slots[5];
          pm->material.sheenColorTex = slots[6];
          pm->material.sheenRoughnessTex = slots[7];
          pm->material.anisotropyTex = slots[8];
          pm->material.iridescenceTex = slots[9];
          pm->material.iridescenceThicknessTex = slots[10];
          pm->material.transmissionTex = slots[11];
          pm->material.thicknessTex = slots[12];
        } else {
          float mv[30] = { 0.0f };
          if (!ParseFloats(s, pos, mv, 30)) {
            failed = true;
            break;
          }
          for (int si = 0; si < 5; ++si) {
            pm->material.xforms[si].offsetU = mv[si * 6 + 0];
            pm->material.xforms[si].offsetV = mv[si * 6 + 1];
            pm->material.xforms[si].scaleU = mv[si * 6 + 2];
            pm->material.xforms[si].scaleV = mv[si * 6 + 3];
            pm->material.xforms[si].rotation = mv[si * 6 + 4];
            pm->material.xforms[si].uvSet = static_cast<int>(mv[si * 6 + 5]);
          }
        }
        continue;
      }
      if (field == "PARTMATDTL") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        long pv = -1;
        if (!ParseInt(s, pos, pv) || pv < 0) {
          failed = true;
          break;
        }
        PartMaterial* pm = nullptr;
        for (size_t pi = 0; pi < current->partMaterials.size(); ++pi) {
          if (current->partMaterials[pi].part == static_cast<uint32_t>(pv)) {
            pm = &current->partMaterials[pi];
            break;
          }
        }
        if (pm == nullptr) {
          PartMaterial fresh;
          fresh.part = static_cast<uint32_t>(pv);
          MakeDefaultMaterial(fresh.material);
          current->partMaterials.push_back(fresh);
          pm = &current->partMaterials.back();
        }
        std::string d0;
        std::string d1;
        if (ParseQuoted(s, pos, d0) && ParseQuoted(s, pos, d1)) {
          pm->material.detailAlbedoTex = d0;
          pm->material.detailNormalTex = d1;
          float dv[9] = { 8.0f, 0.0f, 0.5f, 0.5f, 0.5f, 0.8f, 0.0f, 0.5f, 0.0f };
          if (ParseFloats(s, pos, dv, 9)) {
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
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        long pv = -1;
        if (!ParseInt(s, pos, pv) || pv < 0) {
          failed = true;
          break;
        }
        PartMaterial* pm = nullptr;
        for (size_t pi = 0; pi < current->partMaterials.size(); ++pi) {
          if (current->partMaterials[pi].part == static_cast<uint32_t>(pv)) {
            pm = &current->partMaterials[pi];
            break;
          }
        }
        if (pm == nullptr) {
          PartMaterial fresh;
          fresh.part = static_cast<uint32_t>(pv);
          MakeDefaultMaterial(fresh.material);
          current->partMaterials.push_back(fresh);
          pm = &current->partMaterials.back();
        }
        float dv[4] = { 0.85f, 0.35f, 0.25f, 0.0f };
        if (ParseFloats(s, pos, dv, 4)) {
          pm->material.sssColor[0] = dv[0];
          pm->material.sssColor[1] = dv[1];
          pm->material.sssColor[2] = dv[2];
          pm->material.sssIntensity = dv[3];
        }
        continue;
      }
      if (field == "PARTMATSSSK") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        long pv = -1;
        if (!ParseInt(s, pos, pv) || pv < 0) {
          failed = true;
          break;
        }
        PartMaterial* pm = nullptr;
        for (size_t pi = 0; pi < current->partMaterials.size(); ++pi) {
          if (current->partMaterials[pi].part == static_cast<uint32_t>(pv)) {
            pm = &current->partMaterials[pi];
            break;
          }
        }
        if (pm == nullptr) {
          PartMaterial fresh;
          fresh.part = static_cast<uint32_t>(pv);
          MakeDefaultMaterial(fresh.material);
          current->partMaterials.push_back(fresh);
          pm = &current->partMaterials.back();
        }
        long vv = 0;
        if (!ParseInt(s, pos, vv)) {
          failed = true;
          break;
        }
        pm->material.sssKind = vv < 0 ? 0 : (vv > 3 ? 3 : static_cast<int>(vv));
        continue;
      }
      if (field == "PARTMATVT") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        long pv = -1;
        if (!ParseInt(s, pos, pv) || pv < 0) {
          failed = true;
          break;
        }
        PartMaterial* pm = nullptr;
        for (size_t pi = 0; pi < current->partMaterials.size(); ++pi) {
          if (current->partMaterials[pi].part == static_cast<uint32_t>(pv)) {
            pm = &current->partMaterials[pi];
            break;
          }
        }
        if (pm == nullptr) {
          PartMaterial fresh;
          fresh.part = static_cast<uint32_t>(pv);
          MakeDefaultMaterial(fresh.material);
          current->partMaterials.push_back(fresh);
          pm = &current->partMaterials.back();
        }
        long vv = 0;
        if (!ParseInt(s, pos, vv)) {
          failed = true;
          break;
        }
        pm->material.vtAlbedo = vv != 0;
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
      if (field == "DECALMAT") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        float dv[3] = { 1.0f, 0.5f, 0.0f };
        if (ParseFloats(s, pos, dv, 3)) {
          current->decal.matMask = static_cast<int>(dv[0]);
          if (current->decal.matMask < 0 || current->decal.matMask > 7) {
            current->decal.matMask = 1;
          }
          current->decal.matRough = dv[1];
          current->decal.matMetal = dv[2];
        }
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
      if (field == "POSTVOL") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        float pv[8] = { 10.0f, 5.0f, 10.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.8f };
        if (!ParseFloats(s, pos, pv, 8)) {
          failed = true;
          break;
        }
        MakeDefaultPost(current->post);
        current->post.size[0] = pv[0];
        current->post.size[1] = pv[1];
        current->post.size[2] = pv[2];
        current->post.exposure = pv[3];
        current->post.toneMap = static_cast<int>(pv[4]);
        if (current->post.toneMap < 0 || current->post.toneMap > 3) {
          current->post.toneMap = 0;
        }
        current->post.contrast = pv[5];
        current->post.saturation = pv[6];
        current->post.bloom = pv[7];
        current->hasPost = true;
        continue;
      }
      if (field == "FOGVOL") {
        size_t pos = (sp == std::string::npos) ? s.size() : sp + 1;
        float fv[7] = { 12.0f, 4.0f, 12.0f, 0.02f, 0.6f, 0.65f, 0.7f };
        if (!ParseFloats(s, pos, fv, 7)) {
          failed = true;
          break;
        }
        MakeDefaultFog(current->fog);
        current->fog.size[0] = fv[0];
        current->fog.size[1] = fv[1];
        current->fog.size[2] = fv[2];
        current->fog.density = fv[3];
        current->fog.color[0] = fv[4];
        current->fog.color[1] = fv[5];
        current->fog.color[2] = fv[6];
        current->hasFog = true;
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
