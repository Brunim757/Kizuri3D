#include "Kizuri/GameMaterial.h"
#include <string>
namespace Kizuri {
uint32_t ResolveMaterialSlot(AssetDatabase& assets, const std::string& guid, uint32_t bit, RHITexture& out) {
  out = 0;
  if (guid.empty()) {
    return 0;
  }
  const TextureRecord* tr = assets.GetTexByGuid(guid);
  if (tr == nullptr || !tr->loaded || tr->gpu == 0) {
    return 0;
  }
  if (tr->selectedLevels <= 0 || tr->residentLevels < tr->selectedLevels) {
    return 0;
  }
  out = tr->gpu;
  return bit;
}
void FillDeferredFromEntity(AssetDatabase& assets, const MaterialData& m, DeferredMaterial& partMat) {
  partMat.linkGroup = static_cast<uint32_t>(m.linkMask < 0 ? 0 : (m.linkMask > 255 ? 255 : m.linkMask));
  std::string nodeAlbedoTex = m.albedoTex;
  float nodeAlbedo[3] = { m.albedo[0], m.albedo[1], m.albedo[2] };
  float nodeRough = m.roughness;
  float nodeMetal = m.metallic;
  float nodeEmissive[3] = { m.emissive[0], m.emissive[1], m.emissive[2] };
  if (m.useNodes && !m.graph.nodes.empty()) {
    std::string bakedTex;
    if (EvaluateMaterialGraph(m.graph, nodeAlbedo, nodeRough, nodeMetal, nodeEmissive, bakedTex)) {
      if (!bakedTex.empty()) {
        nodeAlbedoTex = bakedTex;
      }
    }
  }
  partMat.albedo[0] = nodeAlbedo[0];
  partMat.albedo[1] = nodeAlbedo[1];
  partMat.albedo[2] = nodeAlbedo[2];
  partMat.alpha = m.alpha;
  partMat.roughness = nodeRough < 0.0f ? 0.0f : (nodeRough > 1.0f ? 1.0f : nodeRough);
  partMat.metallic = nodeMetal < 0.0f ? 0.0f : (nodeMetal > 1.0f ? 1.0f : nodeMetal);
  partMat.emissive[0] = nodeEmissive[0];
  partMat.emissive[1] = nodeEmissive[1];
  partMat.emissive[2] = nodeEmissive[2];
  partMat.emissiveStrength = m.emissiveStrength;
  partMat.unlit = m.unlit;
  partMat.doubleSided = m.doubleSided;
  partMat.alphaMode = m.alphaMode;
  partMat.alphaCutoff = m.alphaCutoff;
  for (int s = 0; s < 5; ++s) {
    partMat.xforms[s].offsetU = m.xforms[s].offsetU;
    partMat.xforms[s].offsetV = m.xforms[s].offsetV;
    partMat.xforms[s].scaleU = m.xforms[s].scaleU;
    partMat.xforms[s].scaleV = m.xforms[s].scaleV;
    partMat.xforms[s].rotation = m.xforms[s].rotation;
    partMat.xforms[s].uvSet = m.xforms[s].uvSet;
  }
  partMat.ior = m.ior;
  partMat.hasSpecular = m.hasSpecular;
  partMat.specularColor[0] = m.specularColor[0];
  partMat.specularColor[1] = m.specularColor[1];
  partMat.specularColor[2] = m.specularColor[2];
  partMat.specularStrength = m.specularStrength;
  partMat.clearcoat = m.clearcoat;
  partMat.clearcoatRoughness = m.clearcoatRoughness;
  partMat.sheenColor[0] = m.sheenColor[0];
  partMat.sheenColor[1] = m.sheenColor[1];
  partMat.sheenColor[2] = m.sheenColor[2];
  partMat.sheenRoughness = m.sheenRoughness;
  partMat.anisotropy = m.anisotropy;
  partMat.anisotropyRotation = m.anisotropyRotation;
  partMat.iridescence = m.iridescence;
  partMat.iridescenceIor = m.iridescenceIor;
  partMat.iridescenceThicknessMin = m.iridescenceThicknessMin;
  partMat.iridescenceThicknessMax = m.iridescenceThicknessMax;
  partMat.transmission = m.transmission;
  partMat.thickness = m.thickness;
  partMat.attenuationColor[0] = m.attenuationColor[0];
  partMat.attenuationColor[1] = m.attenuationColor[1];
  partMat.attenuationColor[2] = m.attenuationColor[2];
  partMat.attenuationDistance = m.attenuationDistance;
  partMat.dispersion = m.dispersion;
  partMat.detailTiling = m.detailTiling;
  partMat.layerWeight = m.layerWeight;
  partMat.layerAlbedo[0] = m.layerAlbedo[0];
  partMat.layerAlbedo[1] = m.layerAlbedo[1];
  partMat.layerAlbedo[2] = m.layerAlbedo[2];
  partMat.layerRoughness = m.layerRoughness;
  partMat.layerMetallic = m.layerMetallic;
  partMat.layerHeight = m.layerHeight;
  partMat.layerHeightBlend = m.layerHeightBlend;
  partMat.sssColor[0] = m.sssColor[0];
  partMat.sssColor[1] = m.sssColor[1];
  partMat.sssColor[2] = m.sssColor[2];
  partMat.sssIntensity = m.sssIntensity;
  partMat.sssKind = m.sssKind < 0 ? 0 : (m.sssKind > 3 ? 3 : m.sssKind);
  partMat.triplanar = m.triplanar;
  partMat.triplanarScale = m.triplanarScale < 0.001f ? 0.001f : m.triplanarScale;
  uint32_t flags = 0;
  flags |= ResolveMaterialSlot(assets, nodeAlbedoTex, 1u, partMat.albedoTex);
  flags |= ResolveMaterialSlot(assets, m.normalTex, 2u, partMat.normalTex);
  flags |= ResolveMaterialSlot(assets, m.ormTex, 4u, partMat.ormTex);
  flags |= ResolveMaterialSlot(assets, m.heightTex, 8u, partMat.heightTex);
  flags |= ResolveMaterialSlot(assets, m.emissiveTex, 32u, partMat.emissiveTex);
  flags |= ResolveMaterialSlot(assets, m.specularTex, 64u, partMat.specularTex);
  flags |= ResolveMaterialSlot(assets, m.specularColorTex, 128u, partMat.specularColorTex);
  flags |= ResolveMaterialSlot(assets, m.clearcoatTex, 256u, partMat.clearcoatTex);
  flags |= ResolveMaterialSlot(assets, m.clearcoatRoughnessTex, 512u, partMat.clearcoatRoughnessTex);
  flags |= ResolveMaterialSlot(assets, m.clearcoatNormalTex, 1024u, partMat.clearcoatNormalTex);
  flags |= ResolveMaterialSlot(assets, m.sheenColorTex, 2048u, partMat.sheenColorTex);
  flags |= ResolveMaterialSlot(assets, m.sheenRoughnessTex, 4096u, partMat.sheenRoughnessTex);
  flags |= ResolveMaterialSlot(assets, m.anisotropyTex, 8192u, partMat.anisotropyTex);
  flags |= ResolveMaterialSlot(assets, m.iridescenceTex, 16384u, partMat.iridescenceTex);
  flags |= ResolveMaterialSlot(assets, m.iridescenceThicknessTex, 32768u, partMat.iridescenceThicknessTex);
  flags |= ResolveMaterialSlot(assets, m.transmissionTex, 131072u, partMat.transmissionTex);
  flags |= ResolveMaterialSlot(assets, m.thicknessTex, 262144u, partMat.thicknessTex);
  flags |= ResolveMaterialSlot(assets, m.detailAlbedoTex, 1048576u, partMat.detailAlbedoTex);
  if (!m.detailNormalTex.empty()) {
    assets.EnsureTextureKind(m.detailNormalTex, TexKind::Normal);
    flags |= ResolveMaterialSlot(assets, m.detailNormalTex, 2097152u, partMat.detailNormalTex);
  }
  if (m.layerWeight > 0.001f) {
    flags |= 4194304u;
  }
  if (m.layerHeightBlend) {
    flags |= 8388608u;
  }
  if (m.hasSpecular) {
    flags |= 65536u;
  }
  if (m.sssKind == 1) {
    flags |= 16777216u;
  } else if (m.sssKind == 2) {
    flags |= 33554432u;
  } else if (m.sssKind == 3) {
    flags |= 67108864u;
  }
  if (m.triplanar) {
    flags |= 536870912u;
  }
  partMat.vtTable = 0;
  partMat.vtTail = 0;
  partMat.vtResidentMip = 99.0f;
  partMat.vtGuid.clear();
  if (m.vtAlbedo && !nodeAlbedoTex.empty()) {
    VtBundle bundle;
    const TextureRecord* vtRec = assets.GetTexByGuid(nodeAlbedoTex);
    const TextureAssetData* vtData = (vtRec != nullptr && vtRec->loaded) ? &vtRec->data : nullptr;
    if (assets.GetVirtualBundle(nodeAlbedoTex, bundle) && bundle.valid) {
      flags |= 134217728u;
      partMat.albedoTex = bundle.physical;
      partMat.vtTable = bundle.table;
      partMat.vtTail = bundle.tail;
      partMat.vtResidentMip = bundle.residentMip;
      partMat.vtGuid = nodeAlbedoTex;
    } else if (vtData != nullptr && assets.CreateVirtualTexture(vtData, nodeAlbedoTex) >= 0 && assets.GetVirtualBundle(nodeAlbedoTex, bundle) && bundle.valid) {
      flags |= 134217728u;
      partMat.albedoTex = bundle.physical;
      partMat.vtTable = bundle.table;
      partMat.vtTail = bundle.tail;
      partMat.vtResidentMip = bundle.residentMip;
      partMat.vtGuid = nodeAlbedoTex;
    }
  }
  partMat.texFlags = flags;
}
void FillDeferredFromMesh(AssetDatabase& assets, const MeshMaterialData& mm, DeferredMaterial& partMat) {
  partMat.albedo[0] = mm.albedo[0];
  partMat.albedo[1] = mm.albedo[1];
  partMat.albedo[2] = mm.albedo[2];
  partMat.alpha = mm.alpha;
  partMat.roughness = mm.roughness;
  partMat.metallic = mm.metallic;
  partMat.emissive[0] = mm.emissive[0];
  partMat.emissive[1] = mm.emissive[1];
  partMat.emissive[2] = mm.emissive[2];
  partMat.emissiveStrength = mm.emissiveStrength;
  partMat.unlit = mm.unlit;
  partMat.doubleSided = mm.doubleSided;
  partMat.alphaMode = mm.alphaMode;
  partMat.alphaCutoff = mm.alphaCutoff;
  for (int s = 0; s < 5; ++s) {
    partMat.xforms[s].offsetU = mm.xforms[s].offsetU;
    partMat.xforms[s].offsetV = mm.xforms[s].offsetV;
    partMat.xforms[s].scaleU = mm.xforms[s].scaleU;
    partMat.xforms[s].scaleV = mm.xforms[s].scaleV;
    partMat.xforms[s].rotation = mm.xforms[s].rotation;
    partMat.xforms[s].uvSet = mm.xforms[s].uvSet;
  }
  partMat.ior = mm.ior;
  partMat.hasSpecular = mm.hasSpecular;
  partMat.specularColor[0] = mm.specularColor[0];
  partMat.specularColor[1] = mm.specularColor[1];
  partMat.specularColor[2] = mm.specularColor[2];
  partMat.specularStrength = mm.specularStrength;
  partMat.clearcoat = mm.clearcoat;
  partMat.clearcoatRoughness = mm.clearcoatRoughness;
  partMat.sheenColor[0] = mm.sheenColor[0];
  partMat.sheenColor[1] = mm.sheenColor[1];
  partMat.sheenColor[2] = mm.sheenColor[2];
  partMat.sheenRoughness = mm.sheenRoughness;
  partMat.anisotropy = mm.anisotropy;
  partMat.anisotropyRotation = mm.anisotropyRotation;
  partMat.iridescence = mm.iridescence;
  partMat.iridescenceIor = mm.iridescenceIor;
  partMat.iridescenceThicknessMin = mm.iridescenceThicknessMin;
  partMat.iridescenceThicknessMax = mm.iridescenceThicknessMax;
  partMat.transmission = mm.transmission;
  partMat.thickness = mm.thickness;
  partMat.attenuationColor[0] = mm.attenuationColor[0];
  partMat.attenuationColor[1] = mm.attenuationColor[1];
  partMat.attenuationColor[2] = mm.attenuationColor[2];
  partMat.attenuationDistance = mm.attenuationDistance;
  partMat.dispersion = mm.dispersion;
  uint32_t flags = 0;
  flags |= ResolveMaterialSlot(assets, mm.albedoTexGuid, 1u, partMat.albedoTex);
  flags |= ResolveMaterialSlot(assets, mm.normalTexGuid, 2u, partMat.normalTex);
  flags |= ResolveMaterialSlot(assets, mm.ormTexGuid, 4u, partMat.ormTex);
  flags |= ResolveMaterialSlot(assets, mm.heightTexGuid, 8u, partMat.heightTex);
  flags |= ResolveMaterialSlot(assets, mm.emissiveTexGuid, 32u, partMat.emissiveTex);
  flags |= ResolveMaterialSlot(assets, mm.specularTexGuid, 64u, partMat.specularTex);
  flags |= ResolveMaterialSlot(assets, mm.specularColorTexGuid, 128u, partMat.specularColorTex);
  flags |= ResolveMaterialSlot(assets, mm.clearcoatTexGuid, 256u, partMat.clearcoatTex);
  flags |= ResolveMaterialSlot(assets, mm.clearcoatRoughnessTexGuid, 512u, partMat.clearcoatRoughnessTex);
  flags |= ResolveMaterialSlot(assets, mm.clearcoatNormalTexGuid, 1024u, partMat.clearcoatNormalTex);
  flags |= ResolveMaterialSlot(assets, mm.sheenColorTexGuid, 2048u, partMat.sheenColorTex);
  flags |= ResolveMaterialSlot(assets, mm.sheenRoughnessTexGuid, 4096u, partMat.sheenRoughnessTex);
  flags |= ResolveMaterialSlot(assets, mm.anisotropyTexGuid, 8192u, partMat.anisotropyTex);
  flags |= ResolveMaterialSlot(assets, mm.iridescenceTexGuid, 16384u, partMat.iridescenceTex);
  flags |= ResolveMaterialSlot(assets, mm.iridescenceThicknessTexGuid, 32768u, partMat.iridescenceThicknessTex);
  flags |= ResolveMaterialSlot(assets, mm.transmissionTexGuid, 131072u, partMat.transmissionTex);
  flags |= ResolveMaterialSlot(assets, mm.thicknessTexGuid, 262144u, partMat.thicknessTex);
  if (mm.hasSpecular) {
    flags |= 65536u;
  }
  partMat.texFlags = flags;
}
}
