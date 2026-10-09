#pragma once
#include "Kizuri/Scene.h"
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
class EditQueue {
public:
  void PushTransform(EntityId target, const Transform& value);
  void PushLight(EntityId target, const LightData& value);
  void PushMaterial(EntityId target, const MaterialData& value);
  void PushPartMaterial(EntityId target, uint32_t part, const MaterialData& value);
  void PushDecal(EntityId target, const DecalData& value);
  void PushLod(EntityId target, const LodData& value);
  void PushLightmap(EntityId target, const LightmapData& value);
  void PushProbe(EntityId target, const ProbeData& value);
  void PushPost(EntityId target, const PostData& value);
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
  std::vector<LightmapEdit> lightmapEdits;
  std::vector<ProbeEdit> probeEdits;
  std::vector<PostEdit> postEdits;
};
}
