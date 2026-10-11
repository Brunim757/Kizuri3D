#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "Kizuri/RHI.h"
#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include <list>
#include <utility>
#include <set>
namespace Kizuri {
using Microsoft::WRL::ComPtr;
namespace {
DXGI_FORMAT ToDXGI(RHIFormat f) {
  if (f == RHIFormat::RGBA16F) {
    return DXGI_FORMAT_R16G16B16A16_FLOAT;
  }
  if (f == RHIFormat::D24S8) {
    return DXGI_FORMAT_D24_UNORM_S8_UINT;
  }
  return DXGI_FORMAT_R8G8B8A8_UNORM;
}
DXGI_FORMAT ToTextureFormat(RHITextureFormat f) {
  if (f == RHITextureFormat::RGBA8_UNORM_SRGB) {
    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  }
  if (f == RHITextureFormat::RGBA16F) {
    return DXGI_FORMAT_R16G16B16A16_FLOAT;
  }
  if (f == RHITextureFormat::BC1_UNORM) {
    return DXGI_FORMAT_BC1_UNORM;
  }
  if (f == RHITextureFormat::BC1_UNORM_SRGB) {
    return DXGI_FORMAT_BC1_UNORM_SRGB;
  }
  if (f == RHITextureFormat::BC3_UNORM) {
    return DXGI_FORMAT_BC3_UNORM;
  }
  if (f == RHITextureFormat::BC3_UNORM_SRGB) {
    return DXGI_FORMAT_BC3_UNORM_SRGB;
  }
  if (f == RHITextureFormat::BC5_UNORM) {
    return DXGI_FORMAT_BC5_UNORM;
  }
  if (f == RHITextureFormat::BC4_UNORM) {
    return DXGI_FORMAT_BC4_UNORM;
  }
  if (f == RHITextureFormat::BC6H_UF16) {
    return DXGI_FORMAT_BC6H_UF16;
  }
  if (f == RHITextureFormat::BC7_UNORM) {
    return DXGI_FORMAT_BC7_UNORM;
  }
  if (f == RHITextureFormat::BC7_UNORM_SRGB) {
    return DXGI_FORMAT_BC7_UNORM_SRGB;
  }
  return DXGI_FORMAT_R8G8B8A8_UNORM;
}
bool CheckTextureMipParams(int tw, int th, int tmips, RHITextureFormat tfmt, int mip, int mw, int mh, uint32_t rowPitch, size_t bytes) {
  if (mip < 0 || mip >= tmips) {
    return false;
  }
  int ew = tw >> mip;
  int eh = th >> mip;
  if (ew < 1) {
    ew = 1;
  }
  if (eh < 1) {
    eh = 1;
  }
  if (mw != ew || mh != eh) {
    return false;
  }
  size_t expect = 0;
  if (tfmt == RHITextureFormat::RGBA8_UNORM || tfmt == RHITextureFormat::RGBA8_UNORM_SRGB) {
    if (rowPitch != static_cast<uint32_t>(mw * 4)) {
      return false;
    }
    expect = static_cast<size_t>(rowPitch) * static_cast<size_t>(mh);
  } else if (tfmt == RHITextureFormat::RGBA16F) {
    if (rowPitch != static_cast<uint32_t>(mw * 8)) {
      return false;
    }
    expect = static_cast<size_t>(rowPitch) * static_cast<size_t>(mh);
  } else {
    size_t blockBytes = 16;
    if (tfmt == RHITextureFormat::BC1_UNORM || tfmt == RHITextureFormat::BC1_UNORM_SRGB || tfmt == RHITextureFormat::BC4_UNORM) {
      blockBytes = 8;
    }
    size_t blocksX = (static_cast<size_t>(mw) + 3) / 4;
    size_t blocksY = (static_cast<size_t>(mh) + 3) / 4;
    if (rowPitch != blocksX * blockBytes) {
      return false;
    }
    expect = rowPitch * blocksY;
  }
  return bytes == expect;
}
struct TargetRes {
  ComPtr<ID3D11Texture2D> tex;
  ComPtr<ID3D11RenderTargetView> rtv;
  ComPtr<ID3D11ShaderResourceView> srv;
  ComPtr<ID3D11DepthStencilView> dsv;
  int w = 0;
  int h = 0;
  RHIFormat fmt = RHIFormat::RGBA8_UNORM;
  bool isDepth = false;
};
struct ShadowCubeRes {
  ComPtr<ID3D11Texture2D> tex;
  ComPtr<ID3D11DepthStencilView> dsv[6];
  ComPtr<ID3D11ShaderResourceView> srv;
  int size = 0;
};
struct Tstamp {
  ComPtr<ID3D11Query> disjoint;
  ComPtr<ID3D11Query> begin;
  ComPtr<ID3D11Query> finish;
};
struct OccQuery {
  ComPtr<ID3D11Query> query;
};
uint64_t TargetBytes(int tw, int th, RHIFormat fmt) {
  uint64_t bpp = 4;
  if (fmt == RHIFormat::RGBA16F) {
    bpp = 8;
  }
  return static_cast<uint64_t>(tw) * static_cast<uint64_t>(th) * bpp;
}
uint64_t TextureBytes(int tw, int th, int mips, RHITextureFormat fmt) {
  uint64_t total = 0;
  int mw = tw;
  int mh = th;
  for (int m = 0; m < mips; ++m) {
    if (fmt == RHITextureFormat::RGBA8_UNORM || fmt == RHITextureFormat::RGBA8_UNORM_SRGB) {
      total += static_cast<uint64_t>(mw) * static_cast<uint64_t>(mh) * 4;
    } else {
      uint64_t blockBytes = 16;
      if (fmt == RHITextureFormat::BC1_UNORM || fmt == RHITextureFormat::BC1_UNORM_SRGB) {
        blockBytes = 8;
      }
      uint64_t bx = (static_cast<uint64_t>(mw) + 3) / 4;
      uint64_t by = (static_cast<uint64_t>(mh) + 3) / 4;
      total += bx * by * blockBytes;
    }
    if (mw > 1) {
      mw >>= 1;
    }
    if (mh > 1) {
      mh >>= 1;
    }
  }
  return total;
}
}
class D3D11RHI : public IRHI {
public:
  D3D11RHI()
    : hwnd(nullptr)
    , w(0)
    , h(0)
    , nextId(1)
    , total(0)
    , discarded(0)
    , curVS(0)
    , curPS(0)
    , curLayout(0)
    , curVB(0)
    , curVBOffset(0)
    , curIB(0) {
    curRS.cull = RHICull::Back;
    curRS.fill = RHIFill::Solid;
    curRS.frontCCW = false;
    curRS.slopeBias = 0.0f;
    curDS.depthEnable = true;
    curDS.depthWrite = true;
    curBlend.enable = false;
    curTopoD3D = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    curVP.x = 0.0f;
    curVP.y = 0.0f;
    curVP.w = 0.0f;
    curVP.h = 0.0f;
    curVP.minD = 0.0f;
    curVP.maxD = 1.0f;
    hasRS = false;
    hasDS = false;
    hasBlend = false;
    hasVP = false;
  }
  bool Initialize(const RHIDesc& desc) override {
    hwnd = static_cast<HWND>(desc.windowHandle);
    w = desc.width;
    h = desc.height;
    if (hwnd == nullptr || w <= 0 || h <= 0) {
      return false;
    }
    DXGI_SWAP_CHAIN_DESC scd;
    scd.BufferDesc.Width = static_cast<UINT>(w);
    scd.BufferDesc.Height = static_cast<UINT>(h);
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 1;
    scd.OutputWindow = hwnd;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    scd.Flags = 0;
    D3D_FEATURE_LEVEL requested[] = {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0
    };
    D3D_FEATURE_LEVEL obtained = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
      nullptr,
      D3D_DRIVER_TYPE_HARDWARE,
      nullptr,
      0,
      requested,
      4,
      D3D11_SDK_VERSION,
      &scd,
      swapchain.GetAddressOf(),
      device.GetAddressOf(),
      &obtained,
      context.GetAddressOf());
    if (FAILED(hr)) {
      hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_WARP,
        nullptr,
        0,
        requested,
        4,
        D3D11_SDK_VERSION,
        &scd,
        swapchain.GetAddressOf(),
        device.GetAddressOf(),
        &obtained,
        context.GetAddressOf());
      if (FAILED(hr)) {
        return false;
      }
    }
    if (!CreateBackbuffer()) {
      return false;
    }
    return true;
  }
  void Shutdown() override {
    targets.clear();
    pool.clear();
    occQueries.clear();
    tstamps.clear();
    samplers.clear();
    layouts.clear();
    cubes.clear();
    lastCube = 0;
    lastCubeFace = -1;
    vsBlobs.clear();
    vsMap.clear();
    psMap.clear();
    buffers.clear();
    cbuffers.clear();
    target.Reset();
    context.Reset();
    device.Reset();
    swapchain.Reset();
    backRTV.Reset();
    hwnd = nullptr;
    w = 0;
    h = 0;
  }
  bool Resize(int nw, int nh) override {
    if (nw <= 0 || nh <= 0) {
      return false;
    }
    if (swapchain == nullptr) {
      return false;
    }
    context->OMSetRenderTargets(0, nullptr, nullptr);
    backRTV.Reset();
    HRESULT hr = swapchain->ResizeBuffers(1, static_cast<UINT>(nw), static_cast<UINT>(nh), DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    if (FAILED(hr)) {
      return false;
    }
    w = nw;
    h = nh;
    hasRS = false;
    hasDS = false;
    hasBlend = false;
    hasVP = false;
    curRTs.clear();
    curDepth = 0;
    return CreateBackbuffer();
  }
  void Clear(float r, float g, float b, float a) override {
    BindBackbuffer();
    float c[4] = { r, g, b, a };
    if (backRTV != nullptr) {
      context->ClearRenderTargetView(backRTV.Get(), c);
    }
  }
  void Present(bool vsync) override {
    if (swapchain == nullptr) {
      return;
    }
    swapchain->Present(vsync ? 1 : 0, 0);
  }
  const char* BackendName() const override {
    return "D3D11";
  }
  int Width() const override {
    return w;
  }
  int Height() const override {
    return h;
  }
  void* GetNativeDevice() const override {
    return device.Get();
  }
  void* GetNativeContext() const override {
    return context.Get();
  }
  void GetCacheStats(uint64_t& t, uint64_t& d) const override {
    t = total;
    d = discarded;
  }
  void GetShaderCacheStats(uint64_t& hits, uint64_t& misses, uint64_t& permutations) const override {
    hits = shaderCacheHits;
    misses = shaderCacheMiss;
    permutations = static_cast<uint64_t>(shaderPermCounts.size());
  }
  void GetPoolStats(uint64_t& hits, uint64_t& misses) const override {
    hits = poolHits;
    misses = poolMiss;
  }
  uint64_t LiveTargetCount() const override {
    return static_cast<uint64_t>(targets.size());
  }
  RHIGpuFence CreateGpuFence() override {
    D3D11_QUERY_DESC qd;
    qd.Query = D3D11_QUERY_EVENT;
    qd.MiscFlags = 0;
    ComPtr<ID3D11Query> query;
    if (FAILED(device->CreateQuery(&qd, query.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    fences[id] = query;
    return id;
  }
  void DestroyGpuFence(RHIGpuFence fence) override {
    fences.erase(fence);
  }
  void SignalGpuFence(RHIGpuFence fence) override {
    auto it = fences.find(fence);
    if (it == fences.end()) {
      return;
    }
    context->End(it->second.Get());
  }
  bool WaitGpuFence(RHIGpuFence fence, uint32_t timeoutMs) override {
    auto it = fences.find(fence);
    if (it == fences.end()) {
      return false;
    }
    uint32_t waited = 0;
    for (;;) {
      HRESULT hr = context->GetData(it->second.Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH);
      if (hr == S_OK) {
        return true;
      }
      if (waited >= timeoutMs) {
        context->Flush();
        hr = context->GetData(it->second.Get(), nullptr, 0, 0);
        return hr == S_OK;
      }
      Sleep(1);
      ++waited;
    }
  }
  RHIBuffer CreateBuffer(uint64_t size, uint32_t stride, bool isIndex, const void* initialData) override {
    if (size == 0 || size > 256 * 1024 * 1024) {
      return 0;
    }
    D3D11_BUFFER_DESC bd;
    bd.ByteWidth = static_cast<UINT>(size);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = isIndex ? D3D11_BIND_INDEX_BUFFER : D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    bd.StructureByteStride = 0;
    ComPtr<ID3D11Buffer> buf;
    if (initialData != nullptr) {
      D3D11_SUBRESOURCE_DATA sd;
      sd.pSysMem = initialData;
      sd.SysMemPitch = 0;
      sd.SysMemSlicePitch = 0;
      if (FAILED(device->CreateBuffer(&bd, &sd, buf.GetAddressOf()))) {
        return 0;
      }
    } else {
      if (FAILED(device->CreateBuffer(&bd, nullptr, buf.GetAddressOf()))) {
        return 0;
      }
    }
    uint64_t id = nextId++;
    buffers[id] = buf;
    strides[id] = stride;
    sizes[id] = size;
    return id;
  }
  RHIBuffer CreateBufferEmpty(uint64_t size, uint32_t stride, bool isIndex) override {
    if (size == 0 || size > 256 * 1024 * 1024) {
      return 0;
    }
    D3D11_BUFFER_DESC bd;
    bd.ByteWidth = static_cast<UINT>(size);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = isIndex ? D3D11_BIND_INDEX_BUFFER : D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = 0;
    bd.StructureByteStride = 0;
    ComPtr<ID3D11Buffer> buf;
    if (FAILED(device->CreateBuffer(&bd, nullptr, buf.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    buffers[id] = buf;
    strides[id] = stride;
    sizes[id] = size;
    return id;
  }
  bool UpdateBufferRange(RHIBuffer buf, uint64_t offset, const void* data, size_t bytes) override {
    auto it = buffers.find(buf);
    if (it == buffers.end() || data == nullptr || bytes == 0) {
      return false;
    }
    auto sz = sizes.find(buf);
    if (sz == sizes.end() || offset + bytes > sz->second) {
      return false;
    }
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->Map(it->second.Get(), 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped))) {
      return false;
    }
    std::memcpy(static_cast<unsigned char*>(mapped.pData) + static_cast<size_t>(offset), data, bytes);
    context->Unmap(it->second.Get(), 0);
    return true;
  }
  bool UpdateBufferDiscard(RHIBuffer buf, const void* data, size_t bytes) override {
    auto it = buffers.find(buf);
    if (it == buffers.end() || data == nullptr || bytes == 0) {
      return false;
    }
    auto sz = sizes.find(buf);
    if (sz == sizes.end() || bytes > sz->second) {
      return false;
    }
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->Map(it->second.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
      return false;
    }
    std::memcpy(mapped.pData, data, bytes);
    context->Unmap(it->second.Get(), 0);
    return true;
  }
  void DestroyBuffer(RHIBuffer buf) override {
    buffers.erase(buf);
    strides.erase(buf);
    sizes.erase(buf);
  }
  void SetVertexBuffer(RHIBuffer buf, uint32_t offset) override {
    if (curVB == buf && curVBOffset == offset) {
      Note(true);
      return;
    }
    Note(false);
    curVB = buf;
    curVBOffset = offset;
    auto it = buffers.find(buf);
    if (it == buffers.end()) {
      ID3D11Buffer* nullBuf = nullptr;
      UINT zero = 0;
      UINT off = offset;
      context->IASetVertexBuffers(0, 1, &nullBuf, &zero, &off);
      return;
    }
    UINT stride = strides[buf];
    UINT off = offset;
    ID3D11Buffer* b = it->second.Get();
    context->IASetVertexBuffers(0, 1, &b, &stride, &off);
  }
  void SetIndexBuffer(RHIBuffer buf) override {
    if (curIB == buf) {
      Note(true);
      return;
    }
    Note(false);
    curIB = buf;
    auto it = buffers.find(buf);
    if (it == buffers.end()) {
      context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
      return;
    }
    context->IASetIndexBuffer(it->second.Get(), DXGI_FORMAT_R32_UINT, 0);
  }
  RHIConstBuffer CreateConstantBuffer(uint64_t size, const void* initialData) override {
    if (size == 0 || size > 65536) {
      return 0;
    }
    UINT aligned = static_cast<UINT>((size + 15) & ~15ULL);
    D3D11_BUFFER_DESC bd;
    bd.ByteWidth = aligned;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = 0;
    bd.StructureByteStride = 0;
    ComPtr<ID3D11Buffer> buf;
    if (initialData != nullptr) {
      D3D11_SUBRESOURCE_DATA sd;
      sd.pSysMem = initialData;
      sd.SysMemPitch = 0;
      sd.SysMemSlicePitch = 0;
      if (FAILED(device->CreateBuffer(&bd, &sd, buf.GetAddressOf()))) {
        return 0;
      }
    } else {
      if (FAILED(device->CreateBuffer(&bd, nullptr, buf.GetAddressOf()))) {
        return 0;
      }
    }
    uint64_t id = nextId++;
    cbuffers[id] = buf;
    csizes[id] = aligned;
    return id;
  }
  void UpdateConstantBuffer(RHIConstBuffer buf, const void* data, uint64_t size) override {
    auto it = cbuffers.find(buf);
    if (it == cbuffers.end() || data == nullptr) {
      return;
    }
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->Map(it->second.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
      return;
    }
    memcpy(mapped.pData, data, static_cast<size_t>(size));
    context->Unmap(it->second.Get(), 0);
  }
  void DestroyConstantBuffer(RHIConstBuffer buf) override {
    cbuffers.erase(buf);
    csizes.erase(buf);
  }
  void SetVertexConstantBuffer(uint32_t slot, RHIConstBuffer buf) override {
    auto it = cbuffers.find(buf);
    if (it == cbuffers.end()) {
      return;
    }
    ID3D11Buffer* b = it->second.Get();
    context->VSSetConstantBuffers(slot, 1, &b);
  }
  void SetPixelConstantBuffer(uint32_t slot, RHIConstBuffer buf) override {
    auto it = cbuffers.find(buf);
    if (it == cbuffers.end()) {
      return;
    }
    ID3D11Buffer* b = it->second.Get();
    context->PSSetConstantBuffers(slot, 1, &b);
  }
  RHIVertexShader CreateVertexShaderFromFile(const char* path, const char* entry) override {
    ComPtr<ID3DBlob> blob;
    if (!CompileShader(path, entry, "vs_5_0", blob)) {
      return 0;
    }
    ComPtr<ID3D11VertexShader> vs;
    if (FAILED(device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, vs.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    vsMap[id] = vs;
    vsBlobs[id] = blob;
    return id;
  }
  RHIPixelShader CreatePixelShaderFromFile(const char* path, const char* entry) override {
    ComPtr<ID3DBlob> blob;
    if (!CompileShader(path, entry, "ps_5_0", blob)) {
      return 0;
    }
    ComPtr<ID3D11PixelShader> ps;
    if (FAILED(device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, ps.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    psMap[id] = ps;
    return id;
  }
  void SetVertexShader(RHIVertexShader vs) override {
    if (curVS == vs) {
      Note(true);
      return;
    }
    Note(false);
    curVS = vs;
    auto it = vsMap.find(vs);
    context->VSSetShader(it == vsMap.end() ? nullptr : it->second.Get(), nullptr, 0);
  }
  RHIHullShader CreateHullShaderFromFile(const char* path, const char* entry) override {
    ComPtr<ID3DBlob> blob;
    if (!CompileShader(path, entry != nullptr ? entry : "main", "hs_5_0", blob)) {
      return 0;
    }
    ComPtr<ID3D11HullShader> hs;
    if (FAILED(device->CreateHullShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, hs.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    hsMap[id] = hs;
    return id;
  }
  RHIDomainShader CreateDomainShaderFromFile(const char* path, const char* entry) override {
    ComPtr<ID3DBlob> blob;
    if (!CompileShader(path, entry != nullptr ? entry : "main", "ds_5_0", blob)) {
      return 0;
    }
    ComPtr<ID3D11DomainShader> ds;
    if (FAILED(device->CreateDomainShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, ds.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    dsMap[id] = ds;
    return id;
  }
  void SetHullShader(RHIHullShader hs) override {
    auto it = hsMap.find(hs);
    context->HSSetShader(it == hsMap.end() ? nullptr : it->second.Get(), nullptr, 0);
  }
  void SetDomainShader(RHIDomainShader ds) override {
    auto it = dsMap.find(ds);
    context->DSSetShader(it == dsMap.end() ? nullptr : it->second.Get(), nullptr, 0);
  }
  void SetHullConstantBuffer(uint32_t slot, RHIConstBuffer cb) override {
    auto it = cbuffers.find(cb);
    ID3D11Buffer* b = (it == cbuffers.end()) ? nullptr : it->second.Get();
    context->HSSetConstantBuffers(slot, 1, &b);
  }
  void SetDomainConstantBuffer(uint32_t slot, RHIConstBuffer cb) override {
    auto it = cbuffers.find(cb);
    ID3D11Buffer* b = (it == cbuffers.end()) ? nullptr : it->second.Get();
    context->DSSetConstantBuffers(slot, 1, &b);
  }
  void SetDomainTexture(uint32_t slot, RHITexture tex) override {
    auto it = textures.find(tex);
    ID3D11ShaderResourceView* v = (it == textures.end()) ? nullptr : it->second.srv.Get();
    context->DSSetShaderResources(slot, 1, &v);
  }
  void SetDomainSampler(uint32_t slot, RHISampler sampler) override {
    auto it = samplers.find(sampler);
    ID3D11SamplerState* s = (it == samplers.end()) ? nullptr : it->second.Get();
    context->DSSetSamplers(slot, 1, &s);
  }
  void SetPixelShader(RHIPixelShader ps) override {
    if (curPS == ps) {
      Note(true);
      return;
    }
    Note(false);
    curPS = ps;
    auto it = psMap.find(ps);
    context->PSSetShader(it == psMap.end() ? nullptr : it->second.Get(), nullptr, 0);
  }
  RHIInputLayout CreateInputLayoutPNUT(RHIVertexShader vs) override {
    auto it = vsBlobs.find(vs);
    if (it == vsBlobs.end()) {
      return 0;
    }
    D3D11_INPUT_ELEMENT_DESC desc[5];
    desc[0].SemanticName = "POSITION";
    desc[0].SemanticIndex = 0;
    desc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    desc[0].InputSlot = 0;
    desc[0].AlignedByteOffset = 0;
    desc[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    desc[0].InstanceDataStepRate = 0;
    desc[1].SemanticName = "NORMAL";
    desc[1].SemanticIndex = 0;
    desc[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    desc[1].InputSlot = 0;
    desc[1].AlignedByteOffset = 12;
    desc[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    desc[1].InstanceDataStepRate = 0;
    desc[2].SemanticName = "TEXCOORD";
    desc[2].SemanticIndex = 0;
    desc[2].Format = DXGI_FORMAT_R32G32_FLOAT;
    desc[2].InputSlot = 0;
    desc[2].AlignedByteOffset = 24;
    desc[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    desc[2].InstanceDataStepRate = 0;
    desc[3].SemanticName = "TANGENT";
    desc[3].SemanticIndex = 0;
    desc[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc[3].InputSlot = 0;
    desc[3].AlignedByteOffset = 32;
    desc[3].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    desc[3].InstanceDataStepRate = 0;
    desc[4].SemanticName = "TEXCOORD";
    desc[4].SemanticIndex = 1;
    desc[4].Format = DXGI_FORMAT_R32G32_FLOAT;
    desc[4].InputSlot = 0;
    desc[4].AlignedByteOffset = 48;
    desc[4].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    desc[4].InstanceDataStepRate = 0;
    ComPtr<ID3D11InputLayout> layout;
    if (FAILED(device->CreateInputLayout(desc, 5, it->second->GetBufferPointer(), it->second->GetBufferSize(), layout.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    layouts[id] = layout;
    return id;
  }
  RHIInputLayout CreateInputLayoutPNUTInstanced(RHIVertexShader vs) override {
    auto it = vsBlobs.find(vs);
    if (it == vsBlobs.end()) {
      return 0;
    }
    D3D11_INPUT_ELEMENT_DESC desc[9];
    desc[0].SemanticName = "POSITION";
    desc[0].SemanticIndex = 0;
    desc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    desc[0].InputSlot = 0;
    desc[0].AlignedByteOffset = 0;
    desc[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    desc[0].InstanceDataStepRate = 0;
    desc[1].SemanticName = "NORMAL";
    desc[1].SemanticIndex = 0;
    desc[1].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    desc[1].InputSlot = 0;
    desc[1].AlignedByteOffset = 12;
    desc[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    desc[1].InstanceDataStepRate = 0;
    desc[2].SemanticName = "TEXCOORD";
    desc[2].SemanticIndex = 0;
    desc[2].Format = DXGI_FORMAT_R32G32_FLOAT;
    desc[2].InputSlot = 0;
    desc[2].AlignedByteOffset = 24;
    desc[2].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    desc[2].InstanceDataStepRate = 0;
    desc[3].SemanticName = "TANGENT";
    desc[3].SemanticIndex = 0;
    desc[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc[3].InputSlot = 0;
    desc[3].AlignedByteOffset = 32;
    desc[3].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    desc[3].InstanceDataStepRate = 0;
    desc[4].SemanticName = "TEXCOORD";
    desc[4].SemanticIndex = 1;
    desc[4].Format = DXGI_FORMAT_R32G32_FLOAT;
    desc[4].InputSlot = 0;
    desc[4].AlignedByteOffset = 48;
    desc[4].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    desc[4].InstanceDataStepRate = 0;
    for (int r = 0; r < 4; ++r) {
      desc[5 + r].SemanticName = "INSTANCE";
      desc[5 + r].SemanticIndex = r;
      desc[5 + r].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
      desc[5 + r].InputSlot = 1;
      desc[5 + r].AlignedByteOffset = static_cast<UINT>(r * 16);
      desc[5 + r].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
      desc[5 + r].InstanceDataStepRate = 1;
    }
    ComPtr<ID3D11InputLayout> layout;
    if (FAILED(device->CreateInputLayout(desc, 9, it->second->GetBufferPointer(), it->second->GetBufferSize(), layout.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    layouts[id] = layout;
    return id;
  }
  void SetInstanceBuffer(RHIBuffer buf) override {
    auto it = buffers.find(buf);
    ID3D11Buffer* b = it == buffers.end() ? nullptr : it->second.Get();
    UINT stride = 64;
    UINT offset = 0;
    context->IASetVertexBuffers(1, 1, &b, &stride, &offset);
  }
  void SetInputLayout(RHIInputLayout layout) override {    if (curLayout == layout) {
      Note(true);
      return;
    }
    Note(false);
    curLayout = layout;
    auto it = layouts.find(layout);
    context->IASetInputLayout(it == layouts.end() ? nullptr : it->second.Get());
  }
  void SetViewport(const RHIViewport& vp) override {
    if (hasVP && curVP.x == vp.x && curVP.y == vp.y && curVP.w == vp.w && curVP.h == vp.h && curVP.minD == vp.minD && curVP.maxD == vp.maxD) {
      Note(true);
      return;
    }
    Note(false);
    curVP = vp;
    hasVP = true;
    D3D11_VIEWPORT d3dvp;
    d3dvp.TopLeftX = vp.x;
    d3dvp.TopLeftY = vp.y;
    d3dvp.Width = vp.w;
    d3dvp.Height = vp.h;
    d3dvp.MinDepth = vp.minD;
    d3dvp.MaxDepth = vp.maxD;
    context->RSSetViewports(1, &d3dvp);
  }
  void SetTopology(RHITopology topo) override {
    D3D11_PRIMITIVE_TOPOLOGY d3dTopo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    if (topo == RHITopology::Patch3) {
      d3dTopo = D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
    }
    if (curTopoD3D == d3dTopo && hasTopo) {
      Note(true);
      return;
    }
    Note(false);
    curTopoD3D = d3dTopo;
    hasTopo = true;
    context->IASetPrimitiveTopology(d3dTopo);
  }
  void SetRasterizerState(const RHIRasterizer& rs) override {
    if (hasRS && curRS.cull == rs.cull && curRS.fill == rs.fill && curRS.frontCCW == rs.frontCCW && curRS.slopeBias == rs.slopeBias) {
      Note(true);
      return;
    }
    Note(false);
    curRS = rs;
    hasRS = true;
    D3D11_RASTERIZER_DESC d;
    d.FillMode = (rs.fill == RHIFill::Wireframe) ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
    if (rs.cull == RHICull::None) {
      d.CullMode = D3D11_CULL_NONE;
    } else if (rs.cull == RHICull::Front) {
      d.CullMode = D3D11_CULL_FRONT;
    } else {
      d.CullMode = D3D11_CULL_BACK;
    }
    d.FrontCounterClockwise = rs.frontCCW ? TRUE : FALSE;
    d.DepthBias = 0;
    d.DepthBiasClamp = 0.0f;
    d.SlopeScaledDepthBias = rs.slopeBias;
    d.DepthClipEnable = TRUE;
    d.ScissorEnable = FALSE;
    d.MultisampleEnable = FALSE;
    d.AntialiasedLineEnable = FALSE;
    ComPtr<ID3D11RasterizerState> state;
    if (SUCCEEDED(device->CreateRasterizerState(&d, state.GetAddressOf()))) {
      rsCache = state;
      context->RSSetState(state.Get());
    }
  }
  void SetDepthStencilState(const RHIDepthStencil& ds) override {
    if (hasDS && curDS.depthEnable == ds.depthEnable && curDS.depthWrite == ds.depthWrite) {
      Note(true);
      return;
    }
    Note(false);
    curDS = ds;
    hasDS = true;
    D3D11_DEPTH_STENCIL_DESC d;
    d.DepthEnable = ds.depthEnable ? TRUE : FALSE;
    d.DepthWriteMask = ds.depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    d.DepthFunc = D3D11_COMPARISON_LESS;
    d.StencilEnable = FALSE;
    d.StencilReadMask = 0xFF;
    d.StencilWriteMask = 0xFF;
    d.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    d.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    d.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    d.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    d.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    d.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    d.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    d.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    ComPtr<ID3D11DepthStencilState> state;
    if (SUCCEEDED(device->CreateDepthStencilState(&d, state.GetAddressOf()))) {
      dsCache = state;
      context->OMSetDepthStencilState(state.Get(), 0);
    }
  }
  void SetBlendState(const RHIBlend& blend) override {
    if (hasBlend && curBlend.enable == blend.enable) {
      Note(true);
      return;
    }
    Note(false);
    curBlend = blend;
    hasBlend = true;
    D3D11_BLEND_DESC d;
    d.AlphaToCoverageEnable = FALSE;
    d.IndependentBlendEnable = FALSE;
    d.RenderTarget[0].BlendEnable = blend.enable ? TRUE : FALSE;
    d.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    d.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    d.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    for (int i = 1; i < 8; ++i) {
      d.RenderTarget[i] = d.RenderTarget[0];
    }
    ComPtr<ID3D11BlendState> state;
    if (SUCCEEDED(device->CreateBlendState(&d, state.GetAddressOf()))) {
      blendCache = state;
      float f[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
      context->OMSetBlendState(state.Get(), f, 0xFFFFFFFF);
    }
  }
  RHIRenderTarget CreateRenderTarget(int tw, int th, RHIFormat fmt) override {
    if (tw <= 0 || th <= 0) {
      return 0;
    }
    for (auto it = pool.begin(); it != pool.end(); ++it) {
      if (it->w == tw && it->h == th && it->fmt == fmt && it->isDepth == (fmt == RHIFormat::D24S8 || fmt == RHIFormat::R32_DEPTH)) {
        uint64_t id = nextId++;
        targets[id] = std::move(*it);
        pool.erase(it);
        ++poolHits;
        return id;
      }
    }
    TargetRes res;
    res.w = tw;
    res.h = th;
    res.fmt = fmt;
    res.isDepth = (fmt == RHIFormat::D24S8 || fmt == RHIFormat::R32_DEPTH);
    D3D11_TEXTURE2D_DESC td;
    td.Width = static_cast<UINT>(tw);
    td.Height = static_cast<UINT>(th);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = ToDXGI(fmt);
    td.SampleDesc.Count = 1;
    td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.CPUAccessFlags = 0;
    td.MiscFlags = 0;
    if (res.isDepth) {
      if (fmt == RHIFormat::R32_DEPTH) {
        td.Format = DXGI_FORMAT_R32_TYPELESS;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
      } else {
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
      }
      if (FAILED(device->CreateTexture2D(&td, nullptr, res.tex.GetAddressOf()))) {
        return 0;
      }
      if (fmt == RHIFormat::R32_DEPTH) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dd;
        dd.Format = DXGI_FORMAT_D32_FLOAT;
        dd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dd.Flags = 0;
        dd.Texture2D.MipSlice = 0;
        if (FAILED(device->CreateDepthStencilView(res.tex.Get(), &dd, res.dsv.GetAddressOf()))) {
          return 0;
        }
        D3D11_SHADER_RESOURCE_VIEW_DESC sd;
        sd.Format = DXGI_FORMAT_R32_FLOAT;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MostDetailedMip = 0;
        sd.Texture2D.MipLevels = 1;
        if (FAILED(device->CreateShaderResourceView(res.tex.Get(), &sd, res.srv.GetAddressOf()))) {
          return 0;
        }
      } else {
        if (FAILED(device->CreateDepthStencilView(res.tex.Get(), nullptr, res.dsv.GetAddressOf()))) {
          return 0;
        }
      }
    } else {
      td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
      if (FAILED(device->CreateTexture2D(&td, nullptr, res.tex.GetAddressOf()))) {
        return 0;
      }
      if (FAILED(device->CreateRenderTargetView(res.tex.Get(), nullptr, res.rtv.GetAddressOf()))) {
        return 0;
      }
      if (FAILED(device->CreateShaderResourceView(res.tex.Get(), nullptr, res.srv.GetAddressOf()))) {
        return 0;
      }
    }
    uint64_t id = nextId++;
    targets[id] = res;
    ++poolMiss;
    return id;
  }
  void DestroyRenderTarget(RHIRenderTarget rt) override {
    auto it = targets.find(rt);
    if (it == targets.end()) {
      cubes.erase(rt);
      if (lastCube == rt) {
        lastCube = 0;
        lastCubeFace = -1;
      }
      return;
    }
    if (pool.size() < 32 && it->second.w <= 2048 && it->second.h <= 2048) {
      pool.push_back(std::move(it->second));
    }
    targets.erase(it);
    cubes.erase(rt);
    if (lastCube == rt) {
      lastCube = 0;
      lastCubeFace = -1;
    }
  }
  void SetRenderTargets(uint32_t count, const RHIRenderTarget* colorRTs, RHIRenderTarget depthRT) override {
    bool same = (curRTs.size() == count && curDepth == depthRT);
    if (same) {
      for (uint32_t i = 0; i < count; ++i) {
        if (curRTs[i] != colorRTs[i]) {
          same = false;
          break;
        }
      }
    }
    if (same) {
      Note(true);
      return;
    }
    Note(false);
    curRTs.assign(colorRTs, colorRTs + count);
    curDepth = depthRT;
    lastCube = 0;
    lastCubeFace = -1;
    ID3D11RenderTargetView* rtvs[8] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
    uint32_t n = count > 8 ? 8 : count;
    for (uint32_t i = 0; i < n; ++i) {
      auto it = targets.find(colorRTs[i]);
      rtvs[i] = (it == targets.end()) ? nullptr : it->second.rtv.Get();
    }
    ID3D11DepthStencilView* dsv = nullptr;
    auto dit = targets.find(depthRT);
    if (dit != targets.end()) {
      dsv = dit->second.dsv.Get();
    }
    context->OMSetRenderTargets(n, n == 0 ? nullptr : rtvs, dsv);
    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = static_cast<FLOAT>(w);
    vp.Height = static_cast<FLOAT>(h);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    if (!curRTs.empty()) {
      auto it = targets.find(curRTs[0]);
      if (it != targets.end()) {
        vp.Width = static_cast<FLOAT>(it->second.w);
        vp.Height = static_cast<FLOAT>(it->second.h);
      }
    }
    context->RSSetViewports(1, &vp);
  }
  void ClearRenderTarget(RHIRenderTarget rt, float r, float g, float b, float a) override {
    auto it = targets.find(rt);
    if (it == targets.end() || it->second.rtv == nullptr) {
      return;
    }
    float c[4] = { r, g, b, a };
    context->ClearRenderTargetView(it->second.rtv.Get(), c);
  }
  void ClearDepth(RHIRenderTarget depthRT) override {
    auto it = targets.find(depthRT);
    if (it != targets.end() && it->second.dsv != nullptr) {
      context->ClearDepthStencilView(it->second.dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
      return;
    }
    if (depthRT == lastCube && lastCubeFace >= 0 && lastCubeFace < 6) {
      auto cit = cubes.find(depthRT);
      if (cit != cubes.end() && cit->second.dsv[lastCubeFace] != nullptr) {
        context->ClearDepthStencilView(cit->second.dsv[lastCubeFace].Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
      }
    }
  }
  void* GetRenderTargetSRV(RHIRenderTarget rt) const override {
    auto it = targets.find(rt);
    if (it != targets.end()) {
      return it->second.srv.Get();
    }
    auto cit = cubes.find(rt);
    if (cit != cubes.end()) {
      return cit->second.srv.Get();
    }
    return nullptr;
  }
  void* GetTextureSRV(RHITexture tex) const override {
    auto it = textures.find(tex);
    if (it != textures.end()) {
      return it->second.srv.Get();
    }
    return nullptr;
  }
  void BindBackbuffer() override {
    bool same = (curRTs.size() == 1 && curRTs[0] == backId && curDepth == 0);
    if (same) {
      Note(true);
      return;
    }
    Note(false);
    curRTs.clear();
    curRTs.push_back(backId);
    curDepth = 0;
    lastCube = 0;
    lastCubeFace = -1;
    ID3D11RenderTargetView* rtv = backRTV.Get();
    context->OMSetRenderTargets(1, &rtv, nullptr);
    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = static_cast<FLOAT>(w);
    vp.Height = static_cast<FLOAT>(h);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context->RSSetViewports(1, &vp);
  }
  void BeginEvent(const char* name) override {
    if (!annotationTried) {
      annotationTried = true;
      if (context.Get() != nullptr) {
        context->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), reinterpret_cast<void**>(annotation.GetAddressOf()));
      }
    }
    if (annotation.Get() != nullptr && name != nullptr) {
      wchar_t wide[128];
      size_t i = 0;
      for (; name[i] != '\0' && i < 127; ++i) {
        wide[i] = static_cast<wchar_t>(static_cast<unsigned char>(name[i]));
      }
      wide[i] = L'\0';
      annotation->BeginEvent(wide);
    }
  }
  void EndEvent() override {
    if (annotation.Get() != nullptr) {
      annotation->EndEvent();
    }
  }
  RHIComputeShader CreateComputeShaderFromFile(const char* path, const char* entry) override {
    ComPtr<ID3DBlob> blob;
    if (!CompileShader(path, entry != nullptr ? entry : "main", "cs_5_0", blob)) {
      return 0;
    }
    ComPtr<ID3D11ComputeShader> cs;
    if (FAILED(device->CreateComputeShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, cs.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    csMap[id] = cs;
    return id;
  }
  RHIStructBuffer CreateStructBuffer(uint32_t count, uint32_t stride, const void* initData) override {
    if (count == 0 || stride == 0) {
      return 0;
    }
    D3D11_BUFFER_DESC bd;
    bd.ByteWidth = count * stride;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = stride;
    D3D11_SUBRESOURCE_DATA init;
    D3D11_SUBRESOURCE_DATA* initPtr = nullptr;
    if (initData != nullptr) {
      init.pSysMem = initData;
      init.SysMemPitch = 0;
      init.SysMemSlicePitch = 0;
      initPtr = &init;
    }
    ComPtr<ID3D11Buffer> buf;
    if (FAILED(device->CreateBuffer(&bd, initPtr, buf.GetAddressOf()))) {
      return 0;
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC svd;
    svd.Format = DXGI_FORMAT_UNKNOWN;
    svd.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    svd.BufferEx.FirstElement = 0;
    svd.BufferEx.NumElements = count;
    svd.BufferEx.Flags = 0;
    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(device->CreateShaderResourceView(buf.Get(), &svd, srv.GetAddressOf()))) {
      return 0;
    }
    D3D11_UNORDERED_ACCESS_VIEW_DESC uvd;
    uvd.Format = DXGI_FORMAT_UNKNOWN;
    uvd.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uvd.Buffer.FirstElement = 0;
    uvd.Buffer.NumElements = count;
    uvd.Buffer.Flags = 0;
    ComPtr<ID3D11UnorderedAccessView> uav;
    if (FAILED(device->CreateUnorderedAccessView(buf.Get(), &uvd, uav.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    StructBuf sb;
    sb.buf = buf;
    sb.srv = srv;
    sb.uav = uav;
    sb.count = count;
    sb.stride = stride;
    structBufs[id] = sb;
    return id;
  }
  void DestroyStructBuffer(RHIStructBuffer buf) override {
    structBufs.erase(buf);
  }
  void SetComputeShader(RHIComputeShader cs) override {
    auto it = csMap.find(cs);
    context->CSSetShader(it == csMap.end() ? nullptr : it->second.Get(), nullptr, 0);
  }
  void SetComputeConstantBuffer(uint32_t slot, RHIConstBuffer cb) override {
    auto it = cbuffers.find(cb);
    ID3D11Buffer* b = (it == cbuffers.end()) ? nullptr : it->second.Get();
    context->CSSetConstantBuffers(slot, 1, &b);
  }
  void SetComputeSRV(uint32_t slot, RHIStructBuffer buf) override {
    auto it = structBufs.find(buf);
    ID3D11ShaderResourceView* v = (it == structBufs.end()) ? nullptr : it->second.srv.Get();
    context->CSSetShaderResources(slot, 1, &v);
  }
  void SetComputeUAV(uint32_t slot, RHIStructBuffer buf) override {
    auto it = structBufs.find(buf);
    ID3D11UnorderedAccessView* v = (it == structBufs.end()) ? nullptr : it->second.uav.Get();
    UINT initial = 0;
    context->CSSetUnorderedAccessViews(slot, 1, &v, &initial);
  }
  void Dispatch(uint32_t x, uint32_t y, uint32_t z) override {
    if (x == 0 || y == 0 || z == 0) {
      return;
    }
    if (x > 65535) {
      x = 65535;
    }
    if (y > 65535) {
      y = 65535;
    }
    if (z > 64) {
      z = 64;
    }
    context->Dispatch(x, y, z);
  }
  bool ReadStructBuffer(RHIStructBuffer buf, void* dst, size_t bytes) override {
    if (dst == nullptr || bytes == 0) {
      return false;
    }
    auto it = structBufs.find(buf);
    if (it == structBufs.end()) {
      return false;
    }
    size_t total = static_cast<size_t>(it->second.count) * it->second.stride;
    if (bytes > total) {
      bytes = total;
    }
    D3D11_BUFFER_DESC sd;
    sd.ByteWidth = static_cast<UINT>(total);
    sd.Usage = D3D11_USAGE_STAGING;
    sd.BindFlags = 0;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    sd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    sd.StructureByteStride = it->second.stride;
    ComPtr<ID3D11Buffer> staging;
    if (FAILED(device->CreateBuffer(&sd, nullptr, staging.GetAddressOf()))) {
      return false;
    }
    context->CopyResource(staging.Get(), it->second.buf.Get());
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
      return false;
    }
    std::memcpy(dst, mapped.pData, bytes);
    context->Unmap(staging.Get(), 0);
    return true;
  }
  RHIStructBuffer CreateIndirectArgs() override {
    D3D11_BUFFER_DESC bd;
    bd.ByteWidth = 20;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED | D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    bd.StructureByteStride = 4;
    ComPtr<ID3D11Buffer> buf;
    if (FAILED(device->CreateBuffer(&bd, nullptr, buf.GetAddressOf()))) {
      return 0;
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC svd;
    svd.Format = DXGI_FORMAT_UNKNOWN;
    svd.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    svd.BufferEx.FirstElement = 0;
    svd.BufferEx.NumElements = 5;
    svd.BufferEx.Flags = 0;
    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(device->CreateShaderResourceView(buf.Get(), &svd, srv.GetAddressOf()))) {
      return 0;
    }
    D3D11_UNORDERED_ACCESS_VIEW_DESC uvd;
    uvd.Format = DXGI_FORMAT_UNKNOWN;
    uvd.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uvd.Buffer.FirstElement = 0;
    uvd.Buffer.NumElements = 5;
    uvd.Buffer.Flags = 0;
    ComPtr<ID3D11UnorderedAccessView> uav;
    if (FAILED(device->CreateUnorderedAccessView(buf.Get(), &uvd, uav.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    StructBuf sb;
    sb.buf = buf;
    sb.srv = srv;
    sb.uav = uav;
    sb.count = 5;
    sb.stride = 4;
    structBufs[id] = sb;
    return id;
  }
  void DrawIndexedIndirect(RHIStructBuffer argsBuf) override {
    auto it = structBufs.find(argsBuf);
    if (it == structBufs.end()) {
      return;
    }
    context->DrawIndexedInstancedIndirect(it->second.buf.Get(), 0);
  }
  void ClearStructBufferUint(RHIStructBuffer buf, uint32_t v) override {
    auto it = structBufs.find(buf);
    if (it == structBufs.end() || it->second.uav.Get() == nullptr) {
      return;
    }
    UINT values[4] = { v, v, v, v };
    context->ClearUnorderedAccessViewUint(it->second.uav.Get(), values);
  }
  bool UpdateStructBuffer(RHIStructBuffer buf, const void* data, size_t bytes) override {
    auto it = structBufs.find(buf);
    if (it == structBufs.end() || data == nullptr || bytes == 0) {
      return false;
    }
    size_t total = static_cast<size_t>(it->second.count) * it->second.stride;
    if (bytes > total) {
      bytes = total;
    }
    context->UpdateSubresource(it->second.buf.Get(), 0, nullptr, data, 0, 0);
    return true;
  }
  void SetInstanceBufferStruct(RHIStructBuffer buf) override {
    auto it = structBufs.find(buf);
    ID3D11Buffer* b = (it == structBufs.end()) ? nullptr : it->second.buf.Get();
    UINT stride = (it == structBufs.end() || it->second.stride == 0) ? 64 : it->second.stride;
    UINT offset = 0;
    context->IASetVertexBuffers(1, 1, &b, &stride, &offset);
  }
  RHITexture CreateTextureArray(int tw, int th, int slices, int mipLevels, RHITextureFormat fmt) override {
    if (tw <= 0 || th <= 0 || slices <= 0 || slices > 32 || mipLevels <= 0 || mipLevels > 16) {
      return 0;
    }
    D3D11_TEXTURE2D_DESC td;
    td.Width = static_cast<UINT>(tw);
    td.Height = static_cast<UINT>(th);
    td.MipLevels = static_cast<UINT>(mipLevels);
    td.ArraySize = static_cast<UINT>(slices);
    td.Format = ToTextureFormat(fmt);
    td.SampleDesc.Count = 1;
    td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = 0;
    td.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(device->CreateTexture2D(&td, nullptr, tex.GetAddressOf()))) {
      return 0;
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC sd;
    sd.Format = td.Format;
    sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    sd.Texture2DArray.MostDetailedMip = 0;
    sd.Texture2DArray.MipLevels = static_cast<UINT>(mipLevels);
    sd.Texture2DArray.FirstArraySlice = 0;
    sd.Texture2DArray.ArraySize = static_cast<UINT>(slices);
    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(device->CreateShaderResourceView(tex.Get(), &sd, srv.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    TexEntry entry;
    entry.tex = tex;
    entry.srv = srv;
    entry.w = tw;
    entry.h = th;
    entry.mips = mipLevels;
    entry.slices = slices;
    entry.fmt = fmt;
    textures[id] = entry;
    return id;
  }
  bool UpdateTextureArraySlice(RHITexture tex, int slice, int mip, int mw, int mh, uint32_t rowPitch, const void* data, size_t bytes) override {
    auto it = textures.find(tex);
    if (it == textures.end() || data == nullptr) {
      return false;
    }
    if (slice < 0 || slice >= it->second.slices) {
      return false;
    }
    if (!CheckTextureMipParams(it->second.w, it->second.h, it->second.mips, it->second.fmt, mip, mw, mh, rowPitch, bytes)) {
      return false;
    }
    UINT sub = D3D11CalcSubresource(static_cast<UINT>(mip), static_cast<UINT>(slice), static_cast<UINT>(it->second.mips));
    context->UpdateSubresource(it->second.tex.Get(), sub, nullptr, data, rowPitch, 0);
    return true;
  }
  bool UpdateTextureRegion(RHITexture tex, int mip, int x, int y, int w, int h, uint32_t rowPitch, const void* data, size_t bytes) override {
    auto it = textures.find(tex);
    if (it == textures.end() || data == nullptr) {
      return false;
    }
    if (mip < 0 || mip >= it->second.mips || x < 0 || y < 0 || w <= 0 || h <= 0) {
      return false;
    }
    int mw = it->second.w >> mip;
    int mh = it->second.h >> mip;
    if (mw < 1) {
      mw = 1;
    }
    if (mh < 1) {
      mh = 1;
    }
    if (x + w > mw || y + h > mh) {
      return false;
    }
    size_t need = static_cast<size_t>(rowPitch) * static_cast<size_t>(h);
    if (bytes < need) {
      return false;
    }
    UINT sub = D3D11CalcSubresource(static_cast<UINT>(mip), 0, static_cast<UINT>(it->second.mips));
    D3D11_BOX box;
    box.left = static_cast<UINT>(x);
    box.top = static_cast<UINT>(y);
    box.front = 0;
    box.right = static_cast<UINT>(x + w);
    box.bottom = static_cast<UINT>(y + h);
    box.back = 1;
    context->UpdateSubresource(it->second.tex.Get(), sub, &box, data, rowPitch, 0);
    it->second.resident.insert(mip);
    return true;
  }
  void SetPixelTexture(uint32_t slot, RHIRenderTarget rt) override {
    if (rt == 0) {
      ID3D11ShaderResourceView* nullSrv = nullptr;
      context->PSSetShaderResources(slot, 1, &nullSrv);
      return;
    }
    ID3D11ShaderResourceView* srv = nullptr;
    auto it = targets.find(rt);
    if (it != targets.end()) {
      srv = it->second.srv.Get();
    } else {
      auto cit = cubes.find(rt);
      if (cit != cubes.end()) {
        srv = cit->second.srv.Get();
      }
    }
    context->PSSetShaderResources(slot, 1, &srv);
  }
  void SetPixelTexture2D(uint32_t slot, RHITexture tex) override {
    ID3D11ShaderResourceView* srv = nullptr;
    if (tex != 0) {
      auto it = textures.find(tex);
      if (it != textures.end()) {
        srv = it->second.srv.Get();
      }
    }
    context->PSSetShaderResources(slot, 1, &srv);
  }
  void SetPixelTexture3D(uint32_t slot, RHITexture3D tex) override {
    ID3D11ShaderResourceView* srv = nullptr;
    if (tex != 0) {
      auto it = tex3ds.find(tex);
      if (it != tex3ds.end()) {
        srv = it->second.srv.Get();
      }
    }
    context->PSSetShaderResources(slot, 1, &srv);
  }
  RHISampler CreateSamplerLinear() override {
    D3D11_SAMPLER_DESC sd;
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MipLODBias = 0.0f;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.BorderColor[0] = 0.0f;
    sd.BorderColor[1] = 0.0f;
    sd.BorderColor[2] = 0.0f;
    sd.BorderColor[3] = 0.0f;
    sd.MinLOD = 0.0f;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;
    if (FAILED(device->CreateSamplerState(&sd, sampler.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    samplers[id] = sampler;
    return id;
  }
  RHISampler CreateSamplerPoint() override {
    D3D11_SAMPLER_DESC sd;
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MipLODBias = 0.0f;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.BorderColor[0] = 0.0f;
    sd.BorderColor[1] = 0.0f;
    sd.BorderColor[2] = 0.0f;
    sd.BorderColor[3] = 0.0f;
    sd.MinLOD = 0.0f;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;
    if (FAILED(device->CreateSamplerState(&sd, sampler.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    samplers[id] = sampler;
    return id;
  }
  RHISampler CreateSamplerWrap() override {
    D3D11_SAMPLER_DESC sd;
    sd.Filter = D3D11_FILTER_ANISOTROPIC;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MipLODBias = 0.0f;
    sd.MaxAnisotropy = 8;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.BorderColor[0] = 0.0f;
    sd.BorderColor[1] = 0.0f;
    sd.BorderColor[2] = 0.0f;
    sd.BorderColor[3] = 0.0f;
    sd.MinLOD = 0.0f;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;
    if (FAILED(device->CreateSamplerState(&sd, sampler.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    samplers[id] = sampler;
    return id;
  }
  RHIRenderTarget CreateShadowCube(int size) override {
    if (size <= 0) {
      return 0;
    }
    ShadowCubeRes res;
    res.size = size;
    D3D11_TEXTURE2D_DESC td;
    td.Width = static_cast<UINT>(size);
    td.Height = static_cast<UINT>(size);
    td.MipLevels = 1;
    td.ArraySize = 6;
    td.Format = DXGI_FORMAT_R32_TYPELESS;
    td.SampleDesc.Count = 1;
    td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = 0;
    td.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
    if (FAILED(device->CreateTexture2D(&td, nullptr, res.tex.GetAddressOf()))) {
      return 0;
    }
    for (int f = 0; f < 6; ++f) {
      D3D11_DEPTH_STENCIL_VIEW_DESC dd;
      dd.Format = DXGI_FORMAT_D32_FLOAT;
      dd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
      dd.Flags = 0;
      dd.Texture2DArray.MipSlice = 0;
      dd.Texture2DArray.FirstArraySlice = static_cast<UINT>(f);
      dd.Texture2DArray.ArraySize = 1;
      if (FAILED(device->CreateDepthStencilView(res.tex.Get(), &dd, res.dsv[f].GetAddressOf()))) {
        return 0;
      }
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC sd;
    sd.Format = DXGI_FORMAT_R32_FLOAT;
    sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    sd.TextureCube.MostDetailedMip = 0;
    sd.TextureCube.MipLevels = 1;
    if (FAILED(device->CreateShaderResourceView(res.tex.Get(), &sd, res.srv.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    cubes[id] = res;
    return id;
  }
  void SetShadowCubeFace(RHIRenderTarget cube, int face) override {
    if (face < 0 || face > 5) {
      return;
    }
    auto it = cubes.find(cube);
    if (it == cubes.end() || it->second.dsv[face] == nullptr) {
      return;
    }
    if (lastCube == cube && lastCubeFace == face) {
      return;
    }
    lastCube = cube;
    lastCubeFace = face;
    curRTs.clear();
    curDepth = 0;
    context->OMSetRenderTargets(0, nullptr, it->second.dsv[face].Get());
  }
  RHISampler CreateSamplerShadow() override {
    D3D11_SAMPLER_DESC sd;
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    sd.MipLODBias = 0.0f;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.BorderColor[0] = 1.0f;
    sd.BorderColor[1] = 1.0f;
    sd.BorderColor[2] = 1.0f;
    sd.BorderColor[3] = 1.0f;
    sd.MinLOD = 0.0f;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    ComPtr<ID3D11SamplerState> sampler;
    if (FAILED(device->CreateSamplerState(&sd, sampler.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    samplers[id] = sampler;
    return id;
  }
  void SetPixelSampler(uint32_t slot, RHISampler sampler) override {
    auto it = samplers.find(sampler);
    ID3D11SamplerState* s = (it == samplers.end()) ? nullptr : it->second.Get();
    context->PSSetSamplers(slot, 1, &s);
  }
  RHITexture CreateTexture2D(int tw, int th, int mipLevels, RHITextureFormat fmt) override {
    if (tw <= 0 || th <= 0 || mipLevels <= 0 || mipLevels > 16) {
      return 0;
    }
    D3D11_TEXTURE2D_DESC td;
    td.Width = static_cast<UINT>(tw);
    td.Height = static_cast<UINT>(th);
    td.MipLevels = static_cast<UINT>(mipLevels);
    td.ArraySize = 1;
    td.Format = ToTextureFormat(fmt);
    td.SampleDesc.Count = 1;
    td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = 0;
    td.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(device->CreateTexture2D(&td, nullptr, tex.GetAddressOf()))) {
      return 0;
    }
    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(device->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    TexEntry entry;
    entry.tex = tex;
    entry.srv = srv;
    entry.w = tw;
    entry.h = th;
    entry.mips = mipLevels;
    entry.fmt = fmt;
    textures[id] = entry;
    return id;
  }
  bool UpdateTextureMip(RHITexture tex, int mip, int mw, int mh, uint32_t rowPitch, const void* data, size_t bytes) override {
    auto it = textures.find(tex);
    if (it == textures.end() || data == nullptr) {
      return false;
    }
    if (!CheckTextureMipParams(it->second.w, it->second.h, it->second.mips, it->second.fmt, mip, mw, mh, rowPitch, bytes)) {
      return false;
    }
    UINT sub = D3D11CalcSubresource(static_cast<UINT>(mip), 0, static_cast<UINT>(it->second.mips));
    context->UpdateSubresource(it->second.tex.Get(), sub, nullptr, data, rowPitch, 0);
    it->second.resident.insert(mip);
    return true;
  }
  void DestroyTexture(RHITexture tex) override {
    auto it = textures.find(tex);
    if (it != textures.end()) {
      for (int f = 0; f < 6; ++f) {
        if (it->second.faceTargets[f] != 0) {
          targets.erase(it->second.faceTargets[f]);
        }
      }
    }
    textures.erase(tex);
  }
  RHITexture3D CreateTexture3D(int tw, int th, int td, RHITextureFormat fmt) override {
    if (tw <= 0 || th <= 0 || td <= 0 || tw > 64 || th > 64 || td > 64) {
      return 0;
    }
    D3D11_TEXTURE3D_DESC dd;
    dd.Width = static_cast<UINT>(tw);
    dd.Height = static_cast<UINT>(th);
    dd.Depth = static_cast<UINT>(td);
    dd.MipLevels = 1;
    dd.Format = ToTextureFormat(fmt);
    dd.Usage = D3D11_USAGE_DEFAULT;
    dd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    dd.CPUAccessFlags = 0;
    dd.MiscFlags = 0;
    ComPtr<ID3D11Texture3D> tex;
    if (FAILED(device->CreateTexture3D(&dd, nullptr, tex.GetAddressOf()))) {
      return 0;
    }
    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(device->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    Tex3DEntry entry;
    entry.tex = tex;
    entry.srv = srv;
    entry.w = tw;
    entry.h = th;
    entry.d = td;
    entry.fmt = fmt;
    tex3ds[id] = entry;
    return id;
  }
  bool UpdateTexture3D(RHITexture3D tex, int w, int h, int d, uint32_t rowPitch, uint32_t slicePitch, const void* data, size_t bytes) override {
    auto it = tex3ds.find(tex);
    if (it == tex3ds.end() || data == nullptr) {
      return false;
    }
    if (w != it->second.w || h != it->second.h || d != it->second.d) {
      return false;
    }
    size_t bpp = 4;
    if (it->second.fmt == RHITextureFormat::RGBA16F) {
      bpp = 8;
    }
    if (rowPitch != static_cast<uint32_t>(w) * bpp || slicePitch != rowPitch * static_cast<uint32_t>(h)) {
      return false;
    }
    if (bytes != static_cast<size_t>(slicePitch) * static_cast<size_t>(d)) {
      return false;
    }
    context->UpdateSubresource(it->second.tex.Get(), 0, nullptr, data, rowPitch, slicePitch);
    return true;
  }
  void DestroyTexture3D(RHITexture3D tex) override {
    tex3ds.erase(tex);
  }
  bool UpdateCubeFace(RHITexture cube, int face, int size, uint32_t rowPitch, const void* data, size_t bytes) override {
    auto it = textures.find(cube);
    if (it == textures.end() || !it->second.isCube || data == nullptr) {
      return false;
    }
    if (face < 0 || face > 5 || size != it->second.w || size != it->second.h) {
      return false;
    }
    size_t bpp = it->second.fmt == RHITextureFormat::RGBA16F ? 8 : 4;
    if (rowPitch != static_cast<uint32_t>(size) * bpp) {
      return false;
    }
    if (bytes != static_cast<size_t>(rowPitch) * static_cast<size_t>(size)) {
      return false;
    }
    int levels = 1;
    int s = size;
    while (s > 1) {
      s >>= 1;
      ++levels;
    }
    UINT sub = D3D11CalcSubresource(0, static_cast<UINT>(face), static_cast<UINT>(levels));
    context->UpdateSubresource(it->second.tex.Get(), sub, nullptr, data, rowPitch, 0);
    return true;
  }
  RHITexture CreateEnvCube(int size) override {
    if (size < 16 || size > 512) {
      return 0;
    }
    D3D11_TEXTURE2D_DESC td;
    td.Width = static_cast<UINT>(size);
    td.Height = static_cast<UINT>(size);
    td.MipLevels = 0;
    td.ArraySize = 6;
    td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    td.SampleDesc.Count = 1;
    td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = 0;
    td.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE | D3D11_RESOURCE_MISC_GENERATE_MIPS;
    ComPtr<ID3D11Texture2D> tex;
    if (FAILED(device->CreateTexture2D(&td, nullptr, tex.GetAddressOf()))) {
      return 0;
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC svd;
    svd.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    svd.TextureCube.MostDetailedMip = 0;
    svd.TextureCube.MipLevels = static_cast<UINT>(-1);
    ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(device->CreateShaderResourceView(tex.Get(), &svd, srv.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    TexEntry entry;
    entry.tex = tex;
    entry.srv = srv;
    entry.w = size;
    entry.h = size;
    entry.mips = 0;
    entry.fmt = RHITextureFormat::RGBA16F;
    entry.isCube = true;
    for (int f = 0; f < 6; ++f) {
      D3D11_RENDER_TARGET_VIEW_DESC rvd;
      rvd.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
      rvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
      rvd.Texture2DArray.MipSlice = 0;
      rvd.Texture2DArray.FirstArraySlice = static_cast<UINT>(f);
      rvd.Texture2DArray.ArraySize = 1;
      ComPtr<ID3D11RenderTargetView> rtv;
      if (FAILED(device->CreateRenderTargetView(tex.Get(), &rvd, rtv.GetAddressOf()))) {
        return 0;
      }
      entry.faceRTV[f] = rtv;
      TargetRes res;
      res.tex = tex;
      res.rtv = rtv;
      res.w = size;
      res.h = size;
      res.fmt = RHIFormat::RGBA16F;
      res.isDepth = false;
      uint64_t tid = nextId++;
      targets[tid] = res;
      entry.faceTargets[f] = tid;
    }
    textures[id] = entry;
    return id;
  }
  RHIRenderTarget GetEnvCubeFace(RHITexture cube, int face) override {
    if (face < 0 || face > 5) {
      return 0;
    }
    auto it = textures.find(cube);
    if (it == textures.end() || !it->second.isCube) {
      return 0;
    }
    return it->second.faceTargets[face];
  }
  void GenerateCubeMips(RHITexture cube) override {
    auto it = textures.find(cube);
    if (it == textures.end() || !it->second.isCube || it->second.srv == nullptr) {
      return;
    }
    context->GenerateMips(it->second.srv.Get());
  }
  int TextureResidentMips(RHITexture tex) const override {
    auto it = textures.find(tex);
    if (it == textures.end()) {
      return 0;
    }
    return static_cast<int>(it->second.resident.size());
  }
  void DrawIndexed(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex) override {
    context->DrawIndexed(indexCount, startIndex, baseVertex);
  }
  void DrawIndexedInstanced(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex, uint32_t instanceCount) override {
    context->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, 0);
  }
  void DrawFullscreenTriangle() override {
    context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    curVB = 0;
    curIB = 0;
    curLayout = 0;
    context->Draw(3, 0);
  }
  RHITimestamp CreateTimestamp() override {
    D3D11_QUERY_DESC qd;
    qd.MiscFlags = 0;
    Tstamp t;
    qd.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    if (FAILED(device->CreateQuery(&qd, t.disjoint.GetAddressOf()))) {
      return 0;
    }
    qd.Query = D3D11_QUERY_TIMESTAMP;
    if (FAILED(device->CreateQuery(&qd, t.begin.GetAddressOf()))) {
      return 0;
    }
    if (FAILED(device->CreateQuery(&qd, t.finish.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    tstamps[id] = t;
    return id;
  }
  void DestroyTimestamp(RHITimestamp ts) override {
    tstamps.erase(ts);
  }
  void BeginTimestamp(RHITimestamp ts) override {
    auto it = tstamps.find(ts);
    if (it == tstamps.end()) {
      return;
    }
    context->Begin(it->second.disjoint.Get());
    context->End(it->second.begin.Get());
  }
  void EndTimestamp(RHITimestamp ts) override {
    auto it = tstamps.find(ts);
    if (it == tstamps.end()) {
      return;
    }
    context->End(it->second.finish.Get());
    context->End(it->second.disjoint.Get());
  }
  bool GetTimestampMs(RHITimestamp ts, double* ms) override {
    if (ms == nullptr) {
      return false;
    }
    auto it = tstamps.find(ts);
    if (it == tstamps.end()) {
      return false;
    }
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj;
    if (context->GetData(it->second.disjoint.Get(), &dj, sizeof(dj), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) {
      return false;
    }
    if (dj.Disjoint != FALSE) {
      return false;
    }
    UINT64 t0 = 0;
    UINT64 t1 = 0;
    if (context->GetData(it->second.begin.Get(), &t0, sizeof(t0), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) {
      return false;
    }
    if (context->GetData(it->second.finish.Get(), &t1, sizeof(t1), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) {
      return false;
    }
    if (dj.Frequency == 0 || t1 < t0) {
      return false;
    }
    *ms = static_cast<double>(t1 - t0) * 1000.0 / static_cast<double>(dj.Frequency);
    return true;
  }
  void GetMemoryStats(uint64_t& textureBytes, uint64_t& targetBytes, uint64_t& bufferBytes) const override {    textureBytes = 0;
    for (const auto& kv : textures) {
      if (kv.second.isCube) {
        int levels = 1;
        int s = kv.second.w > kv.second.h ? kv.second.w : kv.second.h;
        while (s > 1) {
          s >>= 1;
          ++levels;
        }
        textureBytes += TextureBytes(kv.second.w, kv.second.h, levels, kv.second.fmt) * 6;
      } else {
        textureBytes += TextureBytes(kv.second.w, kv.second.h, kv.second.mips, kv.second.fmt);
      }
    }
    targetBytes = 0;
    for (const auto& kv : targets) {
      targetBytes += TargetBytes(kv.second.w, kv.second.h, kv.second.fmt);
    }
    for (const auto& p : pool) {
      targetBytes += TargetBytes(p.w, p.h, p.fmt);
    }
    for (const auto& kv : cubes) {
      targetBytes += static_cast<uint64_t>(kv.second.size) * static_cast<uint64_t>(kv.second.size) * 4 * 6;
    }
    bufferBytes = 0;
    for (const auto& kv : sizes) {
      bufferBytes += kv.second;
    }
    for (const auto& kv : csizes) {
      bufferBytes += kv.second;
    }
  }
  RHIOcclusionQuery CreateOcclusionQuery() override {
    D3D11_QUERY_DESC qd;
    qd.Query = D3D11_QUERY_OCCLUSION;
    qd.MiscFlags = 0;
    OccQuery o;
    if (FAILED(device->CreateQuery(&qd, o.query.GetAddressOf()))) {
      return 0;
    }
    uint64_t id = nextId++;
    occQueries[id] = o;
    return id;
  }
  void DestroyOcclusionQuery(RHIOcclusionQuery q) override {
    occQueries.erase(q);
  }
  void BeginOcclusionQuery(RHIOcclusionQuery q) override {
    auto it = occQueries.find(q);
    if (it == occQueries.end()) {
      return;
    }
    context->Begin(it->second.query.Get());
  }
  void EndOcclusionQuery(RHIOcclusionQuery q) override {
    auto it = occQueries.find(q);
    if (it == occQueries.end()) {
      return;
    }
    context->End(it->second.query.Get());
  }
  bool GetOcclusionSamples(RHIOcclusionQuery q, uint64_t* samples) override {
    if (samples == nullptr) {
      return false;
    }
    auto it = occQueries.find(q);
    if (it == occQueries.end()) {
      return false;
    }
    UINT64 data = 0;
    if (context->GetData(it->second.query.Get(), &data, sizeof(data), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) {
      return false;
    }
    *samples = static_cast<uint64_t>(data);
    return true;
  }
private:
  void Note(bool dup) {
    ++total;
    if (dup) {
      ++discarded;
    }
  }
  bool CreateBackbuffer() {
    ComPtr<ID3D11Texture2D> back;
    if (FAILED(swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(back.GetAddressOf())))) {
      return false;
    }
    if (FAILED(device->CreateRenderTargetView(back.Get(), nullptr, backRTV.GetAddressOf()))) {
      return false;
    }
    backId = nextId++;
    context->OMSetRenderTargets(1, backRTV.GetAddressOf(), nullptr);
    D3D11_VIEWPORT vp;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = static_cast<FLOAT>(w);
    vp.Height = static_cast<FLOAT>(h);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context->RSSetViewports(1, &vp);
    curRTs.clear();
    curRTs.push_back(backId);
    curDepth = 0;
    return true;
  }
  bool CompileShader(const char* path, const char* entry, const char* target, ComPtr<ID3DBlob>& outBlob) {
    if (path == nullptr || entry == nullptr) {
      return false;
    }
    wchar_t wpath[1024];
    int n = MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 1024);
    if (n == 0) {
      return false;
    }
    std::vector<unsigned char> srcBytes;
    HANDLE hf = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf != INVALID_HANDLE_VALUE) {
      LARGE_INTEGER fsize;
      fsize.QuadPart = 0;
      if (GetFileSizeEx(hf, &fsize) && fsize.QuadPart > 0 && fsize.QuadPart <= 4 * 1024 * 1024) {
        srcBytes.resize(static_cast<size_t>(fsize.QuadPart));
        DWORD got = 0;
        if (!ReadFile(hf, srcBytes.data(), static_cast<DWORD>(srcBytes.size()), &got, nullptr) || got != srcBytes.size()) {
          srcBytes.clear();
        }
      }
      CloseHandle(hf);
    }
    uint64_t key = 1469598103934665603ull;
    auto mixKey = [&key](const void* data, size_t bytes) {
      const unsigned char* p = reinterpret_cast<const unsigned char*>(data);
      for (size_t i = 0; i < bytes; ++i) {
        key ^= static_cast<uint64_t>(p[i]);
        key *= 1099511628211ull;
      }
    };
    mixKey(target, std::strlen(target));
    mixKey(entry, std::strlen(entry));
    if (!srcBytes.empty()) {
      mixKey(srcBytes.data(), srcBytes.size());
    } else {
      mixKey(path, std::strlen(path));
    }
    if (key == 0) {
      key = 1;
    }
    auto mc = shaderCache.find(key);
    if (mc != shaderCache.end() && !mc->second.empty() && mc->second.size() <= 4 * 1024 * 1024) {
      ComPtr<ID3DBlob> cached;
      if (SUCCEEDED(D3DCreateBlob(mc->second.size(), reinterpret_cast<ID3D10Blob**>(cached.GetAddressOf())))) {
        std::memcpy(cached->GetBufferPointer(), mc->second.data(), mc->second.size());
        outBlob = cached;
        ++shaderCacheHits;
        return true;
      }
      shaderCache.erase(mc);
    }
    wchar_t cachePath[1024];
    cachePath[0] = 0;
    DWORD tlen = GetTempPathW(1024, cachePath);
    if (tlen > 0 && tlen < 1000) {
      wchar_t fname[64];
      swprintf_s(fname, L"kizuri_sc_%016llX.kzb", key);
      wcscat_s(cachePath, L"kizuri_shadercache\\");
      CreateDirectoryW(cachePath, nullptr);
      wcscat_s(cachePath, fname);
      HANDLE cf = CreateFileW(cachePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (cf != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER csize;
        csize.QuadPart = 0;
        if (GetFileSizeEx(cf, &csize) && csize.QuadPart > 0 && csize.QuadPart <= 4 * 1024 * 1024) {
          std::vector<unsigned char> cachedBytes(static_cast<size_t>(csize.QuadPart));
          DWORD got = 0;
          if (ReadFile(cf, cachedBytes.data(), static_cast<DWORD>(cachedBytes.size()), &got, nullptr) && got == cachedBytes.size()) {
            ComPtr<ID3DBlob> cached;
            if (SUCCEEDED(D3DCreateBlob(cachedBytes.size(), reinterpret_cast<ID3D10Blob**>(cached.GetAddressOf())))) {
              std::memcpy(cached->GetBufferPointer(), cachedBytes.data(), cachedBytes.size());
              shaderCache[key] = cachedBytes;
              outBlob = cached;
              ++shaderCacheHits;
              CloseHandle(cf);
              return true;
            }
          }
        }
        CloseHandle(cf);
      }
    }
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> errors;
    HRESULT hr = D3DCompileFromFile(wpath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entry, target, flags, 0, blob.GetAddressOf(), errors.GetAddressOf());
    if (FAILED(hr)) {
      return false;
    }
    std::vector<unsigned char> compiled(static_cast<unsigned char*>(blob->GetBufferPointer()), static_cast<unsigned char*>(blob->GetBufferPointer()) + blob->GetBufferSize());
    shaderCache[key] = compiled;
    ++shaderCacheMiss;
    std::string permKey = std::string(target) + "|" + entry + "|" + path;
    ++shaderPermCounts[permKey];
    if (cachePath[0] != 0) {
      HANDLE wf = CreateFileW(cachePath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
      if (wf != INVALID_HANDLE_VALUE) {
        DWORD wrote = 0;
        WriteFile(wf, compiled.data(), static_cast<DWORD>(compiled.size()), &wrote, nullptr);
        CloseHandle(wf);
      }
    }
    outBlob = blob;
    return true;
  }
  HWND hwnd;
  int w;
  int h;
  uint64_t nextId;
  uint64_t total;
  uint64_t discarded;
  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  ComPtr<ID3DUserDefinedAnnotation> annotation;
  bool annotationTried = false;
  ComPtr<IDXGISwapChain> swapchain;
  ComPtr<ID3D11RenderTargetView> backRTV;
  ComPtr<ID3D11RenderTargetView> target;
  uint64_t backId = 0;
  std::unordered_map<uint64_t, ComPtr<ID3D11Buffer>> buffers;
  std::unordered_map<uint64_t, uint32_t> strides;
  std::unordered_map<uint64_t, uint64_t> sizes;
  std::unordered_map<uint64_t, ComPtr<ID3D11Buffer>> cbuffers;
  std::unordered_map<uint64_t, uint64_t> csizes;
  std::unordered_map<uint64_t, ComPtr<ID3D11VertexShader>> vsMap;
  std::unordered_map<uint64_t, ComPtr<ID3DBlob>> vsBlobs;
  std::unordered_map<uint64_t, ComPtr<ID3D11HullShader>> hsMap;
  std::unordered_map<uint64_t, ComPtr<ID3D11DomainShader>> dsMap;
  std::unordered_map<uint64_t, ComPtr<ID3D11PixelShader>> psMap;
  std::unordered_map<uint64_t, ComPtr<ID3D11ComputeShader>> csMap;
  struct StructBuf {
    ComPtr<ID3D11Buffer> buf;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11UnorderedAccessView> uav;
    uint32_t count = 0;
    uint32_t stride = 0;
  };
  std::unordered_map<uint64_t, StructBuf> structBufs;
  std::unordered_map<uint64_t, ComPtr<ID3D11InputLayout>> layouts;
  std::unordered_map<uint64_t, TargetRes> targets;
  std::list<TargetRes> pool;
  std::unordered_map<uint64_t, Tstamp> tstamps;
  std::unordered_map<uint64_t, ComPtr<ID3D11Query>> fences;
  std::unordered_map<uint64_t, std::vector<unsigned char>> shaderCache;
  std::unordered_map<std::string, uint32_t> shaderPermCounts;
  uint64_t shaderCacheHits = 0;
  uint64_t shaderCacheMiss = 0;
  uint64_t poolHits = 0;
  uint64_t poolMiss = 0;
  std::unordered_map<uint64_t, OccQuery> occQueries;
  std::unordered_map<uint64_t, ComPtr<ID3D11SamplerState>> samplers;
  std::unordered_map<uint64_t, ShadowCubeRes> cubes;
  uint64_t lastCube = 0;
  int lastCubeFace = -1;
  struct TexEntry {
    ComPtr<ID3D11Texture2D> tex;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11RenderTargetView> faceRTV[6];
    RHIRenderTarget faceTargets[6] = { 0, 0, 0, 0, 0, 0 };
    int w = 0;
    int h = 0;
    int mips = 0;
    int slices = 1;
    RHITextureFormat fmt = RHITextureFormat::RGBA8_UNORM;
    bool isCube = false;
    std::set<int> resident;
  };
  std::unordered_map<uint64_t, TexEntry> textures;
  struct Tex3DEntry {
    ComPtr<ID3D11Texture3D> tex;
    ComPtr<ID3D11ShaderResourceView> srv;
    int w = 0;
    int h = 0;
    int d = 0;
    RHITextureFormat fmt = RHITextureFormat::RGBA8_UNORM;
  };
  std::unordered_map<uint64_t, Tex3DEntry> tex3ds;
  uint64_t curVS;
  uint64_t curPS;
  uint64_t curLayout;
  uint64_t curVB;
  uint32_t curVBOffset;
  uint64_t curIB;
  RHIRasterizer curRS;
  RHIDepthStencil curDS;
  RHIBlend curBlend;
  RHIViewport curVP;
  D3D11_PRIMITIVE_TOPOLOGY curTopoD3D;
  ComPtr<ID3D11RasterizerState> rsCache;
  ComPtr<ID3D11DepthStencilState> dsCache;
  ComPtr<ID3D11BlendState> blendCache;
  std::vector<uint64_t> curRTs;
  uint64_t curDepth = 0;
  bool hasRS;
  bool hasDS;
  bool hasBlend;
  bool hasVP;
  bool hasTopo = false;
};
IRHI* CreateD3D11RHI() {
  return new D3D11RHI();
}
}
