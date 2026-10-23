#include "Kizuri/EditQueue.h"
#include "Kizuri/Undo.h"
#include <cstring>
#include <utility>
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
void EditQueue::PushRigidbody(EntityId target, const RigidbodyData& value) {
  if (!target.IsValid()) {
    return;
  }
  RigidbodyEdit e;
  e.target = target;
  e.value = value;
  rigidbodyEdits.push_back(e);
}
void EditQueue::PushAudioSource(EntityId target, const AudioSourceData& value) {
  if (!target.IsValid()) {
    return;
  }
  AudioSourceEdit e;
  e.target = target;
  e.value = value;
  audioSourceEdits.push_back(e);
}
void EditQueue::PushStatic(EntityId target, bool value) {
  if (!target.IsValid()) {
    return;
  }
  StaticEdit e;
  e.target = target;
  e.value = value;
  staticEdits.push_back(e);
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
void EditQueue::PushComponent(EntityId target, ComponentTypeId type, const void* value) {
  if (!target.IsValid() || value == nullptr) {
    return;
  }
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(type);
  if (desc == nullptr) {
    return;
  }
  ComponentEdit e;
  e.target = target;
  e.type = type;
  if (!e.value.Capture(desc, value)) {
    return;
  }
  componentEdits.push_back(std::move(e));
}
size_t EditQueue::Pending() const {
  return edits.size() + lightEdits.size() + materialEdits.size() + partMaterialEdits.size() + decalEdits.size() + lodEdits.size() + rigidbodyEdits.size() + audioSourceEdits.size() + lightmapEdits.size() + probeEdits.size() + postEdits.size() + fogEdits.size() + staticEdits.size() + componentEdits.size();
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
    if (LightEqual(e->light, lightEdits[i].value)) {
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
  for (size_t i = 0; i < rigidbodyEdits.size(); ++i) {
    bool last = true;
    for (size_t j = i + 1; j < rigidbodyEdits.size(); ++j) {
      if (rigidbodyEdits[j].target == rigidbodyEdits[i].target) {
        last = false;
        break;
      }
    }
    if (!last) {
      continue;
    }
    const Entity* e = scene.Get(rigidbodyEdits[i].target);
    if (e == nullptr || !e->hasRigidbody) {
      continue;
    }
    if (RigidbodyEqual(e->rigidbody, rigidbodyEdits[i].value)) {
      continue;
    }
    std::unique_ptr<Command> cmd(new SetRigidbodyCmd(rigidbodyEdits[i].target, e->rigidbody, rigidbodyEdits[i].value));
    if (undo.Execute(std::move(cmd), scene)) {
      ++applied;
    }
  }
  rigidbodyEdits.clear();
  for (size_t i = 0; i < audioSourceEdits.size(); ++i) {
    bool last = true;
    for (size_t j = i + 1; j < audioSourceEdits.size(); ++j) {
      if (audioSourceEdits[j].target == audioSourceEdits[i].target) {
        last = false;
        break;
      }
    }
    if (!last) {
      continue;
    }
    const Entity* e = scene.Get(audioSourceEdits[i].target);
    if (e == nullptr || !e->hasAudioSource) {
      continue;
    }
    if (AudioSourceEqual(e->audioSource, audioSourceEdits[i].value)) {
      continue;
    }
    std::unique_ptr<Command> cmd(new SetAudioSourceCmd(audioSourceEdits[i].target, e->audioSource, audioSourceEdits[i].value));
    if (undo.Execute(std::move(cmd), scene)) {
      ++applied;
    }
  }
  audioSourceEdits.clear();
  for (size_t i = 0; i < staticEdits.size(); ++i) {
    bool last = true;
    for (size_t j = i + 1; j < staticEdits.size(); ++j) {
      if (staticEdits[j].target == staticEdits[i].target) {
        last = false;
        break;
      }
    }
    if (!last) {
      continue;
    }
    const Entity* e = scene.Get(staticEdits[i].target);
    if (e == nullptr) {
      continue;
    }
    if (e->isStatic == staticEdits[i].value) {
      continue;
    }
    std::unique_ptr<Command> cmd(new SetStaticCmd(staticEdits[i].target, e->isStatic, staticEdits[i].value));
    if (undo.Execute(std::move(cmd), scene)) {
      ++applied;
    }
  }
  staticEdits.clear();
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
  for (size_t i = 0; i < componentEdits.size(); ++i) {
    bool last = true;
    for (size_t j = i + 1; j < componentEdits.size(); ++j) {
      if (componentEdits[j].target == componentEdits[i].target && componentEdits[j].type == componentEdits[i].type) {
        last = false;
        break;
      }
    }
    if (!last) {
      continue;
    }
    const Entity* e = scene.Get(componentEdits[i].target);
    if (e == nullptr || !EntityHasComponent(e, componentEdits[i].type)) {
      continue;
    }
    const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(componentEdits[i].type);
    if (desc == nullptr) {
      continue;
    }
    const void* current = EntityComponentData(e, componentEdits[i].type);
    if (current == nullptr || !componentEdits[i].value.Valid()) {
      continue;
    }
    if (desc->equal != nullptr && desc->equal(current, componentEdits[i].value.Data())) {
      continue;
    }
    std::unique_ptr<Command> cmd(new SetComponentDataCmd(componentEdits[i].target, componentEdits[i].type, std::move(componentEdits[i].value)));
    if (undo.Execute(std::move(cmd), scene)) {
      ++applied;
    }
  }
  componentEdits.clear();
  return applied;
}
void EditQueue::Clear() {
  edits.clear();
  lightEdits.clear();
  materialEdits.clear();
  partMaterialEdits.clear();
  decalEdits.clear();
  lodEdits.clear();
  rigidbodyEdits.clear();
  audioSourceEdits.clear();
  lightmapEdits.clear();
  probeEdits.clear();
  postEdits.clear();
  fogEdits.clear();
  staticEdits.clear();
  componentEdits.clear();
}
}
