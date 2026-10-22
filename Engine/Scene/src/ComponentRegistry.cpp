#include "Kizuri/ComponentRegistry.h"
#include <cstddef>
#include <cstring>
#include <new>
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
template <typename T>
void CtorComponent(void* data) {
  new (data) T();
}
template <typename T>
void CopyComponent(void* dst, const void* src) {
  *static_cast<T*>(dst) = *static_cast<const T*>(src);
}
template <typename T>
void DtorComponent(void* data) {
  static_cast<T*>(data)->~T();
}
template <typename T>
void RegisterComponent(ComponentRegistry& reg, ComponentTypeId id, const char* name, size_t dataOffset, size_t hasOffset, ComponentResetFn reset) {
  ComponentTypeDesc desc;
  desc.id = id;
  desc.name = name;
  desc.dataOffset = dataOffset;
  desc.dataSize = sizeof(T);
  desc.hasOffset = hasOffset;
  desc.reset = reset;
  desc.ctor = &CtorComponent<T>;
  desc.copy = &CopyComponent<T>;
  desc.dtor = &DtorComponent<T>;
  reg.Register(desc);
}
void RegisterAll(ComponentRegistry& reg) {
  if (reg.Count() > 0) {
    return;
  }
  RegisterComponent<std::string>(reg, ComponentRegistry::kMesh, "Mesh Renderer", offsetof(Entity, meshGuid), offsetof(Entity, hasMesh), &ResetMesh);
  RegisterComponent<LightData>(reg, ComponentRegistry::kLight, "Light", offsetof(Entity, light), offsetof(Entity, hasLight), &ResetLight);
  RegisterComponent<MaterialData>(reg, ComponentRegistry::kMaterial, "Material", offsetof(Entity, material), offsetof(Entity, hasMaterial), &ResetMaterial);
  RegisterComponent<DecalData>(reg, ComponentRegistry::kDecal, "Decal", offsetof(Entity, decal), offsetof(Entity, hasDecal), &ResetDecal);
  RegisterComponent<LodData>(reg, ComponentRegistry::kLod, "LOD", offsetof(Entity, lod), offsetof(Entity, hasLod), &ResetLod);
  RegisterComponent<LightmapData>(reg, ComponentRegistry::kLightmap, "Lightmap", offsetof(Entity, lightmap), offsetof(Entity, hasLightmap), &ResetLightmap);
  RegisterComponent<RigidbodyData>(reg, ComponentRegistry::kRigidbody, "Rigidbody", offsetof(Entity, rigidbody), offsetof(Entity, hasRigidbody), &ResetRigidbody);
  RegisterComponent<AudioSourceData>(reg, ComponentRegistry::kAudioSource, "Audio Source", offsetof(Entity, audioSource), offsetof(Entity, hasAudioSource), &ResetAudioSource);
  RegisterComponent<ProbeData>(reg, ComponentRegistry::kProbe, "Reflection Probe", offsetof(Entity, probe), offsetof(Entity, hasProbe), &ResetProbe);
  RegisterComponent<PostData>(reg, ComponentRegistry::kPost, "Post Volume", offsetof(Entity, post), offsetof(Entity, hasPost), &ResetPost);
  RegisterComponent<FogData>(reg, ComponentRegistry::kFog, "Fog Volume", offsetof(Entity, fog), offsetof(Entity, hasFog), &ResetFog);
}
}
ComponentRegistry& ComponentRegistry::Instance() {
  static ComponentRegistry instance;
  RegisterAll(instance);
  return instance;
}
void ComponentRegistry::Register(const ComponentTypeDesc& desc) {
  if (types.size() <= desc.id) {
    types.resize(desc.id + 1);
  }
  types[desc.id] = desc;
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
const void* EntityComponentData(const Entity* e, ComponentTypeId id) {
  if (e == nullptr) {
    return nullptr;
  }
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(id);
  if (desc == nullptr) {
    return nullptr;
  }
  return reinterpret_cast<const unsigned char*>(e) + desc->dataOffset;
}
void* EntityComponentData(Entity* e, ComponentTypeId id) {
  return const_cast<void*>(EntityComponentData(static_cast<const Entity*>(e), id));
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
