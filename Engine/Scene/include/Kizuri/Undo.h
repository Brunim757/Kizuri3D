#pragma once
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
class AddMeshCmd : public Command {
public:
  explicit AddMeshCmd(EntityId target);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  std::string prevGuid;
};
class RemoveMeshCmd : public Command {
public:
  explicit RemoveMeshCmd(EntityId target);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  std::string prevGuid;
  bool applied;
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
class AddLightCmd : public Command {
public:
  explicit AddLightCmd(EntityId target);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  LightData prevLight;
  bool prevHas;
};
class RemoveLightCmd : public Command {
public:
  explicit RemoveLightCmd(EntityId target);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  LightData prevLight;
  bool prevHas;
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
class AddMaterialCmd : public Command {
public:
  explicit AddMaterialCmd(EntityId target);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  MaterialData prevMaterial;
  bool prevHas;
};
class RemoveMaterialCmd : public Command {
public:
  explicit RemoveMaterialCmd(EntityId target);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  MaterialData prevMaterial;
  bool prevHas;
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
class AddDecalCmd : public Command {
public:
  explicit AddDecalCmd(EntityId target);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  DecalData prevDecal;
  bool prevHas;
};
class RemoveDecalCmd : public Command {
public:
  explicit RemoveDecalCmd(EntityId target);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  DecalData prevDecal;
  bool prevHas;
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
class AddLodCmd : public Command {
public:
  explicit AddLodCmd(EntityId target);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  LodData prevLod;
  bool prevHas;
};
class RemoveLodCmd : public Command {
public:
  explicit RemoveLodCmd(EntityId target);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  LodData prevLod;
  bool prevHas;
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
class AddLightmapCmd : public Command {
public:
  explicit AddLightmapCmd(EntityId target);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  LightmapData prevLightmap;
  bool prevHas;
};
class RemoveLightmapCmd : public Command {
public:
  explicit RemoveLightmapCmd(EntityId target);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  LightmapData prevLightmap;
  bool prevHas;
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
class AddProbeCmd : public Command {
public:
  explicit AddProbeCmd(EntityId target);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  ProbeData prevProbe;
  bool prevHas;
};
class RemoveProbeCmd : public Command {
public:
  explicit RemoveProbeCmd(EntityId target);
  bool Apply(Scene& scene) override;
  bool Revert(Scene& scene) override;
  const char* Name() const override;
private:
  EntityId target;
  ProbeData prevProbe;
  bool prevHas;
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
