#include "Kizuri/Ecs.h"
#include <algorithm>
#include <cstdlib>
#include <new>
#include <utility>
namespace Kizuri {
namespace {
constexpr size_t kEcsAlignment = 16;
size_t AlignUp(size_t value, size_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}
template <typename T>
void EcsCtor(void* data) {
  new (data) T();
}
template <typename T>
void EcsCopy(void* dst, const void* src) {
  *static_cast<T*>(dst) = *static_cast<const T*>(src);
}
template <typename T>
void EcsDtor(void* data) {
  static_cast<T*>(data)->~T();
}
void EcsResetTransform(void* data) {
  MakeIdentityTransform(*static_cast<Transform*>(data));
}
}
EcsComponentColumn::EcsComponentColumn() {
}
EcsComponentColumn::~EcsComponentColumn() {
  Clear();
}
bool EcsComponentColumn::Initialize(const ComponentTypeDesc& desc) {
  if (desc.dataSize == 0 || desc.ctor == nullptr || desc.copy == nullptr || desc.dtor == nullptr) {
    return false;
  }
  return InitializeRaw(desc.dataSize, desc.ctor, desc.copy, desc.dtor, desc.reset);
}
bool EcsComponentColumn::InitializeRaw(size_t size, ComponentCtorFn construct, ComponentCopyFn copyFn, ComponentDtorFn destruct, ComponentResetFn resetFn) {
  if (size == 0 || construct == nullptr || copyFn == nullptr || destruct == nullptr) {
    return false;
  }
  Clear();
  dataSize = size;
  stride = AlignUp(size, kEcsAlignment);
  ctor = construct;
  copy = copyFn;
  dtor = destruct;
  reset = resetFn;
  return true;
}
size_t EcsComponentColumn::SparseIndex(EntityId id) const {
  if (id.index >= sparse.size()) {
    return kInvalidIndex;
  }
  return sparse[id.index];
}
bool EcsComponentColumn::Has(EntityId id) const {
  const size_t dense = SparseIndex(id);
  return dense != kInvalidIndex && dense < count && entities[dense] == id;
}
unsigned char* EcsComponentColumn::Slot(size_t index) {
  return data + index * stride;
}
const unsigned char* EcsComponentColumn::Slot(size_t index) const {
  return data + index * stride;
}
void* EcsComponentColumn::DataAt(size_t index) {
  return index < count ? Slot(index) : nullptr;
}
const void* EcsComponentColumn::DataAt(size_t index) const {
  return index < count ? Slot(index) : nullptr;
}
void* EcsComponentColumn::Get(EntityId id) {
  return const_cast<void*>(static_cast<const EcsComponentColumn*>(this)->Get(id));
}
const void* EcsComponentColumn::Get(EntityId id) const {
  const size_t dense = SparseIndex(id);
  if (dense == kInvalidIndex || dense >= count || entities[dense] != id) {
    return nullptr;
  }
  return Slot(dense);
}
bool EcsComponentColumn::Reserve(size_t needed) {
  if (needed <= capacity) {
    return true;
  }
  size_t nextCapacity = capacity == 0 ? 16 : capacity;
  while (nextCapacity < needed) {
    nextCapacity *= 2;
  }
  if (nextCapacity > static_cast<size_t>(-1) / stride) {
    return false;
  }
  unsigned char* next = static_cast<unsigned char*>(std::malloc(nextCapacity * stride));
  if (next == nullptr) {
    return false;
  }
  size_t constructed = 0;
  try {
    for (size_t i = 0; i < count; ++i) {
      ctor(next + i * stride);
      copy(next + i * stride, data + i * stride);
      ++constructed;
    }
  } catch (...) {
    for (size_t i = 0; i < constructed; ++i) {
      dtor(next + i * stride);
    }
    std::free(next);
    throw;
  }
  for (size_t i = 0; i < count; ++i) {
    dtor(data + i * stride);
  }
  std::free(data);
  data = next;
  capacity = nextCapacity;
  return true;
}
bool EcsComponentColumn::Add(EntityId id, const void* initial) {
  if (id.index == 0xFFFFFFFFu) {
    return false;
  }
  const size_t dense = SparseIndex(id);
  if (dense != kInvalidIndex && dense < count && entities[dense] == id) {
    if (initial != nullptr) {
      copy(Slot(dense), initial);
    }
    return true;
  }
  if (!Reserve(count + 1)) {
    return false;
  }
  unsigned char* slot = Slot(count);
  ctor(slot);
  if (initial != nullptr) {
    copy(slot, initial);
  } else if (reset != nullptr) {
    reset(slot);
  }
  if (sparse.size() <= id.index) {
    sparse.resize(static_cast<size_t>(id.index) + 1, kInvalidIndex);
  }
  sparse[id.index] = static_cast<uint32_t>(count);
  entities.push_back(id);
  ++count;
  return true;
}
bool EcsComponentColumn::Remove(EntityId id) {
  const size_t dense = SparseIndex(id);
  if (dense == kInvalidIndex || dense >= count || entities[dense] != id) {
    return false;
  }
  dtor(Slot(dense));
  const size_t last = count - 1;
  if (dense != last) {
    ctor(Slot(dense));
    copy(Slot(dense), Slot(last));
    dtor(Slot(last));
    entities[dense] = entities[last];
    sparse[entities[dense].index] = static_cast<uint32_t>(dense);
  }
  sparse[id.index] = kInvalidIndex;
  entities.pop_back();
  --count;
  return true;
}
void EcsComponentColumn::Clear() {
  for (size_t i = 0; i < count; ++i) {
    dtor(data + i * stride);
  }
  std::free(data);
  data = nullptr;
  capacity = 0;
  count = 0;
  entities.clear();
  sparse.clear();
}
EcsWorld::EcsWorld() {
  columns.resize(kEcsMaxComponents);
  EnsureColumn(kEcsTransform);
}
EcsWorld::~EcsWorld() {
}
bool EcsWorld::IsLive(EntityId id) const {
  return id.index < records.size() && records[id.index].alive && records[id.index].id == id;
}
bool EcsWorld::Has(EntityId id) const {
  return IsLive(id);
}
EntityId EcsWorld::CreateEntity() {
  while (!freeList.empty()) {
    const uint32_t index = freeList.back();
    freeList.pop_back();
    if (index < records.size() && !records[index].alive) {
      EntityId id;
      id.index = index;
      id.generation = records[index].id.generation + 1;
      if (id.generation == 0) {
        id.generation = 1;
      }
      records[index].id = id;
      records[index].alive = true;
      ++liveCount;
      return id;
    }
  }
  EntityId id;
  id.index = static_cast<uint32_t>(records.size());
  id.generation = 1;
  Record record;
  record.id = id;
  record.alive = true;
  records.push_back(record);
  ++liveCount;
  return id;
}
EntityId EcsWorld::CreateEntityWithId(EntityId id) {
  if (id.index == 0xFFFFFFFFu) {
    return EntityId::Invalid();
  }
  if (id.index >= records.size()) {
    records.resize(static_cast<size_t>(id.index) + 1);
  }
  Record& record = records[id.index];
  if (record.alive) {
    return record.id == id ? record.id : EntityId::Invalid();
  }
  record.id = id;
  record.alive = true;
  ++liveCount;
  return id;
}
bool EcsWorld::DestroyEntity(EntityId id) {
  if (!IsLive(id)) {
    return false;
  }
  for (size_t i = 0; i < columns.size(); ++i) {
    if (columns[i]) {
      columns[i]->Remove(id);
    }
  }
  records[id.index].alive = false;
  freeList.push_back(id.index);
  --liveCount;
  return true;
}
std::vector<EntityId> EcsWorld::Entities() const {
  std::vector<EntityId> ids;
  ids.reserve(liveCount);
  for (size_t i = 0; i < records.size(); ++i) {
    if (records[i].alive) {
      ids.push_back(records[i].id);
    }
  }
  return ids;
}
EcsComponentColumn* EcsWorld::EnsureColumn(EcsComponentId type) {
  if (type >= kEcsMaxComponents) {
    return nullptr;
  }
  if (columns[type]) {
    return columns[type].get();
  }
  std::unique_ptr<EcsComponentColumn> column(new EcsComponentColumn());
  bool ready = false;
  if (type == kEcsTransform) {
    ready = column->InitializeRaw(sizeof(Transform), &EcsCtor<Transform>, &EcsCopy<Transform>, &EcsDtor<Transform>, &EcsResetTransform);
  } else {
    const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(type);
    ready = desc != nullptr && column->Initialize(*desc);
  }
  if (!ready) {
    return nullptr;
  }
  columns[type] = std::move(column);
  return columns[type].get();
}
EcsComponentColumn* EcsWorld::Column(EcsComponentId type) {
  return type < columns.size() ? columns[type].get() : nullptr;
}
const EcsComponentColumn* EcsWorld::Column(EcsComponentId type) const {
  return type < columns.size() ? columns[type].get() : nullptr;
}
bool EcsWorld::AddComponent(EntityId id, EcsComponentId type, const void* initial) {
  if (!IsLive(id)) {
    return false;
  }
  EcsComponentColumn* column = EnsureColumn(type);
  return column != nullptr && column->Add(id, initial);
}
bool EcsWorld::RemoveComponent(EntityId id, EcsComponentId type) {
  if (!IsLive(id)) {
    return false;
  }
  EcsComponentColumn* column = Column(type);
  return column != nullptr && column->Remove(id);
}
bool EcsWorld::HasComponent(EntityId id, EcsComponentId type) const {
  if (!IsLive(id)) {
    return false;
  }
  const EcsComponentColumn* column = Column(type);
  return column != nullptr && column->Has(id);
}
void* EcsWorld::GetComponent(EntityId id, EcsComponentId type) {
  return const_cast<void*>(static_cast<const EcsWorld*>(this)->GetComponent(id, type));
}
const void* EcsWorld::GetComponent(EntityId id, EcsComponentId type) const {
  if (!IsLive(id)) {
    return nullptr;
  }
  const EcsComponentColumn* column = Column(type);
  return column != nullptr ? column->Get(id) : nullptr;
}
bool EcsWorld::SetComponent(EntityId id, EcsComponentId type, const void* data) {
  return data != nullptr && AddComponent(id, type, data);
}
EcsComponentMask EcsWorld::Mask(EntityId id) const {
  EcsComponentMask mask = 0;
  if (HasComponent(id, kEcsTransform)) {
    mask |= EcsBit(kEcsTransform);
  }
  const std::vector<ComponentTypeDesc>& types = ComponentRegistry::Instance().All();
  for (size_t i = 0; i < types.size(); ++i) {
    if (types[i].reset != nullptr && HasComponent(id, types[i].id)) {
      mask |= EcsBit(types[i].id);
    }
  }
  return mask;
}
EcsQuery EcsWorld::Query(EcsComponentMask required) {
  return EcsQuery(*this, required);
}
void EcsWorld::Clear() {
  for (size_t i = 0; i < columns.size(); ++i) {
    if (columns[i]) {
      columns[i]->Clear();
    }
  }
  records.clear();
  freeList.clear();
  liveCount = 0;
}
EcsQuery::EcsQuery(EcsWorld& worldRef, EcsComponentMask requiredMask) : world(&worldRef), required(requiredMask) {
  for (EcsComponentId id = 0; id < kEcsMaxComponents; ++id) {
    if ((required & EcsBit(id)) == 0) {
      continue;
    }
    const EcsComponentColumn* column = world->Column(id);
    if (column == nullptr) {
      invalid = true;
      return;
    }
    columns.push_back(column);
  }
}
size_t EcsQuery::Count() const {
  if (world == nullptr || invalid) {
    return 0;
  }
  const EcsComponentColumn* primary = nullptr;
  for (size_t i = 0; i < columns.size(); ++i) {
    if (columns[i] == nullptr) {
      return 0;
    }
    if (primary == nullptr || columns[i]->Count() < primary->Count()) {
      primary = columns[i];
    }
  }
  if (primary == nullptr) {
    return 0;
  }
  size_t matched = 0;
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
      ++matched;
    }
  }
  return matched;
}
void EcsSystemScheduler::AddSystem(const std::string& name, EcsComponentMask reads, EcsComponentMask writes, EcsSystemFn fn) {
  System system;
  system.name = name;
  system.reads = reads;
  system.writes = writes;
  system.fn = std::move(fn);
  systems.push_back(std::move(system));
}
void EcsSystemScheduler::Clear() {
  systems.clear();
  lastWaveCount = 0;
}
bool EcsSystemScheduler::Conflicts(const System& a, const System& b) {
  return (a.writes & (b.reads | b.writes)) != 0 || (b.writes & a.reads) != 0;
}
size_t EcsSystemScheduler::Run(EcsWorld& world, JobSystem& jobs, float dt) {
  std::vector<std::vector<size_t>> waves;
  for (size_t i = 0; i < systems.size(); ++i) {
    bool placed = false;
    for (size_t w = 0; w < waves.size(); ++w) {
      bool conflict = false;
      for (size_t s = 0; s < waves[w].size(); ++s) {
        if (Conflicts(systems[i], systems[waves[w][s]])) {
          conflict = true;
          break;
        }
      }
      if (!conflict) {
        waves[w].push_back(i);
        placed = true;
        break;
      }
    }
    if (!placed) {
      waves.push_back(std::vector<size_t>(1, i));
    }
  }
  for (size_t w = 0; w < waves.size(); ++w) {
    if (waves[w].size() == 1) {
      if (systems[waves[w][0]].fn) {
        systems[waves[w][0]].fn(world, dt);
      }
    } else {
      jobs.ParallelFor(static_cast<uint32_t>(waves[w].size()), [this, &world, &waves, w, dt](uint32_t index) {
        const size_t systemIndex = waves[w][index];
        if (systems[systemIndex].fn) {
          systems[systemIndex].fn(world, dt);
        }
      }, 1);
    }
  }
  lastWaveCount = waves.size();
  return lastWaveCount;
}
void SceneEcsBridge::Import(Scene& scene, EcsWorld& world) {
  world.Clear();
  const std::vector<EntityId> ids = scene.All();
  const std::vector<ComponentTypeDesc>& types = ComponentRegistry::Instance().All();
  for (size_t i = 0; i < ids.size(); ++i) {
    const Entity* entity = scene.Get(ids[i]);
    if (entity == nullptr) {
      continue;
    }
    world.CreateEntityWithId(ids[i]);
    world.AddComponent(ids[i], kEcsTransform, &entity->transform);
    for (size_t t = 0; t < types.size(); ++t) {
      if (types[t].reset != nullptr && EntityHasComponent(entity, types[t].id)) {
        world.AddComponent(ids[i], types[t].id, EntityComponentData(entity, types[t].id));
      }
    }
  }
}
void SceneEcsBridge::ExportTransforms(const EcsWorld& world, Scene& scene) {
  const std::vector<EntityId> ids = world.Entities();
  for (size_t i = 0; i < ids.size(); ++i) {
    Entity* entity = scene.Get(ids[i]);
    const Transform* transform = static_cast<const Transform*>(world.GetComponent(ids[i], kEcsTransform));
    if (entity != nullptr && transform != nullptr) {
      entity->transform = *transform;
    }
  }
}
void SceneEcsBridge::ExportComponents(const EcsWorld& world, Scene& scene) {
  const std::vector<EntityId> ids = world.Entities();
  const std::vector<ComponentTypeDesc>& types = ComponentRegistry::Instance().All();
  for (size_t i = 0; i < ids.size(); ++i) {
    Entity* entity = scene.Get(ids[i]);
    if (entity == nullptr) {
      continue;
    }
    const Transform* transform = static_cast<const Transform*>(world.GetComponent(ids[i], kEcsTransform));
    if (transform != nullptr) {
      entity->transform = *transform;
    }
    for (size_t t = 0; t < types.size(); ++t) {
      if (types[t].reset == nullptr) {
        continue;
      }
      const bool ecsHas = world.HasComponent(ids[i], types[t].id);
      const bool sceneHas = EntityHasComponent(entity, types[t].id);
      if (ecsHas && !sceneHas) {
        AddEntityComponent(entity, types[t].id);
      } else if (!ecsHas && sceneHas) {
        RemoveEntityComponent(entity, types[t].id);
      }
      if (ecsHas) {
        const void* source = world.GetComponent(ids[i], types[t].id);
        void* target = EntityComponentData(entity, types[t].id);
        if (source != nullptr && target != nullptr && types[t].copy != nullptr) {
          types[t].copy(target, source);
        }
      }
    }
  }
}
}
