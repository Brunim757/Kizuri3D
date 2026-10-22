#pragma once
#include "Kizuri/ComponentRegistry.h"
#include "Kizuri/Scene.h"
#include <memory>
#include <string>
#include <vector>
namespace Kizuri {
struct Command {
  virtual ~Command() = default;
  virtual bool Apply(Scene& scene) = 0;
  virtual bool Revert(Scene& scene) = 0;
  virtual const char* Name() const = 0;
};
struct SnapshotNode {
  std::string name;
  Transform transform;
  std::string meshGuid;
  bool hasMesh;
  LightData light;
  bool hasLight;
  MaterialData material;
  bool hasMaterial;
  DecalData decal;
  bool hasDecal;
  LodData lod;
  bool hasLod;
  LightmapData lightmap;
  bool hasLightmap;
  ProbeData probe;
  bool hasProbe;
  long parent;
};
struct EntitySnapshot {
  std::vector<SnapshotNode> nodes;
  EntityId originalParent;
  bool Capture(const Scene& scene, EntityId root);
  EntityId Restore(Scene& scene, EntityId newParent) const;
};
class ComponentSnapshot {
public:
  ComponentSnapshot() = default;
  ~ComponentSnapshot();
  ComponentSnapshot(const ComponentSnapshot&) = delete;
  ComponentSnapshot& operator=(const ComponentSnapshot&) = delete;
  ComponentSnapshot(ComponentSnapshot&& other) noexcept;
  ComponentSnapshot& operator=(ComponentSnapshot&& other) noexcept;
  bool Capture(const ComponentTypeDesc* type, const void* data);
  bool Restore(void* data) const;
  void Reset();
  bool Valid() const { return valid; }
private:
  ComponentTypeDesc type;
  unsigned char* storage = nullptr;
  bool valid = false;
};
class AddComponentCmd : public Command {
public:
  AddComponentCmd(EntityId target, ComponentTypeId type, const void* initialData = nullptr);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  ComponentTypeId type;
  const void* initialData = nullptr;
  ComponentSnapshot prevState;
  bool prevHas = false;
};
class RemoveComponentCmd : public Command {
public:
  RemoveComponentCmd(EntityId target, ComponentTypeId type);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  ComponentTypeId type;
  ComponentSnapshot prevState;
  bool prevHas = false;
};
class CreateEntityCmd : public Command {
public:
  CreateEntityCmd(const std::string& name, const Transform& t, EntityId parent, const std::string& mesh = "");
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  std::string name;
  Transform transform;
  EntityId parent;
  std::string mesh;
  EntityId live;
};
class DeleteEntityCmd : public Command {
public:
  explicit DeleteEntityCmd(EntityId target);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  EntityId live;
  EntitySnapshot snapshot;
  bool captured;
};
class DuplicateCmd : public Command {
public:
  explicit DuplicateCmd(EntityId source);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId source;
  EntityId live;
  EntitySnapshot snapshot;
  bool captured;
};
class EditTransformCmd : public Command {
public:
  EditTransformCmd(EntityId target, const Transform& before, const Transform& after);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  Transform before;
  Transform after;
};
class RenameCmd : public Command {
public:
  RenameCmd(EntityId target, const std::string& before, const std::string& after);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  std::string before;
  std::string after;
};
class SetMeshGuidCmd : public Command {
public:
  SetMeshGuidCmd(EntityId target, const std::string& before, const std::string& after);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  std::string before;
  std::string after;
};
class SetLightCmd : public Command {
public:
  SetLightCmd(EntityId target, const LightData& before, const LightData& after);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  LightData before;
  LightData after;
};
class SetMaterialCmd : public Command {
public:
  SetMaterialCmd(EntityId target, const MaterialData& before, const MaterialData& after);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  MaterialData before;
  MaterialData after;
};
class SetVariantCmd : public Command {
public:
  SetVariantCmd(EntityId target, int before, int after);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  int before;
  int after;
};
class SetPartMaterialCmd : public Command {
public:
  SetPartMaterialCmd(EntityId target, uint32_t part, const MaterialData& before, const MaterialData& after);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  uint32_t part;
  MaterialData before;
  MaterialData after;
};
class AddPartMaterialCmd : public Command {
public:
  AddPartMaterialCmd(EntityId target, uint32_t part, const MaterialData& initial);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  uint32_t part;
  MaterialData initial;
};
class RemovePartMaterialCmd : public Command {
public:
  RemovePartMaterialCmd(EntityId target, uint32_t part);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  uint32_t part;
  MaterialData prevMaterial;
  bool hadIt;
};
class SetDecalCmd : public Command {
public:
  SetDecalCmd(EntityId target, const DecalData& before, const DecalData& after);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  DecalData before;
  DecalData after;
};
class SetPostCmd : public Command {
public:
  SetPostCmd(EntityId target, const PostData& before, const PostData& after);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  PostData before;
  PostData after;
};
class SetFogCmd : public Command {
public:
  SetFogCmd(EntityId target, const FogData& before, const FogData& after);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  FogData before;
  FogData after;
};
class SetLodCmd : public Command {
public:
  SetLodCmd(EntityId target, const LodData& before, const LodData& after);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  LodData before;
  LodData after;
};
class SetStaticCmd : public Command {
public:
  SetStaticCmd(EntityId target, bool before, bool after);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  bool before;
  bool after;
};
class SetRigidbodyCmd : public Command {
public:
  SetRigidbodyCmd(EntityId target, const RigidbodyData& before, const RigidbodyData& after);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  RigidbodyData before;
  RigidbodyData after;
};
class SetAudioSourceCmd : public Command {
public:
  SetAudioSourceCmd(EntityId target, const AudioSourceData& before, const AudioSourceData& after);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  AudioSourceData before;
  AudioSourceData after;
};
class SetLightmapCmd : public Command {
public:
  SetLightmapCmd(EntityId target, const LightmapData& before, const LightmapData& after);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  LightmapData before;
  LightmapData after;
};
class SetProbeCmd : public Command {
public:
  SetProbeCmd(EntityId target, const ProbeData& before, const ProbeData& after);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  ProbeData before;
  ProbeData after;
};
class SetParentCmd : public Command {
public:
  SetParentCmd(EntityId child, EntityId before, EntityId after);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId child;
  EntityId before;
  EntityId after;
};
struct MultiTransformEdit {
  EntityId target;
  Transform before;
  Transform after;
};
class MultiEditTransformCmd : public Command {
public:
  MultiEditTransformCmd();
  void Add(EntityId target, const Transform& before, const Transform& after);
  bool Empty() const;
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  std::vector<MultiTransformEdit> edits;
};
class UndoStack {
public:
  UndoStack();
  bool Execute(std::unique_ptr<Command> cmd, Scene& scene);
  bool Commit(std::unique_ptr<Command> cmd);
  bool Undo(Scene& scene);
  bool Redo(Scene& scene);
  bool CanUndo() const;
  bool CanRedo() const;
  const char* UndoName() const;
  const char* RedoName() const;
  void Clear();
  size_t UndoDepth() const;
  size_t RedoDepth() const;
private:
  std::vector<std::unique_ptr<Command>> undo;
  std::vector<std::unique_ptr<Command>> redo;
};
}
