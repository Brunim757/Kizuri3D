#include "Kizuri/ComponentRegistry.h"
#include <cstddef>
#include <cstring>
#include <new>
namespace Kizuri {
namespace {
void ResetMesh(void* data) {
  static_cast<std::string*>(data)->clear();
}
void ResetLight(void* data) {
  MakeDefaultLight(*static_cast<LightData*>(data));
}
void ResetMaterial(void* data) {
  MakeDefaultMaterial(*static_cast<MaterialData*>(data));
}
void ResetDecal(void* data) {
  MakeDefaultDecal(*static_cast<DecalData*>(data));
}
void ResetLod(void* data) {
  MakeDefaultLod(*static_cast<LodData*>(data));
}
void ResetLightmap(void* data) {
  MakeDefaultLightmap(*static_cast<LightmapData*>(data));
}
void ResetProbe(void* data) {
  MakeDefaultProbe(*static_cast<ProbeData*>(data));
}
void ResetPost(void* data) {
  MakeDefaultPost(*static_cast<PostData*>(data));
}
void ResetFog(void* data) {
  MakeDefaultFog(*static_cast<FogData*>(data));
}
void ResetRigidbody(void* data) {
  MakeDefaultRigidbody(*static_cast<RigidbodyData*>(data));
}
void ResetAudioSource(void* data) {
  MakeDefaultAudioSource(*static_cast<AudioSourceData*>(data));
}
template <typename T>
void CtorComponent(void* data) {
  new (data) T();
}
template <typename T>
void CopyComponent(void* dst, const void* src) {
  *static_cast<T*>(dst) = *static_cast<const T*>(src);
}
template <typename T>
void DtorComponent(void* data) {
  static_cast<T*>(data)->~T();
}
bool EqualMeshData(const void* a, const void* b) {
  return *static_cast<const std::string*>(a) == *static_cast<const std::string*>(b);
}
bool EqualLightData(const void* a, const void* b) {
  return LightEqual(*static_cast<const LightData*>(a), *static_cast<const LightData*>(b));
}
bool EqualMaterialData(const void* a, const void* b) {
  return MaterialEqual(*static_cast<const MaterialData*>(a), *static_cast<const MaterialData*>(b));
}
bool EqualDecalData(const void* a, const void* b) {
  return DecalEqual(*static_cast<const DecalData*>(a), *static_cast<const DecalData*>(b));
}
bool EqualLodData(const void* a, const void* b) {
  return LodEqual(*static_cast<const LodData*>(a), *static_cast<const LodData*>(b));
}
bool EqualLightmapData(const void* a, const void* b) {
  return LightmapEqual(*static_cast<const LightmapData*>(a), *static_cast<const LightmapData*>(b));
}
bool EqualRigidbodyData(const void* a, const void* b) {
  return RigidbodyEqual(*static_cast<const RigidbodyData*>(a), *static_cast<const RigidbodyData*>(b));
}
bool EqualAudioSourceData(const void* a, const void* b) {
  return AudioSourceEqual(*static_cast<const AudioSourceData*>(a), *static_cast<const AudioSourceData*>(b));
}
bool EqualProbeData(const void* a, const void* b) {
  return ProbeEqual(*static_cast<const ProbeData*>(a), *static_cast<const ProbeData*>(b));
}
bool EqualPostData(const void* a, const void* b) {
  return PostEqual(*static_cast<const PostData*>(a), *static_cast<const PostData*>(b));
}
bool EqualFogData(const void* a, const void* b) {
  return FogEqual(*static_cast<const FogData*>(a), *static_cast<const FogData*>(b));
}
const char* const kLightTypeLabels[] = { "Point", "Spot", "Directional", "Rect" };
const int kLightTypeValues[] = { 0, 1, 2, 3 };
const char* const kShadowSizeLabels[] = { "512", "1024", "2048" };
const int kShadowSizeValues[] = { 512, 1024, 2048 };
const char* const kAlphaModeLabels[] = { "Opaque", "Mask", "Blend" };
const int kAlphaModeValues[] = { 0, 1, 2 };
const char* const kSssKindLabels[] = { "Off", "Skin", "Hair", "Eye" };
const int kSssKindValues[] = { 0, 1, 2, 3 };
const char* const kDecalMaskLabels[] = { "Off", "Albedo", "Albedo+Normal/Rough", "Full", "Full+Emissive" };
const int kDecalMaskValues[] = { 0, 1, 3, 7, 15 };
const char* const kMotionLabels[] = { "Static", "Kinematic", "Dynamic" };
const int kMotionValues[] = { 0, 1, 2 };
const char* const kColliderLabels[] = { "Box", "Sphere", "Capsule", "Mesh" };
const int kColliderValues[] = { 0, 1, 2, 3 };
const char* const kRolloffLabels[] = { "Linear", "Inverse", "Exponential" };
const int kRolloffValues[] = { 0, 1, 2 };
const char* const kToneMapLabels[] = { "ACES", "Reinhard", "AgX", "None" };
const int kToneMapValues[] = { 0, 1, 2, 3 };
const InspectorCondition kLightLocal[] = { { offsetof(LightData, type), InspectorConditionOp::MaskAny, 0x0B } };
const InspectorCondition kLightSpot[] = { { offsetof(LightData, type), InspectorConditionOp::Equals, 1 } };
const InspectorCondition kLightDirectional[] = { { offsetof(LightData, type), InspectorConditionOp::Equals, 2 } };
const InspectorCondition kLightRect[] = { { offsetof(LightData, type), InspectorConditionOp::Equals, 3 } };
const InspectorCondition kLightShadow[] = { { offsetof(LightData, castShadow), InspectorConditionOp::Equals, 1, sizeof(bool) } };
const InspectorCondition kLightShadowDirectional[] = { { offsetof(LightData, castShadow), InspectorConditionOp::Equals, 1, sizeof(bool) }, { offsetof(LightData, type), InspectorConditionOp::Equals, 2 } };
const InspectorCondition kDecalMaterial[] = { { offsetof(DecalData, matMask), InspectorConditionOp::GreaterEqual, 2 } };
const InspectorCondition kRigidbodyDynamic[] = { { offsetof(RigidbodyData, motionType), InspectorConditionOp::Equals, 2 } };
const InspectorCondition kRigidbodyBox[] = { { offsetof(RigidbodyData, colliderType), InspectorConditionOp::Equals, 0 } };
const InspectorCondition kRigidbodyRound[] = { { offsetof(RigidbodyData, colliderType), InspectorConditionOp::MaskAny, 0x06 } };
const InspectorCondition kRigidbodyCapsule[] = { { offsetof(RigidbodyData, colliderType), InspectorConditionOp::Equals, 2 } };
const InspectorCondition kAudioSpatial[] = { { offsetof(AudioSourceData, spatial), InspectorConditionOp::Equals, 1, sizeof(bool) } };
const InspectorFieldDesc kMeshFields[] = {
  { "Mesh", 0, InspectorFieldKind::Mesh }
};
const InspectorFieldDesc kLightFields[] = {
  { "Type", offsetof(LightData, type), InspectorFieldKind::Enum, 1.0f, 0.0f, 0.0f, false, kLightTypeLabels, kLightTypeValues, 4 },
  { "Link Mask", offsetof(LightData, linkMask), InspectorFieldKind::Int, 1.0f, 0.0f, 255.0f, true },
  { "Layer", offsetof(LightData, layer), InspectorFieldKind::Int, 1.0f, 0.0f, 31.0f, true },
  { "Color", offsetof(LightData, color), InspectorFieldKind::Color3 },
  { "Intensity", offsetof(LightData, intensity), InspectorFieldKind::Float, 0.05f, 0.0f, 20.0f },
  { "Radius", offsetof(LightData, radius), InspectorFieldKind::Float, 0.1f, 0.5f, 100.0f, false, nullptr, nullptr, 0, kLightLocal, 1 },
  { "Area W", offsetof(LightData, areaW), InspectorFieldKind::Float, 0.05f, 0.1f, 50.0f, false, nullptr, nullptr, 0, kLightRect, 1 },
  { "Area H", offsetof(LightData, areaH), InspectorFieldKind::Float, 0.05f, 0.1f, 50.0f, false, nullptr, nullptr, 0, kLightRect, 1 },
  { "Angle", offsetof(LightData, spotAngle), InspectorFieldKind::Float, 0.5f, 1.0f, 170.0f, false, nullptr, nullptr, 0, kLightSpot, 1 },
  { "Falloff", offsetof(LightData, falloff), InspectorFieldKind::Float, 0.01f, 0.01f, 0.5f, false, nullptr, nullptr, 0, kLightSpot, 1 },
  { "Cookie", offsetof(LightData, cookieGuid), InspectorFieldKind::Texture, 0.01f, 0.0f, 0.0f, false, nullptr, nullptr, 0, kLightSpot, 1 },
  { "Cast Shadow", offsetof(LightData, castShadow), InspectorFieldKind::Bool },
  { "Light Size", offsetof(LightData, lightSize), InspectorFieldKind::Float, 0.01f, 0.0f, 2.0f, false, nullptr, nullptr, 0, kLightShadow, 1 },
  { "Shadow Size", offsetof(LightData, shadowSize), InspectorFieldKind::Enum, 1.0f, 0.0f, 0.0f, false, kShadowSizeLabels, kShadowSizeValues, 3, kLightShadow, 1 },
  { "Softness", offsetof(LightData, softness), InspectorFieldKind::Float, 0.01f, 0.0f, 2.0f, false, nullptr, nullptr, 0, kLightShadow, 1 },
  { "Cascades", offsetof(LightData, cascades), InspectorFieldKind::Int, 1.0f, 2.0f, 4.0f, true, nullptr, nullptr, 0, kLightShadowDirectional, 2 },
  { "Lambda", offsetof(LightData, lambda), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f, false, nullptr, nullptr, 0, kLightShadowDirectional, 2 }
};
const InspectorFieldDesc kMaterialFields[] = {
  { "Albedo", offsetof(MaterialData, albedo), InspectorFieldKind::Color3 },
  { "Alpha", offsetof(MaterialData, alpha), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f },
  { "Metallic", offsetof(MaterialData, metallic), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f },
  { "Roughness", offsetof(MaterialData, roughness), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f },
  { "Albedo Texture", offsetof(MaterialData, albedoTex), InspectorFieldKind::Texture },
  { "Normal Texture", offsetof(MaterialData, normalTex), InspectorFieldKind::Texture },
  { "ORM Texture", offsetof(MaterialData, ormTex), InspectorFieldKind::Texture },
  { "Height Texture", offsetof(MaterialData, heightTex), InspectorFieldKind::Texture },
  { "Emissive", offsetof(MaterialData, emissive), InspectorFieldKind::Color3 },
  { "Emissive Strength", offsetof(MaterialData, emissiveStrength), InspectorFieldKind::Float, 0.05f, 0.0f, 100.0f },
  { "Emissive Texture", offsetof(MaterialData, emissiveTex), InspectorFieldKind::Texture },
  { "Unlit", offsetof(MaterialData, unlit), InspectorFieldKind::Bool },
  { "Double Sided", offsetof(MaterialData, doubleSided), InspectorFieldKind::Bool },
  { "Alpha Mode", offsetof(MaterialData, alphaMode), InspectorFieldKind::Enum, 1.0f, 0.0f, 0.0f, false, kAlphaModeLabels, kAlphaModeValues, 3 },
  { "Alpha Cutoff", offsetof(MaterialData, alphaCutoff), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f },
  { "IOR", offsetof(MaterialData, ior), InspectorFieldKind::Float, 0.01f, 1.0f, 3.0f },
  { "Specular", offsetof(MaterialData, hasSpecular), InspectorFieldKind::Bool },
  { "Specular Color", offsetof(MaterialData, specularColor), InspectorFieldKind::Color3 },
  { "Specular Strength", offsetof(MaterialData, specularStrength), InspectorFieldKind::Float, 0.01f, 0.0f, 10.0f },
  { "Specular Texture", offsetof(MaterialData, specularTex), InspectorFieldKind::Texture },
  { "Specular Color Texture", offsetof(MaterialData, specularColorTex), InspectorFieldKind::Texture },
  { "Clearcoat", offsetof(MaterialData, clearcoat), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f },
  { "Clearcoat Roughness", offsetof(MaterialData, clearcoatRoughness), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f },
  { "Clearcoat Texture", offsetof(MaterialData, clearcoatTex), InspectorFieldKind::Texture },
  { "Clearcoat Roughness Texture", offsetof(MaterialData, clearcoatRoughnessTex), InspectorFieldKind::Texture },
  { "Clearcoat Normal Texture", offsetof(MaterialData, clearcoatNormalTex), InspectorFieldKind::Texture },
  { "Sheen Color", offsetof(MaterialData, sheenColor), InspectorFieldKind::Color3 },
  { "Sheen Roughness", offsetof(MaterialData, sheenRoughness), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f },
  { "Sheen Color Texture", offsetof(MaterialData, sheenColorTex), InspectorFieldKind::Texture },
  { "Sheen Roughness Texture", offsetof(MaterialData, sheenRoughnessTex), InspectorFieldKind::Texture },
  { "Anisotropy", offsetof(MaterialData, anisotropy), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f },
  { "Anisotropy Rotation", offsetof(MaterialData, anisotropyRotation), InspectorFieldKind::Float, 0.01f, 0.0f, 360.0f },
  { "Anisotropy Texture", offsetof(MaterialData, anisotropyTex), InspectorFieldKind::Texture },
  { "Iridescence", offsetof(MaterialData, iridescence), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f },
  { "Iridescence IOR", offsetof(MaterialData, iridescenceIor), InspectorFieldKind::Float, 0.01f, 1.0f, 3.0f },
  { "Iridescence Thickness Min", offsetof(MaterialData, iridescenceThicknessMin), InspectorFieldKind::Float, 1.0f, 0.0f, 2000.0f },
  { "Iridescence Thickness Max", offsetof(MaterialData, iridescenceThicknessMax), InspectorFieldKind::Float, 1.0f, 0.0f, 2000.0f },
  { "Iridescence Texture", offsetof(MaterialData, iridescenceTex), InspectorFieldKind::Texture },
  { "Iridescence Thickness Texture", offsetof(MaterialData, iridescenceThicknessTex), InspectorFieldKind::Texture },
  { "Transmission", offsetof(MaterialData, transmission), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f },
  { "Transmission Texture", offsetof(MaterialData, transmissionTex), InspectorFieldKind::Texture },
  { "Thickness", offsetof(MaterialData, thickness), InspectorFieldKind::Float, 0.01f, 0.0f, 100.0f },
  { "Thickness Texture", offsetof(MaterialData, thicknessTex), InspectorFieldKind::Texture },
  { "Attenuation Color", offsetof(MaterialData, attenuationColor), InspectorFieldKind::Color3 },
  { "Attenuation Distance", offsetof(MaterialData, attenuationDistance), InspectorFieldKind::Float, 0.01f, 0.0f, 1000.0f },
  { "Dispersion", offsetof(MaterialData, dispersion), InspectorFieldKind::Float, 0.01f, 0.0f, 10.0f },
  { "Link Mask", offsetof(MaterialData, linkMask), InspectorFieldKind::Int, 1.0f, 0.0f, 255.0f, true },
  { "Detail Albedo Texture", offsetof(MaterialData, detailAlbedoTex), InspectorFieldKind::Texture },
  { "Detail Normal Texture", offsetof(MaterialData, detailNormalTex), InspectorFieldKind::Texture },
  { "Detail Tiling", offsetof(MaterialData, detailTiling), InspectorFieldKind::Float, 0.05f, 0.01f, 64.0f },
  { "Layer Weight", offsetof(MaterialData, layerWeight), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f },
  { "Layer Albedo", offsetof(MaterialData, layerAlbedo), InspectorFieldKind::Color3 },
  { "Layer Roughness", offsetof(MaterialData, layerRoughness), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f },
  { "Layer Metallic", offsetof(MaterialData, layerMetallic), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f },
  { "Layer Height", offsetof(MaterialData, layerHeight), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f },
  { "Layer Height Blend", offsetof(MaterialData, layerHeightBlend), InspectorFieldKind::Bool },
  { "SSS Color", offsetof(MaterialData, sssColor), InspectorFieldKind::Color3 },
  { "SSS Intensity", offsetof(MaterialData, sssIntensity), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f },
  { "SSS Kind", offsetof(MaterialData, sssKind), InspectorFieldKind::Enum, 1.0f, 0.0f, 0.0f, false, kSssKindLabels, kSssKindValues, 4 },
  { "Virtual Texture Albedo", offsetof(MaterialData, vtAlbedo), InspectorFieldKind::Bool },
  { "Triplanar", offsetof(MaterialData, triplanar), InspectorFieldKind::Bool },
  { "Triplanar Scale", offsetof(MaterialData, triplanarScale), InspectorFieldKind::Float, 0.01f, 0.001f, 10.0f }
};
const InspectorFieldDesc kDecalFields[] = {
  { "Decal Size", offsetof(DecalData, size), InspectorFieldKind::Vec3, 0.05f, 0.05f, 50.0f },
  { "Life", offsetof(DecalData, life), InspectorFieldKind::Float, 0.1f, 0.0f, 300.0f },
  { "Channels", offsetof(DecalData, matMask), InspectorFieldKind::Enum, 1.0f, 0.0f, 0.0f, false, kDecalMaskLabels, kDecalMaskValues, 5 },
  { "Decal Rough", offsetof(DecalData, matRough), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f, true, nullptr, nullptr, 0, kDecalMaterial, 1 },
  { "Decal Metal", offsetof(DecalData, matMetal), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f, true, nullptr, nullptr, 0, kDecalMaterial, 1 },
  { "Texture", offsetof(DecalData, texGuid), InspectorFieldKind::Texture }
};
const InspectorFieldDesc kLodFields[] = {
  { "Near Dist", offsetof(LodData, nearDist), InspectorFieldKind::Float, 0.5f, 1.0f, 500.0f },
  { "Far Dist", offsetof(LodData, farDist), InspectorFieldKind::Float, 0.5f, 2.0f, 1000.0f },
  { "Far Mesh", offsetof(LodData, farGuid), InspectorFieldKind::Mesh }
};
const InspectorFieldDesc kLightmapFields[] = {
  { "Texture", offsetof(LightmapData, texGuid), InspectorFieldKind::Texture },
  { "Lightmap Intensity", offsetof(LightmapData, intensity), InspectorFieldKind::Float, 0.05f, 0.0f, 4.0f }
};
const InspectorFieldDesc kRigidbodyFields[] = {
  { "Motion", offsetof(RigidbodyData, motionType), InspectorFieldKind::Enum, 1.0f, 0.0f, 0.0f, false, kMotionLabels, kMotionValues, 3 },
  { "Mass", offsetof(RigidbodyData, mass), InspectorFieldKind::Float, 0.1f, 0.01f, 1000.0f, false, nullptr, nullptr, 0, kRigidbodyDynamic, 1 },
  { "Collider", offsetof(RigidbodyData, colliderType), InspectorFieldKind::Enum, 1.0f, 0.0f, 0.0f, false, kColliderLabels, kColliderValues, 4 },
  { "Half Extents", offsetof(RigidbodyData, extents), InspectorFieldKind::Vec3, 0.05f, 0.01f, 100.0f, false, nullptr, nullptr, 0, kRigidbodyBox, 1 },
  { "Radius", offsetof(RigidbodyData, extents[0]), InspectorFieldKind::Float, 0.05f, 0.01f, 100.0f, false, nullptr, nullptr, 0, kRigidbodyRound, 1 },
  { "Half Height", offsetof(RigidbodyData, extents[1]), InspectorFieldKind::Float, 0.05f, 0.01f, 100.0f, false, nullptr, nullptr, 0, kRigidbodyCapsule, 1 },
  { "Friction", offsetof(RigidbodyData, friction), InspectorFieldKind::Float, 0.01f, 0.0f, 2.0f },
  { "Restitution", offsetof(RigidbodyData, restitution), InspectorFieldKind::Float, 0.01f, 0.0f, 1.0f }
};
const InspectorFieldDesc kAudioSourceFields[] = {
  { "Clip", offsetof(AudioSourceData, clipPath), InspectorFieldKind::AudioClip },
  { "Volume", offsetof(AudioSourceData, volume), InspectorFieldKind::Float, 0.01f, 0.0f, 4.0f },
  { "Spatial", offsetof(AudioSourceData, spatial), InspectorFieldKind::Bool },
  { "Min Distance", offsetof(AudioSourceData, minDistance), InspectorFieldKind::Float, 0.1f, 0.01f, 1000.0f, false, nullptr, nullptr, 0, kAudioSpatial, 1 },
  { "Max Distance", offsetof(AudioSourceData, maxDistance), InspectorFieldKind::Float, 0.1f, 0.01f, 10000.0f, false, nullptr, nullptr, 0, kAudioSpatial, 1 },
  { "Rolloff", offsetof(AudioSourceData, rolloffModel), InspectorFieldKind::Enum, 1.0f, 0.0f, 0.0f, false, kRolloffLabels, kRolloffValues, 3, kAudioSpatial, 1 },
  { "Rolloff Factor", offsetof(AudioSourceData, rolloff), InspectorFieldKind::Float, 0.05f, 0.01f, 16.0f, false, nullptr, nullptr, 0, kAudioSpatial, 1 },
  { "Loop", offsetof(AudioSourceData, loop), InspectorFieldKind::Bool },
  { "Playing", offsetof(AudioSourceData, playing), InspectorFieldKind::Bool },
  { "Bus", offsetof(AudioSourceData, bus), InspectorFieldKind::AudioBus }
};
const InspectorFieldDesc kProbeFields[] = {
  { "Probe Size", offsetof(ProbeData, size), InspectorFieldKind::Vec3, 0.1f, 0.5f, 100.0f },
  { "Probe Intensity", offsetof(ProbeData, intensity), InspectorFieldKind::Float, 0.05f, 0.0f, 4.0f }
};
const InspectorFieldDesc kPostFields[] = {
  { "Volume Size", offsetof(PostData, size), InspectorFieldKind::Vec3, 0.1f, 0.5f, 200.0f },
  { "Exposure", offsetof(PostData, exposure), InspectorFieldKind::Float, 0.01f, 0.05f, 8.0f },
  { "Tone Mapper", offsetof(PostData, toneMap), InspectorFieldKind::Enum, 1.0f, 0.0f, 0.0f, false, kToneMapLabels, kToneMapValues, 4 },
  { "Contrast", offsetof(PostData, contrast), InspectorFieldKind::Float, 0.01f, 0.2f, 2.0f },
  { "Saturation", offsetof(PostData, saturation), InspectorFieldKind::Float, 0.01f, 0.0f, 3.0f },
  { "Bloom", offsetof(PostData, bloom), InspectorFieldKind::Float, 0.01f, 0.0f, 2.0f }
};
const InspectorFieldDesc kFogFields[] = {
  { "Volume Size", offsetof(FogData, size), InspectorFieldKind::Vec3, 0.1f, 0.5f, 200.0f },
  { "Density", offsetof(FogData, density), InspectorFieldKind::Float, 0.001f, 0.0f, 1.0f },
  { "Fog Color", offsetof(FogData, color), InspectorFieldKind::Color3 }
};
template <typename T>
void RegisterComponent(ComponentRegistry& reg, ComponentTypeId id, const char* name, size_t dataOffset, size_t hasOffset, ComponentResetFn reset, const InspectorFieldDesc* fields, size_t fieldCount, ComponentEqualFn equal) {
  ComponentTypeDesc desc;
  desc.id = id;
  desc.name = name;
  desc.dataOffset = dataOffset;
  desc.dataSize = sizeof(T);
  desc.hasOffset = hasOffset;
  desc.reset = reset;
  desc.ctor = &CtorComponent<T>;
  desc.copy = &CopyComponent<T>;
  desc.dtor = &DtorComponent<T>;
  desc.equal = equal;
  desc.fields = fields;
  desc.fieldCount = fieldCount;
  reg.Register(desc);
}
void RegisterAll(ComponentRegistry& reg) {
  if (reg.Count() > 0) {
    return;
  }
  RegisterComponent<std::string>(reg, ComponentRegistry::kMesh, "Mesh Renderer", offsetof(Entity, meshGuid), offsetof(Entity, hasMesh), &ResetMesh, kMeshFields, sizeof(kMeshFields) / sizeof(kMeshFields[0]), &EqualMeshData);
  RegisterComponent<LightData>(reg, ComponentRegistry::kLight, "Light", offsetof(Entity, light), offsetof(Entity, hasLight), &ResetLight, kLightFields, sizeof(kLightFields) / sizeof(kLightFields[0]), &EqualLightData);
  RegisterComponent<MaterialData>(reg, ComponentRegistry::kMaterial, "Material", offsetof(Entity, material), offsetof(Entity, hasMaterial), &ResetMaterial, kMaterialFields, sizeof(kMaterialFields) / sizeof(kMaterialFields[0]), &EqualMaterialData);
  RegisterComponent<DecalData>(reg, ComponentRegistry::kDecal, "Decal", offsetof(Entity, decal), offsetof(Entity, hasDecal), &ResetDecal, kDecalFields, sizeof(kDecalFields) / sizeof(kDecalFields[0]), &EqualDecalData);
  RegisterComponent<LodData>(reg, ComponentRegistry::kLod, "LOD", offsetof(Entity, lod), offsetof(Entity, hasLod), &ResetLod, kLodFields, sizeof(kLodFields) / sizeof(kLodFields[0]), &EqualLodData);
  RegisterComponent<LightmapData>(reg, ComponentRegistry::kLightmap, "Lightmap", offsetof(Entity, lightmap), offsetof(Entity, hasLightmap), &ResetLightmap, kLightmapFields, sizeof(kLightmapFields) / sizeof(kLightmapFields[0]), &EqualLightmapData);
  RegisterComponent<RigidbodyData>(reg, ComponentRegistry::kRigidbody, "Rigidbody", offsetof(Entity, rigidbody), offsetof(Entity, hasRigidbody), &ResetRigidbody, kRigidbodyFields, sizeof(kRigidbodyFields) / sizeof(kRigidbodyFields[0]), &EqualRigidbodyData);
  RegisterComponent<AudioSourceData>(reg, ComponentRegistry::kAudioSource, "Audio Source", offsetof(Entity, audioSource), offsetof(Entity, hasAudioSource), &ResetAudioSource, kAudioSourceFields, sizeof(kAudioSourceFields) / sizeof(kAudioSourceFields[0]), &EqualAudioSourceData);
  RegisterComponent<ProbeData>(reg, ComponentRegistry::kProbe, "Reflection Probe", offsetof(Entity, probe), offsetof(Entity, hasProbe), &ResetProbe, kProbeFields, sizeof(kProbeFields) / sizeof(kProbeFields[0]), &EqualProbeData);
  RegisterComponent<PostData>(reg, ComponentRegistry::kPost, "Post Volume", offsetof(Entity, post), offsetof(Entity, hasPost), &ResetPost, kPostFields, sizeof(kPostFields) / sizeof(kPostFields[0]), &EqualPostData);
  RegisterComponent<FogData>(reg, ComponentRegistry::kFog, "Fog Volume", offsetof(Entity, fog), offsetof(Entity, hasFog), &ResetFog, kFogFields, sizeof(kFogFields) / sizeof(kFogFields[0]), &EqualFogData);
}
}
ComponentRegistry& ComponentRegistry::Instance() {
  static ComponentRegistry instance;
  RegisterAll(instance);
  return instance;
}
void ComponentRegistry::Register(const ComponentTypeDesc& desc) {
  if (types.size() <= desc.id) {
    types.resize(desc.id + 1);
  }
  types[desc.id] = desc;
}
void ComponentRegistry::SetSerializer(ComponentTypeId id, ComponentWriteFn write, ComponentReadFn read) {
  if (id >= types.size()) {
    return;
  }
  types[id].write = write;
  types[id].read = read;
}
const ComponentTypeDesc* ComponentRegistry::Get(ComponentTypeId id) const {
  if (id >= types.size() || types[id].reset == nullptr) {
    return nullptr;
  }
  return &types[id];
}
const ComponentTypeDesc* ComponentRegistry::FindByName(const std::string& name) const {
  for (size_t i = 0; i < types.size(); ++i) {
    if (types[i].reset != nullptr && name == types[i].name) {
      return &types[i];
    }
  }
  return nullptr;
}
ComponentTypeId FindComponentType(const char* name) {
  if (name == nullptr) {
    return 0xFFFFFFFFu;
  }
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().FindByName(name);
  return desc != nullptr ? desc->id : 0xFFFFFFFFu;
}
bool EntityHasComponent(const Entity* e, ComponentTypeId id) {
  if (e == nullptr) {
    return false;
  }
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(id);
  if (desc == nullptr) {
    return false;
  }
  return *reinterpret_cast<const bool*>(reinterpret_cast<const unsigned char*>(e) + desc->hasOffset);
}
const void* EntityComponentData(const Entity* e, ComponentTypeId id) {
  if (e == nullptr) {
    return nullptr;
  }
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(id);
  if (desc == nullptr) {
    return nullptr;
  }
  return reinterpret_cast<const unsigned char*>(e) + desc->dataOffset;
}
void* EntityComponentData(Entity* e, ComponentTypeId id) {
  return const_cast<void*>(EntityComponentData(static_cast<const Entity*>(e), id));
}
void ResetEntityComponent(Entity* e, ComponentTypeId id) {
  void* data = EntityComponentData(e, id);
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(id);
  if (data != nullptr && desc != nullptr && desc->reset != nullptr) {
    desc->reset(data);
  }
}
bool AddEntityComponent(Entity* e, ComponentTypeId id) {
  if (e == nullptr) {
    return false;
  }
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(id);
  if (desc == nullptr) {
    return false;
  }
  bool* has = reinterpret_cast<bool*>(reinterpret_cast<unsigned char*>(e) + desc->hasOffset);
  if (*has) {
    return false;
  }
  ResetEntityComponent(e, id);
  *has = true;
  return true;
}
bool RemoveEntityComponent(Entity* e, ComponentTypeId id) {
  if (e == nullptr) {
    return false;
  }
  const ComponentTypeDesc* desc = ComponentRegistry::Instance().Get(id);
  if (desc == nullptr) {
    return false;
  }
  bool* has = reinterpret_cast<bool*>(reinterpret_cast<unsigned char*>(e) + desc->hasOffset);
  if (!*has) {
    return false;
  }
  *has = false;
  return true;
}
namespace {
bool Float3Equal(const float a[3], const float b[3]) {
  return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}
bool UvXformEqual(const UvXformData& a, const UvXformData& b) {
  return a.offsetU == b.offsetU && a.offsetV == b.offsetV && a.scaleU == b.scaleU && a.scaleV == b.scaleV && a.rotation == b.rotation && a.uvSet == b.uvSet;
}
bool MaterialNodeEqual(const MaterialNode& a, const MaterialNode& b) {
  return a.id == b.id && a.type == b.type && a.inA == b.inA && a.inB == b.inB && a.inC == b.inC && Float3Equal(a.color, b.color) && a.value == b.value && a.texGuid == b.texGuid && a.posX == b.posX && a.posY == b.posY;
}
bool MaterialGraphEqual(const MaterialGraph& a, const MaterialGraph& b) {
  if (a.outAlbedo != b.outAlbedo || a.outRoughness != b.outRoughness || a.outMetallic != b.outMetallic || a.outEmissive != b.outEmissive || a.nextId != b.nextId) {
    return false;
  }
  if (a.nodes.size() != b.nodes.size()) {
    return false;
  }
  for (size_t i = 0; i < a.nodes.size(); ++i) {
    if (!MaterialNodeEqual(a.nodes[i], b.nodes[i])) {
      return false;
    }
  }
  return true;
}
}
bool MaterialEqual(const MaterialData& a, const MaterialData& b) {
  if (!Float3Equal(a.albedo, b.albedo) || a.alpha != b.alpha || a.metallic != b.metallic || a.roughness != b.roughness) {
    return false;
  }
  if (a.albedoTex != b.albedoTex || a.normalTex != b.normalTex || a.ormTex != b.ormTex || a.heightTex != b.heightTex) {
    return false;
  }
  if (!Float3Equal(a.emissive, b.emissive) || a.emissiveStrength != b.emissiveStrength || a.emissiveTex != b.emissiveTex || a.unlit != b.unlit || a.doubleSided != b.doubleSided || a.alphaMode != b.alphaMode || a.alphaCutoff != b.alphaCutoff) {
    return false;
  }
  for (int s = 0; s < 5; ++s) {
    if (!UvXformEqual(a.xforms[s], b.xforms[s])) {
      return false;
    }
  }
  if (a.ior != b.ior || a.hasSpecular != b.hasSpecular || !Float3Equal(a.specularColor, b.specularColor) || a.specularStrength != b.specularStrength || a.specularTex != b.specularTex || a.specularColorTex != b.specularColorTex) {
    return false;
  }
  if (a.clearcoat != b.clearcoat || a.clearcoatRoughness != b.clearcoatRoughness || a.clearcoatTex != b.clearcoatTex || a.clearcoatRoughnessTex != b.clearcoatRoughnessTex || a.clearcoatNormalTex != b.clearcoatNormalTex) {
    return false;
  }
  if (!Float3Equal(a.sheenColor, b.sheenColor) || a.sheenRoughness != b.sheenRoughness || a.sheenColorTex != b.sheenColorTex || a.sheenRoughnessTex != b.sheenRoughnessTex) {
    return false;
  }
  if (a.anisotropy != b.anisotropy || a.anisotropyRotation != b.anisotropyRotation || a.anisotropyTex != b.anisotropyTex) {
    return false;
  }
  if (a.iridescence != b.iridescence || a.iridescenceIor != b.iridescenceIor || a.iridescenceThicknessMin != b.iridescenceThicknessMin || a.iridescenceThicknessMax != b.iridescenceThicknessMax || a.iridescenceTex != b.iridescenceTex || a.iridescenceThicknessTex != b.iridescenceThicknessTex) {
    return false;
  }
  if (a.transmission != b.transmission || a.transmissionTex != b.transmissionTex || a.thickness != b.thickness || a.thicknessTex != b.thicknessTex) {
    return false;
  }
  if (!Float3Equal(a.attenuationColor, b.attenuationColor) || a.attenuationDistance != b.attenuationDistance || a.dispersion != b.dispersion) {
    return false;
  }
  if (a.linkMask != b.linkMask || a.detailAlbedoTex != b.detailAlbedoTex || a.detailNormalTex != b.detailNormalTex || a.detailTiling != b.detailTiling) {
    return false;
  }
  if (a.layerWeight != b.layerWeight || !Float3Equal(a.layerAlbedo, b.layerAlbedo) || a.layerRoughness != b.layerRoughness || a.layerMetallic != b.layerMetallic || a.layerHeight != b.layerHeight || a.layerHeightBlend != b.layerHeightBlend) {
    return false;
  }
  if (!Float3Equal(a.sssColor, b.sssColor) || a.sssIntensity != b.sssIntensity || a.sssKind != b.sssKind || a.vtAlbedo != b.vtAlbedo || a.triplanar != b.triplanar || a.triplanarScale != b.triplanarScale) {
    return false;
  }
  if (a.useNodes != b.useNodes || !MaterialGraphEqual(a.graph, b.graph)) {
    return false;
  }
  return true;
}
bool LightEqual(const LightData& a, const LightData& b) {
  return a.type == b.type && Float3Equal(a.color, b.color) && a.intensity == b.intensity && a.radius == b.radius && a.spotAngle == b.spotAngle && a.falloff == b.falloff && a.castShadow == b.castShadow && a.lightSize == b.lightSize && a.shadowSize == b.shadowSize && a.softness == b.softness && a.cascades == b.cascades && a.lambda == b.lambda && a.areaW == b.areaW && a.areaH == b.areaH && a.linkMask == b.linkMask && a.layer == b.layer && a.cookieGuid == b.cookieGuid;
}
bool DecalEqual(const DecalData& a, const DecalData& b) {
  return Float3Equal(a.size, b.size) && a.life == b.life && a.texGuid == b.texGuid && a.matMask == b.matMask && a.matRough == b.matRough && a.matMetal == b.matMetal;
}
bool LodEqual(const LodData& a, const LodData& b) {
  return a.farGuid == b.farGuid && a.nearDist == b.nearDist && a.farDist == b.farDist;
}
bool RigidbodyEqual(const RigidbodyData& a, const RigidbodyData& b) {
  return a.motionType == b.motionType && a.mass == b.mass && a.colliderType == b.colliderType && Float3Equal(a.extents, b.extents) && a.friction == b.friction && a.restitution == b.restitution;
}
bool AudioSourceEqual(const AudioSourceData& a, const AudioSourceData& b) {
  return a.clipPath == b.clipPath && a.volume == b.volume && a.minDistance == b.minDistance && a.maxDistance == b.maxDistance && a.rolloff == b.rolloff && a.rolloffModel == b.rolloffModel && a.loop == b.loop && a.spatial == b.spatial && a.playing == b.playing && a.bus == b.bus;
}
bool LightmapEqual(const LightmapData& a, const LightmapData& b) {
  return a.texGuid == b.texGuid && a.intensity == b.intensity;
}
bool ProbeEqual(const ProbeData& a, const ProbeData& b) {
  return Float3Equal(a.size, b.size) && a.intensity == b.intensity;
}
bool PostEqual(const PostData& a, const PostData& b) {
  return Float3Equal(a.size, b.size) && a.exposure == b.exposure && a.toneMap == b.toneMap && a.contrast == b.contrast && a.saturation == b.saturation && a.bloom == b.bloom;
}
bool FogEqual(const FogData& a, const FogData& b) {
  return Float3Equal(a.size, b.size) && a.density == b.density && Float3Equal(a.color, b.color);
}
}
