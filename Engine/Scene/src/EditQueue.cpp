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
bool MaterialEqual(const MaterialData& a, const MaterialData& b) {
  return a.albedo[0] == b.albedo[0] && a.albedo[1] == b.albedo[1] && a.albedo[2] == b.albedo[2] && a.metallic == b.metallic && a.roughness == b.roughness && a.albedoTex == b.albedoTex && a.normalTex == b.normalTex && a.ormTex == b.ormTex && a.heightTex == b.heightTex;
}
bool DecalEqual(const DecalData& a, const DecalData& b) {
  return a.size[0] == b.size[0] && a.size[1] == b.size[1] && a.size[2] == b.size[2] && a.life == b.life && a.texGuid == b.texGuid;
}
bool LodEqual(const LodData& a, const LodData& b) {
  return a.farGuid == b.farGuid && a.nearDist == b.nearDist && a.farDist == b.farDist;
}
size_t EditQueue::Pending() const {
  return edits.size() + lightEdits.size() + materialEdits.size() + decalEdits.size() + lodEdits.size();
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
  return applied;
}
void EditQueue::Clear() {
  edits.clear();
  lightEdits.clear();
  materialEdits.clear();
  decalEdits.clear();
  lodEdits.clear();
}
}
