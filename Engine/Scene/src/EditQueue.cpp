#include "Kizuri/EditQueue.h"
#include "Kizuri/Undo.h"
#include <cstring>
namespace Kizuri {
void EditQueue::PushTransform(EntityId target, const Transform& value) {
  if (!target.IsValid()) {
    return;
  }
  TransformEdit e;
  e.target = target;
  e.value = value;
  edits.push_back(e);
}
void EditQueue::PushLight(EntityId target, const LightData& value) {
  if (!target.IsValid()) {
    return;
  }
  LightEdit e;
  e.target = target;
  e.value = value;
  lightEdits.push_back(e);
}
void EditQueue::PushMaterial(EntityId target, const MaterialData& value) {
  if (!target.IsValid()) {
    return;
  }
  MaterialEdit e;
  e.target = target;
  e.value = value;
  materialEdits.push_back(e);
}
void EditQueue::PushPartMaterial(EntityId target, uint32_t part, const MaterialData& value) {
  if (!target.IsValid()) {
    return;
  }
  PartMaterialEdit e;
  e.target = target;
  e.part = part;
  e.value = value;
  partMaterialEdits.push_back(e);
}
void EditQueue::PushDecal(EntityId target, const DecalData& value) {
  if (!target.IsValid()) {
    return;
  }
  DecalEdit e;
  e.target = target;
  e.value = value;
  decalEdits.push_back(e);
}
void EditQueue::PushLod(EntityId target, const LodData& value) {
  if (!target.IsValid()) {
    return;
  }
  LodEdit e;
  e.target = target;
  e.value = value;
  lodEdits.push_back(e);
}
void EditQueue::PushLightmap(EntityId target, const LightmapData& value) {
  if (!target.IsValid()) {
    return;
  }
  LightmapEdit e;
  e.target = target;
  e.value = value;
  lightmapEdits.push_back(e);
}
void EditQueue::PushProbe(EntityId target, const ProbeData& value) {
  if (!target.IsValid()) {
    return;
  }
  ProbeEdit e;
  e.target = target;
  e.value = value;
  probeEdits.push_back(e);
}
void EditQueue::PushPost(EntityId target, const PostData& value) {
  if (!target.IsValid()) {
    return;
  }
  PostEdit e;
  e.target = target;
  e.value = value;
  postEdits.push_back(e);
}
void EditQueue::PushFog(EntityId target, const FogData& value) {
  if (!target.IsValid()) {
    return;
  }
  FogEdit e;
  e.target = target;
  e.value = value;
  fogEdits.push_back(e);
}
bool MaterialEqual(const MaterialData& a, const MaterialData& b) {
  if (!(a.albedo[0] == b.albedo[0] && a.albedo[1] == b.albedo[1] && a.albedo[2] == b.albedo[2] && a.alpha == b.alpha && a.metallic == b.metallic && a.roughness == b.roughness && a.albedoTex == b.albedoTex && a.normalTex == b.normalTex && a.ormTex == b.ormTex && a.heightTex == b.heightTex)) {
    return false;
  }
  if (!(a.emissive[0] == b.emissive[0] && a.emissive[1] == b.emissive[1] && a.emissive[2] == b.emissive[2] && a.emissiveStrength == b.emissiveStrength && a.emissiveTex == b.emissiveTex && a.unlit == b.unlit && a.doubleSided == b.doubleSided && a.alphaMode == b.alphaMode && a.alphaCutoff == b.alphaCutoff)) {
    return false;
  }
  for (int s = 0; s < 5; ++s) {
    if (!(a.xforms[s].offsetU == b.xforms[s].offsetU && a.xforms[s].offsetV == b.xforms[s].offsetV && a.xforms[s].scaleU == b.xforms[s].scaleU && a.xforms[s].scaleV == b.xforms[s].scaleV && a.xforms[s].rotation == b.xforms[s].rotation && a.xforms[s].uvSet == b.xforms[s].uvSet)) {
      return false;
    }
  }
  if (!(a.ior == b.ior && a.hasSpecular == b.hasSpecular && a.specularColor[0] == b.specularColor[0] && a.specularColor[1] == b.specularColor[1] && a.specularColor[2] == b.specularColor[2] && a.specularStrength == b.specularStrength && a.specularTex == b.specularTex && a.specularColorTex == b.specularColorTex)) {
    return false;
  }
  if (!(a.clearcoat == b.clearcoat && a.clearcoatRoughness == b.clearcoatRoughness && a.clearcoatTex == b.clearcoatTex && a.clearcoatRoughnessTex == b.clearcoatRoughnessTex && a.clearcoatNormalTex == b.clearcoatNormalTex)) {
    return false;
  }
  if (!(a.sheenColor[0] == b.sheenColor[0] && a.sheenColor[1] == b.sheenColor[1] && a.sheenColor[2] == b.sheenColor[2] && a.sheenRoughness == b.sheenRoughness && a.sheenColorTex == b.sheenColorTex && a.sheenRoughnessTex == b.sheenRoughnessTex)) {
    return false;
  }
  if (!(a.anisotropy == b.anisotropy && a.anisotropyRotation == b.anisotropyRotation && a.anisotropyTex == b.anisotropyTex)) {
    return false;
  }
  if (!(a.iridescence == b.iridescence && a.iridescenceIor == b.iridescenceIor && a.iridescenceThicknessMin == b.iridescenceThicknessMin && a.iridescenceThicknessMax == b.iridescenceThicknessMax && a.iridescenceTex == b.iridescenceTex && a.iridescenceThicknessTex == b.iridescenceThicknessTex)) {
    return false;
  }
  if (!(a.transmission == b.transmission && a.transmissionTex == b.transmissionTex && a.thickness == b.thickness && a.thicknessTex == b.thicknessTex && a.attenuationColor[0] == b.attenuationColor[0] && a.attenuationColor[1] == b.attenuationColor[1] && a.attenuationColor[2] == b.attenuationColor[2] && a.attenuationDistance == b.attenuationDistance && a.dispersion == b.dispersion)) {
    return false;
  }
  return true;
}
bool DecalEqual(const DecalData& a, const DecalData& b) {
  return a.size[0] == b.size[0] && a.size[1] == b.size[1] && a.size[2] == b.size[2] && a.life == b.life && a.texGuid == b.texGuid && a.matMask == b.matMask && a.matRough == b.matRough && a.matMetal == b.matMetal;
}
bool LodEqual(const LodData& a, const LodData& b) {
  return a.farGuid == b.farGuid && a.nearDist == b.nearDist && a.farDist == b.farDist;
}
bool LightmapEqual(const LightmapData& a, const LightmapData& b) {
  return a.texGuid == b.texGuid && a.intensity == b.intensity;
}
bool ProbeEqual(const ProbeData& a, const ProbeData& b) {
  return a.size[0] == b.size[0] && a.size[1] == b.size[1] && a.size[2] == b.size[2] && a.intensity == b.intensity;
}
bool PostEqual(const PostData& a, const PostData& b) {
  return a.size[0] == b.size[0] && a.size[1] == b.size[1] && a.size[2] == b.size[2] && a.exposure == b.exposure && a.toneMap == b.toneMap && a.contrast == b.contrast && a.saturation == b.saturation && a.bloom == b.bloom;
}
bool FogEqual(const FogData& a, const FogData& b) {
  return a.size[0] == b.size[0] && a.size[1] == b.size[1] && a.size[2] == b.size[2] && a.density == b.density && a.color[0] == b.color[0] && a.color[1] == b.color[1] && a.color[2] == b.color[2];
}
size_t EditQueue::Pending() const {
  return edits.size() + lightEdits.size() + materialEdits.size() + partMaterialEdits.size() + decalEdits.size() + lodEdits.size() + lightmapEdits.size() + probeEdits.size() + postEdits.size() + fogEdits.size();
}
size_t EditQueue::ApplyAll(Scene& scene, UndoStack& undo) {
  size_t applied = 0;
  for (size_t i = 0; i < edits.size(); ++i) {
    bool last = true;
    for (size_t j = i + 1; j < edits.size(); ++j) {
      if (edits[j].target == edits[i].target) {
        last = false;
        break;
      }
    }
    if (!last) {
      continue;
    }
    const Entity* e = scene.Get(edits[i].target);
    if (e == nullptr) {
      continue;
    }
    if (std::memcmp(&e->transform, &edits[i].value, sizeof(Transform)) == 0) {
      continue;
    }
    std::unique_ptr<Command> cmd(new EditTransformCmd(edits[i].target, e->transform, edits[i].value));
    if (undo.Execute(std::move(cmd), scene)) {
      ++applied;
    }
  }
  for (size_t i = 0; i < lightEdits.size(); ++i) {
    bool last = true;
    for (size_t j = i + 1; j < lightEdits.size(); ++j) {
      if (lightEdits[j].target == lightEdits[i].target) {
        last = false;
        break;
      }
    }
    if (!last) {
      continue;
    }
    const Entity* e = scene.Get(lightEdits[i].target);
    if (e == nullptr || !e->hasLight) {
      continue;
    }
    if (std::memcmp(&e->light, &lightEdits[i].value, sizeof(LightData)) == 0) {
      continue;
    }
    std::unique_ptr<Command> cmd(new SetLightCmd(lightEdits[i].target, e->light, lightEdits[i].value));
    if (undo.Execute(std::move(cmd), scene)) {
      ++applied;
    }
  }
  edits.clear();
  lightEdits.clear();
  for (size_t i = 0; i < materialEdits.size(); ++i) {
    bool last = true;
    for (size_t j = i + 1; j < materialEdits.size(); ++j) {
      if (materialEdits[j].target == materialEdits[i].target) {
        last = false;
        break;
      }
    }
    if (!last) {
      continue;
    }
    const Entity* e = scene.Get(materialEdits[i].target);
    if (e == nullptr || !e->hasMaterial) {
      continue;
    }
    if (MaterialEqual(e->material, materialEdits[i].value)) {
      continue;
    }
    std::unique_ptr<Command> cmd(new SetMaterialCmd(materialEdits[i].target, e->material, materialEdits[i].value));
    if (undo.Execute(std::move(cmd), scene)) {
      ++applied;
    }
  }
  materialEdits.clear();
  for (size_t i = 0; i < partMaterialEdits.size(); ++i) {
    bool last = true;
    for (size_t j = i + 1; j < partMaterialEdits.size(); ++j) {
      if (partMaterialEdits[j].target == partMaterialEdits[i].target && partMaterialEdits[j].part == partMaterialEdits[i].part) {
        last = false;
        break;
      }
    }
    if (!last) {
      continue;
    }
    Entity* e = scene.Get(partMaterialEdits[i].target);
    if (e == nullptr) {
      continue;
    }
    const MaterialData* cur = nullptr;
    for (size_t k = 0; k < e->partMaterials.size(); ++k) {
      if (e->partMaterials[k].part == partMaterialEdits[i].part) {
        cur = &e->partMaterials[k].material;
        break;
      }
    }
    if (cur == nullptr) {
      continue;
    }
    if (MaterialEqual(*cur, partMaterialEdits[i].value)) {
      continue;
    }
    std::unique_ptr<Command> cmd(new SetPartMaterialCmd(partMaterialEdits[i].target, partMaterialEdits[i].part, *cur, partMaterialEdits[i].value));
    if (undo.Execute(std::move(cmd), scene)) {
      ++applied;
    }
  }
  partMaterialEdits.clear();
  for (size_t i = 0; i < decalEdits.size(); ++i) {
    bool last = true;
    for (size_t j = i + 1; j < decalEdits.size(); ++j) {
      if (decalEdits[j].target == decalEdits[i].target) {
        last = false;
        break;
      }
    }
    if (!last) {
      continue;
    }
    const Entity* e = scene.Get(decalEdits[i].target);
    if (e == nullptr || !e->hasDecal) {
      continue;
    }
    if (DecalEqual(e->decal, decalEdits[i].value)) {
      continue;
    }
    std::unique_ptr<Command> cmd(new SetDecalCmd(decalEdits[i].target, e->decal, decalEdits[i].value));
    if (undo.Execute(std::move(cmd), scene)) {
      ++applied;
    }
  }
  decalEdits.clear();
  for (size_t i = 0; i < lodEdits.size(); ++i) {
    bool last = true;
    for (size_t j = i + 1; j < lodEdits.size(); ++j) {
      if (lodEdits[j].target == lodEdits[i].target) {
        last = false;
        break;
      }
    }
    if (!last) {
      continue;
    }
    const Entity* e = scene.Get(lodEdits[i].target);
    if (e == nullptr || !e->hasLod) {
      continue;
    }
    if (LodEqual(e->lod, lodEdits[i].value)) {
      continue;
    }
    std::unique_ptr<Command> cmd(new SetLodCmd(lodEdits[i].target, e->lod, lodEdits[i].value));
    if (undo.Execute(std::move(cmd), scene)) {
      ++applied;
    }
  }
  lodEdits.clear();
  for (size_t i = 0; i < lightmapEdits.size(); ++i) {
    bool last = true;
    for (size_t j = i + 1; j < lightmapEdits.size(); ++j) {
      if (lightmapEdits[j].target == lightmapEdits[i].target) {
        last = false;
        break;
      }
    }
    if (!last) {
      continue;
    }
    const Entity* e = scene.Get(lightmapEdits[i].target);
    if (e == nullptr || !e->hasLightmap) {
      continue;
    }
    if (LightmapEqual(e->lightmap, lightmapEdits[i].value)) {
      continue;
    }
    std::unique_ptr<Command> cmd(new SetLightmapCmd(lightmapEdits[i].target, e->lightmap, lightmapEdits[i].value));
    if (undo.Execute(std::move(cmd), scene)) {
      ++applied;
    }
  }
  lightmapEdits.clear();
  for (size_t i = 0; i < probeEdits.size(); ++i) {
    bool last = true;
    for (size_t j = i + 1; j < probeEdits.size(); ++j) {
      if (probeEdits[j].target == probeEdits[i].target) {
        last = false;
        break;
      }
    }
    if (!last) {
      continue;
    }
    const Entity* e = scene.Get(probeEdits[i].target);
    if (e == nullptr || !e->hasProbe) {
      continue;
    }
    if (ProbeEqual(e->probe, probeEdits[i].value)) {
      continue;
    }
    std::unique_ptr<Command> cmd(new SetProbeCmd(probeEdits[i].target, e->probe, probeEdits[i].value));
    if (undo.Execute(std::move(cmd), scene)) {
      ++applied;
    }
  }
  probeEdits.clear();
  for (size_t i = 0; i < postEdits.size(); ++i) {
    bool last = true;
    for (size_t j = i + 1; j < postEdits.size(); ++j) {
      if (postEdits[j].target == postEdits[i].target) {
        last = false;
        break;
      }
    }
    if (!last) {
      continue;
    }
    const Entity* e = scene.Get(postEdits[i].target);
    if (e == nullptr || !e->hasPost) {
      continue;
    }
    if (PostEqual(e->post, postEdits[i].value)) {
      continue;
    }
    std::unique_ptr<Command> cmd(new SetPostCmd(postEdits[i].target, e->post, postEdits[i].value));
    if (undo.Execute(std::move(cmd), scene)) {
      ++applied;
    }
  }
  postEdits.clear();
  for (size_t i = 0; i < fogEdits.size(); ++i) {
    bool last = true;
    for (size_t j = i + 1; j < fogEdits.size(); ++j) {
      if (fogEdits[j].target == fogEdits[i].target) {
        last = false;
        break;
      }
    }
    if (!last) {
      continue;
    }
    const Entity* e = scene.Get(fogEdits[i].target);
    if (e == nullptr || !e->hasFog) {
      continue;
    }
    if (FogEqual(e->fog, fogEdits[i].value)) {
      continue;
    }
    std::unique_ptr<Command> cmd(new SetFogCmd(fogEdits[i].target, e->fog, fogEdits[i].value));
    if (undo.Execute(std::move(cmd), scene)) {
      ++applied;
    }
  }
  fogEdits.clear();
  return applied;
}
void EditQueue::Clear() {
  edits.clear();
  lightEdits.clear();
  materialEdits.clear();
  partMaterialEdits.clear();
  decalEdits.clear();
  lodEdits.clear();
  lightmapEdits.clear();
  probeEdits.clear();
  postEdits.clear();
  fogEdits.clear();
}
}
