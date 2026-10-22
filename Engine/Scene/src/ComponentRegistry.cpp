#include "Kizuri/ComponentRegistry.h"
#include <cstddef>
#include <cstring>
namespace Kizuri {
namespace {
void ResetMesh(void* data) {
  static_cast<std::string*>(data)->clear();
}
void ResetLight(void* data) {
  MakeDefaultLight(*static_cast<LightData*>(data));
}
void ResetMaterial(void* data) {
  MakeDefaultMaterial(*static_cast<MaterialData*>(data));
}
void ResetDecal(void* data) {
  MakeDefaultDecal(*static_cast<DecalData*>(data));
}
void ResetLod(void* data) {
  MakeDefaultLod(*static_cast<LodData*>(data));
}
void ResetLightmap(void* data) {
  MakeDefaultLightmap(*static_cast<LightmapData*>(data));
}
void ResetProbe(void* data) {
  MakeDefaultProbe(*static_cast<ProbeData*>(data));
}
void ResetPost(void* data) {
  MakeDefaultPost(*static_cast<PostData*>(data));
}
void ResetFog(void* data) {
  MakeDefaultFog(*static_cast<FogData*>(data));
}
void ResetRigidbody(void* data) {
  MakeDefaultRigidbody(*static_cast<RigidbodyData*>(data));
}
void ResetAudioSource(void* data) {
  MakeDefaultAudioSource(*static_cast<AudioSourceData*>(data));
}
void RegisterAll() {
  ComponentRegistry& reg = ComponentRegistry::Instance();
  if (reg.Count() > 0) {
    return;
  }
  reg.Register({ ComponentRegistry::kMesh, "Mesh Renderer", offsetof(Entity, meshGuid), sizeof(Entity::meshGuid), offsetof(Entity, hasMesh), &ResetMesh, nullptr, nullptr });
  reg.Register({ ComponentRegistry::kLight, "Light", offsetof(Entity, light), sizeof(Entity::light), offsetof(Entity, hasLight), &ResetLight, nullptr, nullptr });
  reg.Register({ ComponentRegistry::kMaterial, "Material", offsetof(Entity, material), sizeof(Entity::material), offsetof(Entity, hasMaterial), &ResetMaterial, nullptr, nullptr });
  reg.Register({ ComponentRegistry::kDecal, "Decal", offsetof(Entity, decal), sizeof(Entity::decal), offsetof(Entity, hasDecal), &ResetDecal, nullptr, nullptr });
  reg.Register({ ComponentRegistry::kLod, "LOD", offsetof(Entity, lod), sizeof(Entity::lod), offsetof(Entity, hasLod), &ResetLod, nullptr, nullptr });
  reg.Register({ ComponentRegistry::kLightmap, "Lightmap", offsetof(Entity, lightmap), sizeof(Entity::lightmap), offsetof(Entity, hasLightmap), &ResetLightmap, nullptr, nullptr });
  reg.Register({ ComponentRegistry::kProbe, "Reflection Probe", offsetof(Entity, probe), sizeof(Entity::probe), offsetof(Entity, hasProbe), &ResetProbe, nullptr, nullptr });
  reg.Register({ ComponentRegistry::kPost, "Post Volume", offsetof(Entity, post), sizeof(Entity::post), offsetof(Entity, hasPost), &ResetPost, nullptr, nullptr });
  reg.Register({ ComponentRegistry::kFog, "Fog Volume", offsetof(Entity, fog), sizeof(Entity::fog), offsetof(Entity, hasFog), &ResetFog, nullptr, nullptr });
  reg.Register({ ComponentRegistry::kRigidbody, "Rigidbody", offsetof(Entity, rigidbody), sizeof(Entity::rigidbody), offsetof(Entity, hasRigidbody), &ResetRigidbody, nullptr, nullptr });
  reg.Register({ ComponentRegistry::kAudioSource, "Audio Source", offsetof(Entity, audioSource), sizeof(Entity::audioSource), offsetof(Entity, hasAudioSource), &ResetAudioSource, nullptr, nullptr });
}
}
ComponentRegistry& ComponentRegistry::Instance() {
  static ComponentRegistry instance;
  RegisterAll();
  return instance;
}
void ComponentRegistry::Register(const ComponentTypeDesc& desc) {
  ComponentTypeDesc copy = desc;
  copy.id = static_cast<ComponentTypeId>(types.size());
  types.push_back(copy);
}
void ComponentRegistry::SetSerializer(ComponentTypeId id, ComponentWriteFn write, ComponentReadFn read) {
  if (id >= types.size()) {
    return;
  }
  types[id].write = write;
  types[id].read = read;
}
const ComponentTypeDesc* ComponentRegistry::Get(ComponentTypeId id) const {
  if (id >= types.size()) {
    return nullptr;
  }
  return &types[id];
}
const ComponentTypeDesc* ComponentRegistry::FindByName(const std::string& name) const {
  for (size_t i = 0; i < types.size(); ++i) {
    if (name == types[i].name) {
      return &types[i];
    }
  }
  return nullptr;
}
ComponentTypeId FindComponentType(const char* name) {
  if (name == nullptr) {
    return 0xFFFFFFFFu;
  }
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().FindByName(name);
  return desc != nullptr ? desc->id : 0xFFFFFFFFu;
}
bool EntityHasComponent(const Entity* e, ComponentTypeId id) {
  if (e == nullptr) {
    return false;
  }
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(id);
  if (desc == nullptr) {
    return false;
  }
  return *reinterpret_cast<const bool*>(reinterpret_cast<const unsigned char*>(e) + desc->hasOffset);
}
void* EntityComponentData(Entity* e, ComponentTypeId id) {
  if (e == nullptr) {
    return nullptr;
  }
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(id);
  if (desc == nullptr) {
    return nullptr;
  }
  return reinterpret_cast<unsigned char*>(e) + desc->dataOffset;
}
void ResetEntityComponent(Entity* e, ComponentTypeId id) {
  void* data = EntityComponentData(e, id);
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(id);
  if (data != nullptr && desc != nullptr && desc->reset != nullptr) {
    desc->reset(data);
  }
}
bool AddEntityComponent(Entity* e, ComponentTypeId id) {
  if (e == nullptr) {
    return false;
  }
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(id);
  if (desc == nullptr) {
    return false;
  }
  bool* has = reinterpret_cast<bool*>(reinterpret_cast<unsigned char*>(e) + desc->hasOffset);
  if (*has) {
    return false;
  }
  ResetEntityComponent(e, id);
  *has = true;
  return true;
}
bool RemoveEntityComponent(Entity* e, ComponentTypeId id) {
  if (e == nullptr) {
    return false;
  }
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(id);
  if (desc == nullptr) {
    return false;
  }
  bool* has = reinterpret_cast<bool*>(reinterpret_cast<unsigned char*>(e) + desc->hasOffset);
  if (!*has) {
    return false;
  }
  *has = false;
  return true;
}
}
