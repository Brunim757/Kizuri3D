#include "Kizuri/Undo.h"
#include <cstring>
namespace Kizuri {
bool EntitySnapshot::Capture(const Scene& scene, EntityId root) {
  nodes.clear();
  originalParent = EntityId::Invalid();
  const Entity* r = scene.Get(root);
  if (r == nullptr) {
    return false;
  }
  originalParent = r->parent;
  std::vector<EntityId> stack;
  std::vector<long> parentStack;
  stack.push_back(root);
  parentStack.push_back(-1);
  while (!stack.empty()) {
    EntityId cur = stack.back();
    stack.pop_back();
    long par = parentStack.back();
    parentStack.pop_back();
    const Entity* e = scene.Get(cur);
    if (e == nullptr) {
      return false;
    }
    SnapshotNode node;
    node.name = e->name;
    node.transform = e->transform;
    node.meshGuid = e->meshGuid;
    node.hasMesh = e->hasMesh;
    node.light = e->light;
    node.hasLight = e->hasLight;
    node.material = e->material;
    node.hasMaterial = e->hasMaterial;
    node.decal = e->decal;
    node.hasDecal = e->hasDecal;
    node.lod = e->lod;
    node.hasLod = e->hasLod;
    node.lightmap = e->lightmap;
    node.hasLightmap = e->hasLightmap;
    node.probe = e->probe;
    node.hasProbe = e->hasProbe;
    node.parent = par;
    long idx = static_cast<long>(nodes.size());
    nodes.push_back(node);
    for (size_t i = 0; i < e->children.size(); ++i) {
      stack.push_back(e->children[i]);
      parentStack.push_back(idx);
    }
  }
  return !nodes.empty();
}
EntityId EntitySnapshot::Restore(Scene& scene, EntityId newParent) const {
  if (nodes.empty()) {
    return EntityId::Invalid();
  }
  std::vector<EntityId> created;
  for (size_t i = 0; i < nodes.size(); ++i) {
    EntityId id = scene.CreateEntity(nodes[i].name);
    if (!id.IsValid()) {
      return EntityId::Invalid();
    }
    scene.SetTransform(id, nodes[i].transform);
    Entity* createdEntity = scene.Get(id);
    if (createdEntity != nullptr) {
      createdEntity->meshGuid = nodes[i].meshGuid;
      createdEntity->hasMesh = nodes[i].hasMesh;
      createdEntity->light = nodes[i].light;
      createdEntity->hasLight = nodes[i].hasLight;
      createdEntity->material = nodes[i].material;
      createdEntity->hasMaterial = nodes[i].hasMaterial;
      createdEntity->decal = nodes[i].decal;
      createdEntity->hasDecal = nodes[i].hasDecal;
      createdEntity->lod = nodes[i].lod;
      createdEntity->hasLod = nodes[i].hasLod;
      createdEntity->lightmap = nodes[i].lightmap;
      createdEntity->hasLightmap = nodes[i].hasLightmap;
      createdEntity->probe = nodes[i].probe;
      createdEntity->hasProbe = nodes[i].hasProbe;
    }
    created.push_back(id);
  }
  for (size_t i = 1; i < nodes.size(); ++i) {
    if (nodes[i].parent >= 0) {
      scene.SetParent(created[i], created[static_cast<size_t>(nodes[i].parent)]);
    }
  }
  if (newParent.IsValid()) {
    scene.SetParent(created[0], newParent);
  }
  return created[0];
}
ComponentSnapshot::~ComponentSnapshot() {
  Reset();
}
ComponentSnapshot::ComponentSnapshot(ComponentSnapshot&& other) noexcept
  : type(other.type)
  , storage(other.storage)
  , valid(other.valid) {
  other.storage = nullptr;
  other.valid = false;
}
ComponentSnapshot& ComponentSnapshot::operator=(ComponentSnapshot&& other) noexcept {
  if (this != &other) {
    Reset();
    type = other.type;
    storage = other.storage;
    valid = other.valid;
    other.storage = nullptr;
    other.valid = false;
  }
  return *this;
}
bool ComponentSnapshot::Capture(const ComponentTypeDesc* d, const void* data) {
  Reset();
  if (d == nullptr || d->ctor == nullptr || d->copy == nullptr || d->dtor == nullptr || data == nullptr) {
    return false;
  }
  unsigned char* mem = new unsigned char[d->dataSize];
  d->ctor(mem);
  d->copy(mem, data);
  type = *d;
  storage = mem;
  valid = true;
  return true;
}
bool ComponentSnapshot::Restore(void* data) const {
  if (!valid || storage == nullptr || data == nullptr) {
    return false;
  }
  type.copy(data, storage);
  return true;
}
void ComponentSnapshot::Reset() {
  if (valid && storage != nullptr && type.dtor != nullptr) {
    type.dtor(storage);
  }
  delete[] storage;
  storage = nullptr;
  valid = false;
}
AddComponentCmd::AddComponentCmd(EntityId t, ComponentTypeId ty, const void* initial)
  : target(t)
  , type(ty)
  , initialData(initial) {
}
bool AddComponentCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr) {
    return false;
  }
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(type);
  if (desc == nullptr) {
    return false;
  }
  void* data = EntityComponentData(e, type);
  if (data == nullptr) {
    return false;
  }
  if (!prevState.Capture(desc, data)) {
    return false;
  }
  prevHas = EntityHasComponent(e, type);
  if (!AddEntityComponent(e, type)) {
    return false;
  }
  if (initialData != nullptr) {
    desc->copy(data, initialData);
  }
  scene.MarkDirty();
  return true;
}
bool AddComponentCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr) {
    return false;
  }
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(type);
  if (desc == nullptr) {
    return false;
  }
  void* data = EntityComponentData(e, type);
  if (data == nullptr) {
    return false;
  }
  if (prevHas) {
    AddEntityComponent(e, type);
    prevState.Restore(data);
  } else {
    RemoveEntityComponent(e, type);
  }
  scene.MarkDirty();
  return true;
}
const char* AddComponentCmd::Name() const {
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(type);
  return desc != nullptr ? desc->name : "Add Component";
}
RemoveComponentCmd::RemoveComponentCmd(EntityId t, ComponentTypeId ty)
  : target(t)
  , type(ty) {
}
bool RemoveComponentCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr) {
    return false;
  }
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(type);
  if (desc == nullptr) {
    return false;
  }
  void* data = EntityComponentData(e, type);
  if (data == nullptr) {
    return false;
  }
  if (!prevState.Capture(desc, data)) {
    return false;
  }
  prevHas = EntityHasComponent(e, type);
  if (!RemoveEntityComponent(e, type)) {
    return false;
  }
  ResetEntityComponent(e, type);
  scene.MarkDirty();
  return true;
}
bool RemoveComponentCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr) {
    return false;
  }
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(type);
  if (desc == nullptr) {
    return false;
  }
  void* data = EntityComponentData(e, type);
  if (data == nullptr) {
    return false;
  }
  if (prevHas) {
    AddEntityComponent(e, type);
    prevState.Restore(data);
  }
  scene.MarkDirty();
  return true;
}
const char* RemoveComponentCmd::Name() const {
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(type);
  return desc != nullptr ? desc->name : "Remove Component";
}
CreateEntityCmd::CreateEntityCmd(const std::string& n, const Transform& t, EntityId p, const std::string& m)
  : name(n)
  , transform(t)
  , parent(p)
  , mesh(m)
  , live(EntityId::Invalid()) {
}
bool CreateEntityCmd::Apply(Scene& scene) {
  EntityId useParent = parent.IsValid() && scene.Has(parent) ? parent : EntityId::Invalid();
  live = scene.CreateEntity(name.empty() ? "Entity" : name);
  if (!live.IsValid()) {
    return false;
  }
  scene.SetTransform(live, transform);
  if (!mesh.empty()) {
    Entity* e = scene.Get(live);
    if (e != nullptr) {
      e->meshGuid = mesh;
      e->hasMesh = true;
    }
  }
  if (useParent.IsValid()) {
    scene.SetParent(live, useParent);
  }
  return true;
}
bool CreateEntityCmd::Revert(Scene& scene) {
  if (!live.IsValid() || !scene.Has(live)) {
    return false;
  }
  return scene.DeleteEntity(live);
}
const char* CreateEntityCmd::Name() const {
  return "Create Entity";
}
DeleteEntityCmd::DeleteEntityCmd(EntityId t)
  : target(t)
  , live(t)
  , captured(false) {
}
bool DeleteEntityCmd::Apply(Scene& scene) {
  if (!captured) {
    if (!snapshot.Capture(scene, target)) {
      return false;
    }
    captured = true;
    live = target;
  }
  if (!live.IsValid() || !scene.Has(live)) {
    return false;
  }
  return scene.DeleteEntity(live);
}
bool DeleteEntityCmd::Revert(Scene& scene) {
  EntityId useParent = snapshot.originalParent.IsValid() && scene.Has(snapshot.originalParent) ? snapshot.originalParent : EntityId::Invalid();
  live = snapshot.Restore(scene, useParent);
  return live.IsValid();
}
const char* DeleteEntityCmd::Name() const {
  return "Delete Entity";
}
DuplicateCmd::DuplicateCmd(EntityId s)
  : source(s)
  , live(EntityId::Invalid())
  , captured(false) {
}
bool DuplicateCmd::Apply(Scene& scene) {
  if (!captured) {
    const Entity* e = scene.Get(source);
    if (e == nullptr) {
      return false;
    }
    if (!snapshot.Capture(scene, source)) {
      return false;
    }
    for (size_t i = 0; i < snapshot.nodes.size(); ++i) {
      snapshot.nodes[i].name += " Copy";
    }
    captured = true;
  }
  const Entity* e = scene.Get(source);
  EntityId useParent = (e != nullptr && e->parent.IsValid() && scene.Has(e->parent)) ? e->parent : EntityId::Invalid();
  if (!useParent.IsValid() && snapshot.originalParent.IsValid() && scene.Has(snapshot.originalParent)) {
    useParent = snapshot.originalParent;
  }
  live = snapshot.Restore(scene, useParent);
  return live.IsValid();
}
bool DuplicateCmd::Revert(Scene& scene) {
  if (!live.IsValid() || !scene.Has(live)) {
    return false;
  }
  return scene.DeleteEntity(live);
}
const char* DuplicateCmd::Name() const {
  return "Duplicate Entity";
}
EditTransformCmd::EditTransformCmd(EntityId t, const Transform& b, const Transform& a)
  : target(t)
  , before(b)
  , after(a) {
}
bool EditTransformCmd::Apply(Scene& scene) {
  if (std::memcmp(&before, &after, sizeof(Transform)) == 0) {
    return scene.Has(target);
  }
  return scene.SetTransform(target, after);
}
bool EditTransformCmd::Revert(Scene& scene) {
  return scene.SetTransform(target, before);
}
const char* EditTransformCmd::Name() const {
  return "Edit Transform";
}
RenameCmd::RenameCmd(EntityId t, const std::string& b, const std::string& a)
  : target(t)
  , before(b)
  , after(a) {
}
bool RenameCmd::Apply(Scene& scene) {
  return scene.RenameEntity(target, after);
}
bool RenameCmd::Revert(Scene& scene) {
  return scene.RenameEntity(target, before);
}
const char* RenameCmd::Name() const {
  return "Rename Entity";
}
SetMeshGuidCmd::SetMeshGuidCmd(EntityId t, const std::string& b, const std::string& a)
  : target(t)
  , before(b)
  , after(a) {
}
bool SetMeshGuidCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr) {
    return false;
  }
  e->meshGuid = after;
  e->hasMesh = !after.empty();
  scene.MarkDirty();
  return true;
}
bool SetMeshGuidCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr) {
    return false;
  }
  e->meshGuid = before;
  e->hasMesh = !before.empty();
  scene.MarkDirty();
  return true;
}
const char* SetMeshGuidCmd::Name() const {
  return "Set Mesh";
}
SetLightCmd::SetLightCmd(EntityId t, const LightData& b, const LightData& a)
  : target(t)
  , before(b)
  , after(a) {
}
bool SetLightCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasLight) {
    return false;
  }
  e->light = after;
  scene.MarkDirty();
  return true;
}
bool SetLightCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasLight) {
    return false;
  }
  e->light = before;
  scene.MarkDirty();
  return true;
}
const char* SetLightCmd::Name() const {
  return "Set Light";
}
SetMaterialCmd::SetMaterialCmd(EntityId t, const MaterialData& b, const MaterialData& a)
  : target(t)
  , before(b)
  , after(a) {
}
bool SetMaterialCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasMaterial) {
    return false;
  }
  e->material = after;
  scene.MarkDirty();
  return true;
}
bool SetMaterialCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasMaterial) {
    return false;
  }
  e->material = before;
  scene.MarkDirty();
  return true;
}
const char* SetMaterialCmd::Name() const {
  return "Set Material";
}
SetVariantCmd::SetVariantCmd(EntityId t, int b, int a)
  : target(t)
  , before(b)
  , after(a) {
}
bool SetVariantCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr) {
    return false;
  }
  e->variantIndex = after;
  scene.MarkDirty();
  return true;
}
bool SetVariantCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr) {
    return false;
  }
  e->variantIndex = before;
  scene.MarkDirty();
  return true;
}
const char* SetVariantCmd::Name() const {
  return "Set Variant";
}
static MaterialData* FindPartMaterial(Entity& e, uint32_t part) {
  for (size_t i = 0; i < e.partMaterials.size(); ++i) {
    if (e.partMaterials[i].part == part) {
      return &e.partMaterials[i].material;
    }
  }
  return nullptr;
}
SetPartMaterialCmd::SetPartMaterialCmd(EntityId t, uint32_t p, const MaterialData& b, const MaterialData& a)
  : target(t)
  , part(p)
  , before(b)
  , after(a) {
}
bool SetPartMaterialCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr) {
    return false;
  }
  MaterialData* m = FindPartMaterial(*e, part);
  if (m == nullptr) {
    return false;
  }
  *m = after;
  scene.MarkDirty();
  return true;
}
bool SetPartMaterialCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr) {
    return false;
  }
  MaterialData* m = FindPartMaterial(*e, part);
  if (m == nullptr) {
    return false;
  }
  *m = before;
  scene.MarkDirty();
  return true;
}
const char* SetPartMaterialCmd::Name() const {
  return "Set Part Material";
}
AddPartMaterialCmd::AddPartMaterialCmd(EntityId t, uint32_t p, const MaterialData& initial)
  : target(t)
  , part(p)
  , initial(initial) {
}
bool AddPartMaterialCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || FindPartMaterial(*e, part) != nullptr) {
    return false;
  }
  PartMaterial pm;
  pm.part = part;
  pm.material = initial;
  e->partMaterials.push_back(pm);
  scene.MarkDirty();
  return true;
}
bool AddPartMaterialCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr) {
    return false;
  }
  for (size_t i = 0; i < e->partMaterials.size(); ++i) {
    if (e->partMaterials[i].part == part) {
      e->partMaterials.erase(e->partMaterials.begin() + static_cast<long>(i));
      scene.MarkDirty();
      return true;
    }
  }
  return false;
}
const char* AddPartMaterialCmd::Name() const {
  return "Add Part Material";
}
RemovePartMaterialCmd::RemovePartMaterialCmd(EntityId t, uint32_t p)
  : target(t)
  , part(p)
  , hadIt(false) {
  MakeDefaultMaterial(prevMaterial);
}
bool RemovePartMaterialCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr) {
    return false;
  }
  for (size_t i = 0; i < e->partMaterials.size(); ++i) {
    if (e->partMaterials[i].part == part) {
      prevMaterial = e->partMaterials[i].material;
      hadIt = true;
      e->partMaterials.erase(e->partMaterials.begin() + static_cast<long>(i));
      scene.MarkDirty();
      return true;
    }
  }
  return false;
}
bool RemovePartMaterialCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !hadIt) {
    return false;
  }
  PartMaterial pm;
  pm.part = part;
  pm.material = prevMaterial;
  e->partMaterials.push_back(pm);
  scene.MarkDirty();
  return true;
}
const char* RemovePartMaterialCmd::Name() const {
  return "Remove Part Material";
}
SetDecalCmd::SetDecalCmd(EntityId t, const DecalData& b, const DecalData& a)
  : target(t)
  , before(b)
  , after(a) {
}
bool SetDecalCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasDecal) {
    return false;
  }
  e->decal = after;
  scene.MarkDirty();
  return true;
}
bool SetDecalCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasDecal) {
    return false;
  }
  e->decal = before;
  scene.MarkDirty();
  return true;
}
const char* SetDecalCmd::Name() const {
  return "Set Decal";
}
SetPostCmd::SetPostCmd(EntityId t, const PostData& b, const PostData& a)
  : target(t)
  , before(b)
  , after(a) {
}
bool SetPostCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasPost) {
    return false;
  }
  e->post = after;
  scene.MarkDirty();
  return true;
}
bool SetPostCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasPost) {
    return false;
  }
  e->post = before;
  scene.MarkDirty();
  return true;
}
const char* SetPostCmd::Name() const {
  return "Set Post Volume";
}
SetFogCmd::SetFogCmd(EntityId t, const FogData& b, const FogData& a)
  : target(t)
  , before(b)
  , after(a) {
}
bool SetFogCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasFog) {
    return false;
  }
  e->fog = after;
  scene.MarkDirty();
  return true;
}
bool SetFogCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasFog) {
    return false;
  }
  e->fog = before;
  scene.MarkDirty();
  return true;
}
const char* SetFogCmd::Name() const {
  return "Set Fog Volume";
}
SetLodCmd::SetLodCmd(EntityId t, const LodData& b, const LodData& a)
  : target(t)
  , before(b)
  , after(a) {
}
bool SetLodCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasLod) {
    return false;
  }
  e->lod = after;
  scene.MarkDirty();
  return true;
}
bool SetLodCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasLod) {
    return false;
  }
  e->lod = before;
  scene.MarkDirty();
  return true;
}
const char* SetLodCmd::Name() const {
  return "Set LOD";
}
SetStaticCmd::SetStaticCmd(EntityId t, bool b, bool a)
  : target(t)
  , before(b)
  , after(a) {
}
bool SetStaticCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr) {
    return false;
  }
  e->isStatic = after;
  scene.MarkDirty();
  return true;
}
bool SetStaticCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr) {
    return false;
  }
  e->isStatic = before;
  scene.MarkDirty();
  return true;
}
const char* SetStaticCmd::Name() const {
  return "Set Static";
}
SetRigidbodyCmd::SetRigidbodyCmd(EntityId t, const RigidbodyData& b, const RigidbodyData& a)
  : target(t)
  , before(b)
  , after(a) {
}
bool SetRigidbodyCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasRigidbody) {
    return false;
  }
  e->rigidbody = after;
  scene.MarkDirty();
  return true;
}
bool SetRigidbodyCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasRigidbody) {
    return false;
  }
  e->rigidbody = before;
  scene.MarkDirty();
  return true;
}
const char* SetRigidbodyCmd::Name() const {
  return "Set Rigidbody";
}
SetAudioSourceCmd::SetAudioSourceCmd(EntityId t, const AudioSourceData& b, const AudioSourceData& a)
  : target(t)
  , before(b)
  , after(a) {
}
bool SetAudioSourceCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasAudioSource) {
    return false;
  }
  e->audioSource = after;
  scene.MarkDirty();
  return true;
}
bool SetAudioSourceCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasAudioSource) {
    return false;
  }
  e->audioSource = before;
  scene.MarkDirty();
  return true;
}
const char* SetAudioSourceCmd::Name() const {
  return "Set Audio Source";
}
SetLightmapCmd::SetLightmapCmd(EntityId t, const LightmapData& b, const LightmapData& a)
  : target(t)
  , before(b)
  , after(a) {
}
bool SetLightmapCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasLightmap) {
    return false;
  }
  e->lightmap = after;
  scene.MarkDirty();
  return true;
}
bool SetLightmapCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasLightmap) {
    return false;
  }
  e->lightmap = before;
  scene.MarkDirty();
  return true;
}
const char* SetLightmapCmd::Name() const {
  return "Set Lightmap";
}
SetProbeCmd::SetProbeCmd(EntityId t, const ProbeData& b, const ProbeData& a)
  : target(t)
  , before(b)
  , after(a) {
}
bool SetProbeCmd::Apply(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasProbe) {
    return false;
  }
  e->probe = after;
  scene.MarkDirty();
  return true;
}
bool SetProbeCmd::Revert(Scene& scene) {
  Entity* e = scene.Get(target);
  if (e == nullptr || !e->hasProbe) {
    return false;
  }
  e->probe = before;
  scene.MarkDirty();
  return true;
}
const char* SetProbeCmd::Name() const {
  return "Set Probe";
}
SetParentCmd::SetParentCmd(EntityId c, EntityId b, EntityId a)
  : child(c)
  , before(b)
  , after(a) {
}
bool SetParentCmd::Apply(Scene& scene) {
  return scene.SetParent(child, after);
}
bool SetParentCmd::Revert(Scene& scene) {
  return scene.SetParent(child, before);
}
const char* SetParentCmd::Name() const {
  return "Set Parent";
}
MultiEditTransformCmd::MultiEditTransformCmd() {
}
void MultiEditTransformCmd::Add(EntityId target, const Transform& before, const Transform& after) {
  if (!target.IsValid()) {
    return;
  }
  MultiTransformEdit e;
  e.target = target;
  e.before = before;
  e.after = after;
  edits.push_back(e);
}
bool MultiEditTransformCmd::Empty() const {
  return edits.empty();
}
bool MultiEditTransformCmd::Apply(Scene& scene) {
  size_t done = 0;
  for (size_t i = 0; i < edits.size(); ++i) {
    if (scene.SetTransform(edits[i].target, edits[i].after)) {
      ++done;
    }
  }
  return done > 0;
}
bool MultiEditTransformCmd::Revert(Scene& scene) {
  size_t done = 0;
  for (size_t i = 0; i < edits.size(); ++i) {
    if (scene.SetTransform(edits[i].target, edits[i].before)) {
      ++done;
    }
  }
  return done > 0;
}
const char* MultiEditTransformCmd::Name() const {
  return "Edit Transforms";
}
UndoStack::UndoStack() {
}
bool UndoStack::Execute(std::unique_ptr<Command> cmd, Scene& scene) {
  if (cmd == nullptr || !cmd->Apply(scene)) {
    return false;
  }
  undo.push_back(std::move(cmd));
  redo.clear();
  return true;
}
bool UndoStack::Commit(std::unique_ptr<Command> cmd) {
  if (cmd == nullptr) {
    return false;
  }
  undo.push_back(std::move(cmd));
  redo.clear();
  return true;
}
bool UndoStack::Undo(Scene& scene) {
  if (undo.empty()) {
    return false;
  }
  std::unique_ptr<Command> cmd = std::move(undo.back());
  undo.pop_back();
  cmd->Revert(scene);
  redo.push_back(std::move(cmd));
  return true;
}
bool UndoStack::Redo(Scene& scene) {
  if (redo.empty()) {
    return false;
  }
  std::unique_ptr<Command> cmd = std::move(redo.back());
  redo.pop_back();
  cmd->Apply(scene);
  undo.push_back(std::move(cmd));
  return true;
}
bool UndoStack::CanUndo() const {
  return !undo.empty();
}
bool UndoStack::CanRedo() const {
  return !redo.empty();
}
const char* UndoStack::UndoName() const {
  return undo.empty() ? "" : undo.back()->Name();
}
const char* UndoStack::RedoName() const {
  return redo.empty() ? "" : redo.back()->Name();
}
void UndoStack::Clear() {
  undo.clear();
  redo.clear();
}
size_t UndoStack::UndoDepth() const {
  return undo.size();
}
size_t UndoStack::RedoDepth() const {
  return redo.size();
}
}
