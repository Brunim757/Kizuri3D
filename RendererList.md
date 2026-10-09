Kizuri Engine — Renderer Roadmap

Requisitos e objetivo

- API principal: DirectX 11
- Requisito mínimo alvo: NVIDIA GTX 1060
- Renderer poderoso sem depender de Ray Tracing
- Recursos devem possuir níveis de qualidade configuráveis
- Prioridade: qualidade visual + desempenho + escalabilidade

---

1. Material / PBR

Já implementado

- [x] Albedo / Base Color
- [x] Metallic
- [x] Roughness
- [x] Normal Mapping
- [x] Ambient Occlusion
- [x] Height Mapping
- [x] Emissive
- [x] Emissive Strength
- [x] Unlit
- [x] Double Sided
- [x] Alpha Mask
- [x] Alpha Blend
- [x] IOR
- [x] Specular
- [x] Clearcoat
- [x] Sheen
- [x] Transmission
- [x] Volume
- [x] Anisotropy
- [x] Iridescence
- [x] Dispersion
- [x] Specular-Glossiness
- [x] Texture Transform
- [x] Material Variants

Futuro

- [ ] Layered Materials
- [ ] Material blending avançado
- [ ] Decal materials
- [ ] Detail maps
- [ ] Detail normal maps
- [x] Parallax Occlusion Mapping
- [ ] Tessellation/displacement quando aplicável
- [ ] Material instances
- [ ] Material inheritance
- [x] Runtime material overrides

---

2. Image Based Lighting

- [x] IBL completo
- [x] Environment maps
- [x] HDR environment maps
- [x] Irradiance maps
- [x] Prefiltered environment maps
- [x] BRDF LUT
- [x] Image-based diffuse lighting
- [x] Image-based specular lighting
- [x] Reflection probes
- [ ] Reflection probe blending
- [ ] Reflection probe volumes
- [x] Box-projected reflection probes
- [ ] Probe prioritization
- [x] Probe baking
- [ ] Probe streaming

---

3. Iluminação

- [x] Directional lights avançadas
- [x] Point lights
- [x] Spot lights
- [ ] Area lights
- [ ] Rectangular area lights
- [ ] Light attenuation avançada
- [ ] Physically based light units
- [x] Temperature / Kelvin lights
- [ ] Light layers
- [ ] Light cookies
- [ ] Light linking
- [ ] Clustered lighting
- [ ] Forward+ lighting
- [ ] GPU light culling
- [x] Large numbers of dynamic lights

---

4. Sombras

- [x] Shadow maps
- [x] Cascaded Shadow Maps
- [x] PCF
- [x] PCSS
- [x] VSM
- [x] EVSM
- [x] Contact shadows
- [x] Soft shadows
- [x] Shadow bias automático
- [x] Normal bias
- [x] Shadow atlas
- [x] Shadow caching
- [x] Shadow resolution dinâmica
- [x] Cascades configuráveis
- [x] Distance-based shadow quality

---

5. Screen Space

- [x] Screen Space Ambient Occlusion
- [ ] GTAO
- [ ] HBAO-like techniques
- [x] Screen Space Reflections
- [ ] SSR temporal
- [x] Screen Space Contact Shadows
- [x] Screen Space Global Illumination
- [ ] Screen Space Bent Normals
- [ ] Screen Space Decals
- [ ] Depth-aware effects

---

6. Global Illumination

Sem Ray Tracing

- [x] SSGI
- [x] Light probes
- [ ] Irradiance volumes
- [ ] Probe grids
- [ ] Probe blending
- [x] Baked global illumination
- [x] Lightmap system
- [ ] Lightmap denoising
- [ ] Lightmap compression
- [x] Mixed lighting
- [x] Dynamic/baked lighting combination
- [ ] Ambient probe
- [x] Reflection probe integration

---

7. Volumétricos

