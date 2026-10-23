#pragma once
#include "Kizuri/Scene.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
namespace Kizuri {
using ComponentTypeId = uint32_t;
using ComponentResetFn = void (*)(void* data);
using ComponentCtorFn = void (*)(void* data);
using ComponentCopyFn = void (*)(void* dst, const void* src);
using ComponentDtorFn = void (*)(void* data);
using ComponentWriteFn = void (*)(std::FILE* fp, const Entity* e);
using ComponentReadFn = bool (*)(const std::string& field, const std::string& rest, void* entity);
using ComponentEqualFn = bool (*)(const void* a, const void* b);
enum class InspectorFieldKind : uint8_t {
  Bool,
  Int,
  Float,
  Vec3,
  Color3,
  Text,
  Enum,
  Texture,
  Mesh,
  AudioClip,
  AudioBus
};
enum class InspectorConditionOp : uint8_t {
  Equals,
  NotEquals,
  MaskAny,
  GreaterEqual,
  LessEqual
};
struct InspectorCondition {
  size_t offset = 0;
  InspectorConditionOp op = InspectorConditionOp::Equals;
  int value = 0;
  size_t size = sizeof(int);
};
struct InspectorFieldDesc {
  const char* label = "";
  size_t offset = 0;
  InspectorFieldKind kind = InspectorFieldKind::Float;
  float speed = 0.01f;
  float minValue = 0.0f;
  float maxValue = 0.0f;
  bool slider = false;
  const char* const* enumLabels = nullptr;
  const int* enumValues = nullptr;
  int enumCount = 0;
  const InspectorCondition* conditions = nullptr;
  int conditionCount = 0;
};
struct ComponentTypeDesc {
  ComponentTypeId id = 0;
  const char* name = "";
  size_t dataOffset = 0;
  size_t dataSize = 0;
  size_t hasOffset = 0;
  ComponentResetFn reset = nullptr;
  ComponentCtorFn ctor = nullptr;
  ComponentCopyFn copy = nullptr;
  ComponentDtorFn dtor = nullptr;
  ComponentWriteFn write = nullptr;
  ComponentReadFn read = nullptr;
  ComponentEqualFn equal = nullptr;
  const InspectorFieldDesc* fields = nullptr;
  size_t fieldCount = 0;
};
bool MaterialEqual(const MaterialData& a, const MaterialData& b);
bool LightEqual(const LightData& a, const LightData& b);
bool DecalEqual(const DecalData& a, const DecalData& b);
bool LodEqual(const LodData& a, const LodData& b);
bool RigidbodyEqual(const RigidbodyData& a, const RigidbodyData& b);
bool AudioSourceEqual(const AudioSourceData& a, const AudioSourceData& b);
bool LightmapEqual(const LightmapData& a, const LightmapData& b);
bool ProbeEqual(const ProbeData& a, const ProbeData& b);
bool PostEqual(const PostData& a, const PostData& b);
bool FogEqual(const FogData& a, const FogData& b);
class ComponentRegistry {
public:
  static constexpr ComponentTypeId kMesh = 0;
  static constexpr ComponentTypeId kLight = 1;
  static constexpr ComponentTypeId kMaterial = 2;
  static constexpr ComponentTypeId kDecal = 3;
  static constexpr ComponentTypeId kLod = 4;
  static constexpr ComponentTypeId kLightmap = 5;
  static constexpr ComponentTypeId kRigidbody = 6;
  static constexpr ComponentTypeId kAudioSource = 7;
  static constexpr ComponentTypeId kProbe = 8;
  static constexpr ComponentTypeId kPost = 9;
  static constexpr ComponentTypeId kFog = 10;
  static ComponentRegistry& Instance();
  void Register(const ComponentTypeDesc& desc);
  void SetSerializer(ComponentTypeId id, ComponentWriteFn write, ComponentReadFn read);
  size_t Count() const { return types.size(); }
  const ComponentTypeDesc* Get(ComponentTypeId id) const;
  const ComponentTypeDesc* FindByName(const std::string& name) const;
  const std::vector<ComponentTypeDesc>& All() const { return types; }
  void Clear() { types.clear(); }
private:
  std::vector<ComponentTypeDesc> types;
};
ComponentTypeId FindComponentType(const char* name);
bool EntityHasComponent(const Entity* e, ComponentTypeId id);
const void* EntityComponentData(const Entity* e, ComponentTypeId id);
void* EntityComponentData(Entity* e, ComponentTypeId id);
bool AddEntityComponent(Entity* e, ComponentTypeId id);
bool RemoveEntityComponent(Entity* e, ComponentTypeId id);
void ResetEntityComponent(Entity* e, ComponentTypeId id);
}
