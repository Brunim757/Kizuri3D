#pragma once
#include <stdint.h>
namespace Kizuri {
enum class RHI_API {
  Null,
  D3D11
};
enum class RHIFormat {
  RGBA8_UNORM,
  RGBA16F,
  D24S8,
  R32_DEPTH
};
enum class RHITopology {
  TriangleList
};
enum class RHICull {
  None,
  Back,
  Front
};
enum class RHIFill {
  Solid,
  Wireframe
};
enum class RHITextureFormat {
  RGBA8_UNORM,
  RGBA8_UNORM_SRGB,
  RGBA16F,
  BC1_UNORM,
  BC1_UNORM_SRGB,
  BC3_UNORM,
  BC3_UNORM_SRGB,
  BC5_UNORM
};
using RHIBuffer = uint64_t;
using RHIConstBuffer = uint64_t;
using RHIVertexShader = uint64_t;
using RHIPixelShader = uint64_t;
using RHIInputLayout = uint64_t;
using RHIRenderTarget = uint64_t;
using RHISampler = uint64_t;
using RHITexture = uint64_t;
using RHITimestamp = uint64_t;
using RHIOcclusionQuery = uint64_t;
using RHITexture3D = uint64_t;
struct RHIDesc {
  void* windowHandle;
  int width;
  int height;
  bool vsync;
};
struct RHIRasterizer {
  RHICull cull;
  RHIFill fill;
  bool frontCCW;
  float slopeBias;
};
struct RHIDepthStencil {
  bool depthEnable;
  bool depthWrite;
};
struct RHIBlend {
  bool enable;
};
struct RHIViewport {
  float x;
  float y;
  float w;
  float h;
  float minD;
  float maxD;
};
class IRHI {
public:
  virtual ~IRHI() = default;
  virtual bool Initialize(const RHIDesc& desc) = 0;
  virtual void Shutdown() = 0;
  virtual bool Resize(int w, int h) = 0;
  virtual void Clear(float r, float g, float b, float a) = 0;
  virtual void Present(bool vsync) = 0;
  virtual const char* BackendName() const = 0;
  virtual int Width() const = 0;
  virtual int Height() const = 0;
  virtual void* GetNativeDevice() const = 0;
  virtual void* GetNativeContext() const = 0;
  virtual void GetCacheStats(uint64_t& total, uint64_t& discarded) const = 0;
  virtual RHIBuffer CreateBuffer(uint64_t size, uint32_t stride, bool isIndex, const void* initialData) = 0;
  virtual RHIBuffer CreateBufferEmpty(uint64_t size, uint32_t stride, bool isIndex) = 0;
  virtual bool UpdateBufferRange(RHIBuffer buf, uint64_t offset, const void* data, size_t bytes) = 0;
  virtual bool UpdateBufferDiscard(RHIBuffer buf, const void* data, size_t bytes) = 0;
  virtual void DestroyBuffer(RHIBuffer buf) = 0;
  virtual void SetVertexBuffer(RHIBuffer buf, uint32_t offset) = 0;
  virtual void SetIndexBuffer(RHIBuffer buf) = 0;
  virtual RHIConstBuffer CreateConstantBuffer(uint64_t size, const void* initialData) = 0;
  virtual void UpdateConstantBuffer(RHIConstBuffer buf, const void* data, uint64_t size) = 0;
  virtual void DestroyConstantBuffer(RHIConstBuffer buf) = 0;
  virtual void SetVertexConstantBuffer(uint32_t slot, RHIConstBuffer buf) = 0;
  virtual void SetPixelConstantBuffer(uint32_t slot, RHIConstBuffer buf) = 0;
  virtual RHIVertexShader CreateVertexShaderFromFile(const char* path, const char* entry) = 0;
  virtual RHIPixelShader CreatePixelShaderFromFile(const char* path, const char* entry) = 0;
  virtual void SetVertexShader(RHIVertexShader vs) = 0;
  virtual void SetPixelShader(RHIPixelShader ps) = 0;
  virtual RHIInputLayout CreateInputLayoutPNUT(RHIVertexShader vs) = 0;
  virtual RHIInputLayout CreateInputLayoutPNUTInstanced(RHIVertexShader vs) = 0;
  virtual void SetInstanceBuffer(RHIBuffer buf) = 0;
  virtual void SetInputLayout(RHIInputLayout layout) = 0;
  virtual void SetViewport(const RHIViewport& vp) = 0;
  virtual void SetTopology(RHITopology topo) = 0;
  virtual void SetRasterizerState(const RHIRasterizer& rs) = 0;
  virtual void SetDepthStencilState(const RHIDepthStencil& ds) = 0;
  virtual void SetBlendState(const RHIBlend& blend) = 0;
  virtual RHIRenderTarget CreateRenderTarget(int w, int h, RHIFormat fmt) = 0;
  virtual void DestroyRenderTarget(RHIRenderTarget rt) = 0;
  virtual void SetRenderTargets(uint32_t count, const RHIRenderTarget* colorRTs, RHIRenderTarget depthRT) = 0;
  virtual void ClearRenderTarget(RHIRenderTarget rt, float r, float g, float b, float a) = 0;
  virtual void ClearDepth(RHIRenderTarget depthRT) = 0;
  virtual void* GetRenderTargetSRV(RHIRenderTarget rt) const = 0;
  virtual void BindBackbuffer() = 0;
  virtual void SetPixelTexture(uint32_t slot, RHIRenderTarget rt) = 0;
  virtual void SetPixelTexture2D(uint32_t slot, RHITexture tex) = 0;
  virtual void SetPixelTexture3D(uint32_t slot, RHITexture3D tex) = 0;
  virtual RHISampler CreateSamplerLinear() = 0;
  virtual RHISampler CreateSamplerWrap() = 0;
  virtual void SetPixelSampler(uint32_t slot, RHISampler sampler) = 0;
  virtual RHIRenderTarget CreateShadowCube(int size) = 0;
  virtual void SetShadowCubeFace(RHIRenderTarget cube, int face) = 0;
  virtual RHISampler CreateSamplerShadow() = 0;
  virtual RHITexture CreateTexture2D(int w, int h, int mipLevels, RHITextureFormat fmt) = 0;
  virtual bool UpdateTextureMip(RHITexture tex, int mip, int w, int h, uint32_t rowPitch, const void* data, size_t bytes) = 0;
  virtual void DestroyTexture(RHITexture tex) = 0;
  virtual int TextureResidentMips(RHITexture tex) const = 0;
  virtual RHITexture CreateEnvCube(int size) = 0;
  virtual RHIRenderTarget GetEnvCubeFace(RHITexture cube, int face) = 0;
  virtual void GenerateCubeMips(RHITexture cube) = 0;
  virtual void DrawIndexed(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex) = 0;
  virtual void DrawIndexedInstanced(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex, uint32_t instanceCount) = 0;
  virtual void DrawFullscreenTriangle() = 0;
  virtual RHITimestamp CreateTimestamp() = 0;
  virtual void DestroyTimestamp(RHITimestamp ts) = 0;
  virtual void BeginTimestamp(RHITimestamp ts) = 0;
  virtual void EndTimestamp(RHITimestamp ts) = 0;
  virtual bool GetTimestampMs(RHITimestamp ts, double* ms) = 0;
  virtual void GetMemoryStats(uint64_t& textureBytes, uint64_t& targetBytes, uint64_t& bufferBytes) const = 0;
  virtual RHIOcclusionQuery CreateOcclusionQuery() = 0;
  virtual void DestroyOcclusionQuery(RHIOcclusionQuery q) = 0;
  virtual void BeginOcclusionQuery(RHIOcclusionQuery q) = 0;
  virtual void EndOcclusionQuery(RHIOcclusionQuery q) = 0;
  virtual bool GetOcclusionSamples(RHIOcclusionQuery q, uint64_t* samples) = 0;
  virtual RHITexture3D CreateTexture3D(int w, int h, int d, RHITextureFormat fmt) = 0;
  virtual bool UpdateTexture3D(RHITexture3D tex, int w, int h, int d, uint32_t rowPitch, uint32_t slicePitch, const void* data, size_t bytes) = 0;
  virtual void DestroyTexture3D(RHITexture3D tex) = 0;
};
IRHI* CreateRHI(RHI_API api);
void DestroyRHI(IRHI* rhi);
}
