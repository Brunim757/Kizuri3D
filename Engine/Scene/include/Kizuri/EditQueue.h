#pragma once
#include "Kizuri/ComponentRegistry.h"
#include "Kizuri/Scene.h"
#include "Kizuri/Undo.h"
#include <vector>
namespace Kizuri {
class UndoStack;
struct TransformEdit {
  EntityId target;
  Transform value;
};
struct LightEdit {
  EntityId target;
  LightData value;
};
struct MaterialEdit {
  EntityId target;
  MaterialData value;
};
struct PartMaterialEdit {
  EntityId target;
  uint32_t part;
  MaterialData value;
};
struct DecalEdit {
  EntityId target;
  DecalData value;
};
struct LodEdit {
  EntityId target;
  LodData value;
};
struct RigidbodyEdit {
  EntityId target;
  RigidbodyData value;
};
struct AudioSourceEdit {
  EntityId target;
  AudioSourceData value;
};
struct StaticEdit {
  EntityId target;
  bool value;
};
struct LightmapEdit {
  EntityId target;
  LightmapData value;
};
struct ProbeEdit {
  EntityId target;
  ProbeData value;
};
struct PostEdit {
  EntityId target;
  PostData value;
};
struct FogEdit {
  EntityId target;
  FogData value;
};
struct ComponentEdit {
  EntityId target;
  ComponentTypeId type;
  ComponentSnapshot value;
};
class EditQueue {
public:
  void PushTransform(EntityId target, const Transform& value);
  void PushLight(EntityId target, const LightData& value);
  void PushMaterial(EntityId target, const MaterialData& value);
  void PushPartMaterial(EntityId target, uint32_t part, const MaterialData& value);
  void PushDecal(EntityId target, const DecalData& value);
  void PushLod(EntityId target, const LodData& value);
  void PushRigidbody(EntityId target, const RigidbodyData& value);
  void PushAudioSource(EntityId target, const AudioSourceData& value);
  void PushStatic(EntityId target, bool value);
  void PushLightmap(EntityId target, const LightmapData& value);
  void PushProbe(EntityId target, const ProbeData& value);
  void PushPost(EntityId target, const PostData& value);
  void PushFog(EntityId target, const FogData& value);
  void PushComponent(EntityId target, ComponentTypeId type, const void* value);
  size_t Pending() const;
  size_t ApplyAll(Scene& scene, UndoStack& undo);
  void Clear();
private:
  std::vector<TransformEdit> edits;
  std::vector<LightEdit> lightEdits;
  std::vector<MaterialEdit> materialEdits;
  std::vector<PartMaterialEdit> partMaterialEdits;
  std::vector<DecalEdit> decalEdits;
  std::vector<LodEdit> lodEdits;
  std::vector<RigidbodyEdit> rigidbodyEdits;
  std::vector<AudioSourceEdit> audioSourceEdits;
  std::vector<StaticEdit> staticEdits;
  std::vector<LightmapEdit> lightmapEdits;
  std::vector<ProbeEdit> probeEdits;
  std::vector<PostEdit> postEdits;
  std::vector<FogEdit> fogEdits;
  std::vector<ComponentEdit> componentEdits;
};
}
