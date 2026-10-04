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
struct DecalEdit {
  EntityId target;
  DecalData value;
};
class EditQueue {
public:
  void PushTransform(EntityId target, const Transform& value);
  void PushLight(EntityId target, const LightData& value);
  void PushMaterial(EntityId target, const MaterialData& value);
  void PushDecal(EntityId target, const DecalData& value);
  size_t Pending() const;
  size_t ApplyAll(Scene& scene, UndoStack& undo);
  void Clear();
private:
  std::vector<TransformEdit> edits;
  std::vector<LightEdit> lightEdits;
  std::vector<MaterialEdit> materialEdits;
  std::vector<DecalEdit> decalEdits;
};
}
