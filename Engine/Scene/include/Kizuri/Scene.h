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
  std::string cookieGuid;
};
struct UvXformData {
  float offsetU = 0.0f;
  float offsetV = 0.0f;
  float scaleU = 1.0f;
  float scaleV = 1.0f;
  float rotation = 0.0f;
  int uvSet = 0;
};
struct MaterialNode {
  int id = -1;
  int type = 0;
  int inA = -1;
  int inB = -1;
  int inC = -1;
  float color[3] = { 1.0f, 1.0f, 1.0f };
  float value = 0.0f;
  std::string texGuid;
  float posX = 0.0f;
  float posY = 0.0f;
};
struct MaterialGraph {
  std::vector<MaterialNode> nodes;
  int outAlbedo = -1;
  int outRoughness = -1;
  int outMetallic = -1;
  int outEmissive = -1;
  int nextId = 0;
};
bool EvaluateMaterialGraph(const MaterialGraph& graph, float outAlbedo[3], float& outRoughness, float& outMetallic, float outEmissive[3], std::string& outAlbedoTex);
void EmitMaterialGraphHlsl(const MaterialGraph& graph, std::string& outHlsl);
struct MaterialData {  float albedo[3];
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
  std::string detailAlbedoTex;
  std::string detailNormalTex;
  float detailTiling = 8.0f;
  float layerWeight = 0.0f;
  float layerAlbedo[3] = { 0.5f, 0.5f, 0.5f };
  float layerRoughness = 0.8f;
  float layerMetallic = 0.0f;
  float layerHeight = 0.5f;
  bool layerHeightBlend = false;
  float sssColor[3] = { 0.85f, 0.35f, 0.25f };
  float sssIntensity = 0.0f;
  int sssKind = 0;
  bool vtAlbedo = false;
  bool triplanar = false;
  float triplanarScale = 0.25f;
  MaterialGraph graph;
  bool useNodes = false;
};
struct RigidbodyData {
  int motionType = 0;
  float mass = 1.0f;
  int colliderType = 0;
  float extents[3] = { 0.5f, 0.5f, 0.5f };
  float friction = 0.5f;
  float restitution = 0.0f;
};
struct DecalData {  float size[3];
  float life;
  std::string texGuid;
  int matMask;
  float matRough;
  float matMetal;
};
struct PostData {
  float size[3];
  float exposure;
  int toneMap;
  float contrast;
  float saturation;
  float bloom;
};
struct FogData {
  float size[3];
  float density;
  float color[3];
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
  PostData post;
  bool hasPost;
  FogData fog;
  bool hasFog;
  RigidbodyData rigidbody;
  bool hasRigidbody;
  EntityId parent;
  std::vector<EntityId> children;
  bool isStatic = false;
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
void MakeDefaultPost(PostData& p);
void MakeDefaultFog(FogData& f);
void MakeDefaultRigidbody(RigidbodyData& r);
void EntityForward(const Transform& t, float dir[3]);
void ComposeMatrix(const Transform& t, float m[16]);
bool InvertMatrix4(const float m[16], float invOut[16]);
}
