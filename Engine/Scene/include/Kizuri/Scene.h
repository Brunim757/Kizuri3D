#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include "Kizuri/Origin.h"
namespace Kizuri {
struct EntityId {
  uint32_t index;
  uint32_t generation;
  bool operator==(const EntityId& o) const { return index == o.index && generation == o.generation; }
  bool operator!=(const EntityId& o) const { return !(*this == o); }
  bool IsValid() const { return index != 0xFFFFFFFFu; }
  static EntityId Invalid() { EntityId e; e.index = 0xFFFFFFFFu; e.generation = 0; return e; }
};
struct EntityIdHash {
  size_t operator()(const EntityId& e) const { return (static_cast<size_t>(e.index) * 1315423911u) ^ static_cast<size_t>(e.generation); }
};
struct Transform {
  float position[3];
  float rotation[3];
  float scale[3];
};
enum class LightType {
  Point = 0,
  Spot = 1,
  Directional = 2,
  Rect = 3
};
struct LightData {
  int type;
  float color[3];
  float intensity;
  float radius;
  float spotAngle;
  float falloff;
  bool castShadow;
  float lightSize;
  int shadowSize;
  float softness;
  int cascades;
  float lambda;
  float areaW;
  float areaH;
  int linkMask;
  int layer;
};
struct UvXformData {
  float offsetU = 0.0f;
  float offsetV = 0.0f;
  float scaleU = 1.0f;
  float scaleV = 1.0f;
  float rotation = 0.0f;
  int uvSet = 0;
};
struct MaterialData {
  float albedo[3];
  float alpha = 1.0f;
  float metallic;
  float roughness;
  std::string albedoTex;
  std::string normalTex;
  std::string ormTex;
  std::string heightTex;
  float emissive[3] = { 0.0f, 0.0f, 0.0f };
  float emissiveStrength = 1.0f;
  std::string emissiveTex;
  bool unlit = false;
  bool doubleSided = false;
  int alphaMode = 0;
  float alphaCutoff = 0.5f;
  UvXformData xforms[5];
  float ior = 1.5f;
  bool hasSpecular = false;
  float specularColor[3] = { 1.0f, 1.0f, 1.0f };
  float specularStrength = 1.0f;
  std::string specularTex;
  std::string specularColorTex;
  float clearcoat = 0.0f;
  float clearcoatRoughness = 0.0f;
  std::string clearcoatTex;
  std::string clearcoatRoughnessTex;
  std::string clearcoatNormalTex;
  float sheenColor[3] = { 0.0f, 0.0f, 0.0f };
  float sheenRoughness = 0.0f;
  std::string sheenColorTex;
  std::string sheenRoughnessTex;
  float anisotropy = 0.0f;
  float anisotropyRotation = 0.0f;
  std::string anisotropyTex;
  float iridescence = 0.0f;
  float iridescenceIor = 1.3f;
  float iridescenceThicknessMin = 100.0f;
  float iridescenceThicknessMax = 400.0f;
  std::string iridescenceTex;
  std::string iridescenceThicknessTex;
  float transmission = 0.0f;
  std::string transmissionTex;
  float thickness = 0.0f;
  std::string thicknessTex;
  float attenuationColor[3] = { 1.0f, 1.0f, 1.0f };
  float attenuationDistance = 0.0f;
  float dispersion = 0.0f;
  int linkMask = 255;
};
struct DecalData {
  float size[3];
  float life;
  std::string texGuid;
};
struct LodData {
  std::string farGuid;
  float nearDist;
  float farDist;
};
struct LightmapData {
  std::string texGuid;
  float intensity;
};
struct ProbeData {
  float size[3];
  float intensity;
};
struct PartMaterial {
  uint32_t part;
  MaterialData material;
};
struct Entity {
  EntityId id;
  std::string name;
  Transform transform;
  std::string meshGuid;
  bool hasMesh;
  LightData light;
  bool hasLight;
  MaterialData material;
  bool hasMaterial;
  int variantIndex = -1;
  std::vector<PartMaterial> partMaterials;
  DecalData decal;
  bool hasDecal;
  LodData lod;
  bool hasLod;
  LightmapData lightmap;
  bool hasLightmap;
  ProbeData probe;
  bool hasProbe;
  EntityId parent;
  std::vector<EntityId> children;
};
class Scene {
public:
  Scene();
  EntityId CreateEntity(const std::string& name);
  bool DeleteEntity(EntityId id);
  EntityId DuplicateEntity(EntityId id);
  bool RenameEntity(EntityId id, const std::string& name);
  bool SetTransform(EntityId id, const Transform& t);
  bool SetParent(EntityId child, EntityId parent);
  bool Has(EntityId id) const;
  Entity* Get(EntityId id);
  const Entity* Get(EntityId id) const;
  size_t Count() const;
  std::vector<EntityId> All() const;
  void Clear();
  bool IsDirty() const;
  void ClearDirty();
  void MarkDirty();
  OriginRebaser& Origin();
  const OriginRebaser& Origin() const;
  void RebaseWorld(double dx, double dy, double dz);
private:
  struct Slot {
    Entity entity;
    bool alive;
  };
  std::vector<Slot> slots;
  std::vector<uint32_t> freeList;
  OriginRebaser origin;
  bool dirty;
  bool IsLive(uint32_t idx, uint32_t gen) const;
  EntityId DuplicateRecursive(EntityId id, EntityId newParent);
};
void MakeIdentityTransform(Transform& t);
void MakeDefaultLight(LightData& l);
void MakeDefaultMaterial(MaterialData& m);
void MakeDefaultDecal(DecalData& d);
void MakeDefaultLod(LodData& l);
void MakeDefaultLightmap(LightmapData& l);
void MakeDefaultProbe(ProbeData& p);
void EntityForward(const Transform& t, float dir[3]);
void ComposeMatrix(const Transform& t, float m[16]);
bool InvertMatrix4(const float m[16], float invOut[16]);
}
