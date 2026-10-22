#pragma once
#include "Kizuri/Scene.h"
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
namespace Kizuri {
using ComponentTypeId = uint32_t;
using ComponentResetFn = void (*)(void* data);
using ComponentWriteFn = void (*)(std::FILE* fp, const Entity* e);
using ComponentReadFn = bool (*)(const std::string& field, const std::string& rest, Entity* e);
struct ComponentTypeDesc {
  ComponentTypeId id = 0;
  const char* name = "";
  size_t dataOffset = 0;
  size_t dataSize = 0;
  size_t hasOffset = 0;
  ComponentResetFn reset = nullptr;
  ComponentWriteFn write = nullptr;
  ComponentReadFn read = nullptr;
};
class ComponentRegistry {
public:
  static ComponentRegistry& Instance();
  void Register(const ComponentTypeDesc& desc);
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
void* EntityComponentData(Entity* e, ComponentTypeId id);
bool AddEntityComponent(Entity* e, ComponentTypeId id);
bool RemoveEntityComponent(Entity* e, ComponentTypeId id);
void ResetEntityComponent(Entity* e, ComponentTypeId id);
}