- [x] Volumetric fog
- [x] Height fog
- [x] Exponential fog
- [x] Volumetric lighting
- [x] Light shafts
- [x] God rays
- [ ] Volumetric shadows
- [ ] Local volumetric fog
- [ ] Fog volumes
- [ ] Height-based atmospheric effects

---

8. Atmosfera

- [x] Sky rendering
- [x] HDR sky
- [x] Procedural sky
- [x] Sun disk
- [x] Moon
- [x] Atmospheric scattering
- [x] Rayleigh scattering
- [x] Mie scattering
- [x] Horizon effects
- [x] Sun/moon atmospheric integration
- [x] Sky irradiance
- [x] Sky reflection

---

9. Post Processing

- [x] HDR pipeline
- [x] Exposure
- [x] Auto Exposure
- [ ] Histogram exposure
- [x] Tone Mapping
- [x] ACES
- [x] AgX
- [x] Reinhard
- [x] Bloom
- [ ] Lens dirt
- [x] Vignette
- [x] Color grading
- [x] LUT color grading
- [x] Contrast
- [x] Saturation
- [x] Temperature
- [x] Tint
- [x] Chromatic aberration
- [x] Film grain
- [x] Sharpening
- [x] FSR upscaling
- [x] Depth of Field
- [x] Motion Blur
- [ ] Screen-space distortion

---

10. Anti-Aliasing

- [x] FXAA
- [x] TAA
- [x] Temporal jitter
- [x] Motion vectors
- [x] History buffers
- [ ] TAA sharpening
- [x] TAA ghosting reduction
- [x] Temporal reconstruction

---

11. Geometry / Rendering

- [x] Frustum culling
- [x] Occlusion culling
- [ ] Hierarchical Z-buffer
- [ ] GPU culling
- [ ] GPU-driven rendering
- [ ] Indirect drawing
- [ ] Multi-draw indirect quando aplicável
- [x] Instanced rendering
- [x] GPU instancing
- [x] Dynamic batching quando vantajoso
- [ ] Static batching quando vantajoso
- [x] Mesh LOD
- [x] Automatic LOD
- [x] LOD groups
- [x] LOD streaming
- [ ] Mesh streaming
- [ ] Geometry streaming
- [ ] Meshlet-like organization compatível com DX11
- [x] Render queues
- [x] Sorting otimizado

---

12. Texturas

- [x] Texture streaming
- [x] Mipmaps
- [x] Mipmap streaming
- [x] Anisotropic filtering
- [ ] Virtual texturing
- [ ] Sparse-like virtual texture system
- [x] Texture compression
- [x] BC1
- [x] BC3
- [ ] BC4
- [x] BC5
- [ ] BC6H
- [ ] BC7
- [x] HDR textures
- [ ] Texture arrays
- [ ] Texture atlases
- [ ] Runtime texture generation
- [x] Async texture upload
- [x] Texture residency management

---

13. Decals

- [x] Basic decals
- [x] Deferred decals
- [ ] Screen-space decals
- [ ] Normal decals
- [ ] Roughness decals
- [ ] Metallic decals
- [ ] Emissive decals
- [x] Decal blending
- [x] Decal projection
- [ ] Decal sorting

---

14. Subsurface / Especializados

- [x] Subsurface scattering
- [ ] Screen-space SSS
- [ ] Separable SSS
- [ ] Skin material
- [ ] Hair/fiber shading
- [ ] Anisotropic hair
- [ ] Eye shader
- [ ] Cornea
- [ ] Subsurface materials
- [x] Cloth/fabric shading

---

15. Rendering Architecture

- [x] G-Buffer otimizado
- [x] Material ID system
- [x] Material table
- [x] Forward pass para transmissivos
- [ ] Render graph
- [ ] Resource lifetime tracking
- [ ] Transient render targets
- [x] Render target pooling
- [x] Async resource loading
- [ ] GPU synchronization
- [ ] CPU/GPU frame pacing
- [ ] Multi-threaded command preparation
- [ ] Render thread
- [ ] Worker threads
- [x] GPU profiling
- [x] CPU profiling

