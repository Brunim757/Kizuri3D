#pragma once
#include "Kizuri/ComponentRegistry.h"
#include "Kizuri/JobSystem.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace Kizuri {
using EcsComponentId = uint32_t;
using EcsComponentMask = uint64_t;
constexpr EcsComponentId kEcsTransform = 63;
constexpr uint32_t kEcsMaxComponents = 64;
inline EcsComponentMask EcsBit(EcsComponentId id) {
  return id < kEcsMaxComponents ? (EcsComponentMask(1) << id) : EcsComponentMask(0);
}
class EcsWorld;
class EcsQuery;
class EcsQueryEntity {
public:
  EcsQueryEntity(EcsWorld* world, EntityId id) : world(world), id(id) {}
  EntityId Id() const { return id; }
  template <typename T>
  T* Get(EcsComponentId type) const;
private:
  EcsWorld* world;
  EntityId id;
};
class EcsComponentColumn {
public:
  EcsComponentColumn();
  ~EcsComponentColumn();
  EcsComponentColumn(const EcsComponentColumn&) = delete;
  EcsComponentColumn& operator=(const EcsComponentColumn&) = delete;
  bool Initialize(const ComponentTypeDesc& desc);
  bool InitializeRaw(size_t dataSize, ComponentCtorFn ctor, ComponentCopyFn copy, ComponentDtorFn dtor, ComponentResetFn reset);
  bool IsValid() const { return dataSize != 0 && ctor != nullptr && copy != nullptr && dtor != nullptr; }
  size_t Count() const { return count; }
  EntityId EntityAt(size_t index) const { return index < entities.size() ? entities[index] : EntityId::Invalid(); }
  bool Has(EntityId id) const;
  void* Get(EntityId id);
  const void* Get(EntityId id) const;
  void* DataAt(size_t index);
  const void* DataAt(size_t index) const;
  bool Add(EntityId id, const void* initial);
  bool Remove(EntityId id);
  void Clear();
private:
  static constexpr uint32_t kInvalidIndex = 0xFFFFFFFFu;
  size_t SparseIndex(EntityId id) const;
  unsigned char* Slot(size_t index);
  const unsigned char* Slot(size_t index) const;
  bool Reserve(size_t needed);
  size_t dataSize = 0;
  size_t stride = 0;
  ComponentCtorFn ctor = nullptr;
  ComponentCopyFn copy = nullptr;
  ComponentDtorFn dtor = nullptr;
  ComponentResetFn reset = nullptr;
  unsigned char* data = nullptr;
  size_t capacity = 0;
  size_t count = 0;
  std::vector<EntityId> entities;
  std::vector<uint32_t> sparse;
};
class EcsWorld {
public:
  EcsWorld();
  ~EcsWorld();
  EcsWorld(const EcsWorld&) = delete;
  EcsWorld& operator=(const EcsWorld&) = delete;
  EntityId CreateEntity();
  EntityId CreateEntityWithId(EntityId id);
  bool DestroyEntity(EntityId id);
  bool Has(EntityId id) const;
  size_t Count() const { return liveCount; }
  std::vector<EntityId> Entities() const;
  bool AddComponent(EntityId id, EcsComponentId type, const void* initial = nullptr);
  bool RemoveComponent(EntityId id, EcsComponentId type);
  bool HasComponent(EntityId id, EcsComponentId type) const;
  void* GetComponent(EntityId id, EcsComponentId type);
  const void* GetComponent(EntityId id, EcsComponentId type) const;
  bool SetComponent(EntityId id, EcsComponentId type, const void* data);
  EcsComponentColumn* Column(EcsComponentId type);
  const EcsComponentColumn* Column(EcsComponentId type) const;
  EcsComponentMask Mask(EntityId id) const;
  EcsQuery Query(EcsComponentMask required);
  void Clear();
private:
  struct Record {
    EntityId id;
    bool alive = false;
  };
  bool IsLive(EntityId id) const;
  EcsComponentColumn* EnsureColumn(EcsComponentId type);
  std::vector<Record> records;
  std::vector<uint32_t> freeList;
  std::vector<std::unique_ptr<EcsComponentColumn>> columns;
  size_t liveCount = 0;
};
class EcsQuery {
public:
  EcsQuery(EcsWorld& world, EcsComponentMask required);
  template <typename Fn>
  void Each(Fn&& fn) const {
    if (world == nullptr || invalid) {
      return;
    }
    const EcsComponentColumn* primary = nullptr;
    for (size_t i = 0; i < columns.size(); ++i) {
      if (columns[i] == nullptr) {
        return;
      }
      if (primary == nullptr || columns[i]->Count() < primary->Count()) {
        primary = columns[i];
      }
    }
    if (primary == nullptr || primary->Count() == 0) {
      return;
    }
    for (size_t i = 0; i < primary->Count(); ++i) {
      const EntityId id = primary->EntityAt(i);
      bool match = true;
      for (size_t c = 0; c < columns.size(); ++c) {
        if (!columns[c]->Has(id)) {
          match = false;
          break;
        }
      }
      if (match) {
        fn(EcsQueryEntity(world, id));
      }
    }
  }
  size_t Count() const;
private:
  EcsWorld* world;
  EcsComponentMask required;
  std::vector<const EcsComponentColumn*> columns;
  bool invalid = false;
};
using EcsSystemFn = std::function<void(EcsWorld&, float)>;
class EcsSystemScheduler {
public:
  void AddSystem(const std::string& name, EcsComponentMask reads, EcsComponentMask writes, EcsSystemFn fn);
  void Clear();
  size_t Run(EcsWorld& world, JobSystem& jobs, float dt);
  size_t WaveCount() const { return lastWaveCount; }
  size_t SystemCount() const { return systems.size(); }
private:
  struct System {
    std::string name;
    EcsComponentMask reads = 0;
    EcsComponentMask writes = 0;
    EcsSystemFn fn;
  };
  static bool Conflicts(const System& a, const System& b);
  std::vector<System> systems;
  size_t lastWaveCount = 0;
};
class SceneEcsBridge {
public:
  static void Import(Scene& scene, EcsWorld& world);
  static void ExportTransforms(const EcsWorld& world, Scene& scene);
  static void ExportComponents(const EcsWorld& world, Scene& scene);
};
template <typename T>
T* EcsQueryEntity::Get(EcsComponentId type) const {
  return world != nullptr ? static_cast<T*>(world->GetComponent(id, type)) : nullptr;
}
}