---

16. Performance

- [x] GPU timing
- [x] CPU timing
- [x] Per-pass GPU timings
- [x] Draw call statistics
- [x] Triangle statistics
- [x] Texture memory statistics
- [x] VRAM statistics
- [x] Render target statistics
- [ ] Shader permutation statistics
- [x] Dynamic resolution
- [x] Resolution scaling
- [x] Quality presets
- [x] Low
- [x] Medium
- [x] High
- [x] Ultra
- [ ] Custom
- [x] Automatic quality scaling

---

17. Editor / Artist Workflow

- [x] Material Inspector completo
- [x] Mesh materials read-only
- [ ] Material preview
- [ ] HDR environment preview
- [ ] Reflection probe preview
- [ ] Light preview
- [ ] Shadow preview
- [ ] Texture preview
- [ ] Shader preview
- [ ] Post-processing volume editor
- [ ] LOD editor
- [ ] Decal editor
- [ ] Lightmap visualization
- [ ] UV visualization
- [ ] Normal visualization
- [ ] Roughness visualization
- [ ] Metallic visualization
- [ ] Depth visualization
- [ ] Overdraw visualization
- [x] GPU performance overlay
- [ ] RenderDoc-friendly debugging

---

18. Debug / Development

- [ ] Wireframe mode
- [ ] Unlit view
- [ ] Albedo-only view
- [ ] Normal-only view
- [ ] Roughness-only view
- [ ] Metallic-only view
- [ ] AO-only view
- [ ] Emissive-only view
- [ ] Depth view
- [ ] Motion vector view
- [ ] Shadow map view
- [ ] Reflection probe view
- [ ] Lightmap view
- [ ] Bounding box view
- [ ] LOD visualization
- [ ] Overdraw visualization
- [ ] Material ID visualization
- [x] GPU timing overlay
- [x] Render statistics
- [ ] Shader debugging

---

19. Asset Pipeline

- [x] glTF import
- [x] KZMesh
- [x] KZTex
- [x] Meshopt decoding
- [x] Draco decoding
- [x] BasisU decoding
- [x] WebP decoding
- [x] Texture transform
- [x] Material variants
- [x] glTF lights
- [x] GPU instancing
- [x] Asset dependency graph
- [ ] Incremental importing
- [x] Import cache
- [x] Background importing
- [x] Parallel import (textures + geometry)
- [x] Asset hot reload
- [ ] Asset versioning
- [ ] Asset validation
- [ ] Shader compilation cache
- [ ] Texture processing cache
- [ ] Mesh processing cache

---

20. Futuro / Experimental

- [ ] Advanced temporal reconstruction
- [ ] Advanced volumetric reconstruction
- [ ] Advanced GI sem Ray Tracing
- [ ] Probe-based dynamic GI
- [ ] Irradiance fields
- [ ] DDGI-like techniques compatíveis com o renderer
- [ ] Advanced virtual texturing
- [ ] GPU scene management
- [ ] GPU occlusion culling
- [ ] Advanced LOD selection
- [ ] Procedural material systems
- [ ] Node-based material editor
- [ ] Shader graph
- [ ] Compute-based post processing

---

Fora do objetivo atual

- [ ] Ray-traced reflections
- [ ] Ray-traced shadows
- [ ] Path tracing
- [ ] Hardware RT obrigatório

O renderer deve continuar DX11-first e ter a GTX 1060 como requisito mínimo alvo.

---

Objetivo final

A Kizuri deve possuir um renderer:

Poderoso + visualmente avançado + escalável + compatível com hardware relativamente antigo.

A qualidade máxima não deve exigir hardware moderno para simplesmente funcionar, enquanto usuários com GPUs mais fortes podem aumentar resolução, samples, sombras, volumétricos, SSR e demais efeitos.

Prioridade máxima:

1. Qualidade visual
2. Performance
3. Estabilidade
4. Escalabilidade
5. Ferramentas para artistas
6. Compatibilidade