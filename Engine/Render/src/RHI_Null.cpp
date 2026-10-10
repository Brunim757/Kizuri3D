#include "Kizuri/RHI.h"
#include <unordered_map>
#include <vector>
#include <list>
#include <cstring>
#include <set>
namespace Kizuri {
class NullRHI : public IRHI {
public:
  NullRHI()
    : w(0)
    , h(0)
    , nextId(1)
    , total(0)
    , discarded(0)
    , curVS(0)
    , curPS(0)
    , curLayout(0)
    , curVB(0)
    , curIB(0) {
    curRS.cull = RHICull::Back;
    curRS.fill = RHIFill::Solid;
    curRS.frontCCW = false;
    curDS.depthEnable = true;
    curDS.depthWrite = true;
    curBlend.enable = false;
    curTopo = RHITopology::TriangleList;
    curVP.x = 0.0f;
    curVP.y = 0.0f;
    curVP.w = 0.0f;
    curVP.h = 0.0f;
    curVP.minD = 0.0f;
    curVP.maxD = 1.0f;
  }
  bool Initialize(const RHIDesc& desc) override {
    w = desc.width;
    h = desc.height;
    return w > 0 && h > 0;
  }
  void Shutdown() override {
    buffers.clear();
    cbuffers.clear();
    targets.clear();
    pool.clear();
    w = 0;
    h = 0;
  }
  bool Resize(int nw, int nh) override {
    if (nw <= 0 || nh <= 0) {
      return false;
    }
    w = nw;
    h = nh;
    return true;
  }
  void Clear(float r, float g, float b, float a) override {
    (void)r;
    (void)g;
    (void)b;
    (void)a;
    Note(false);
  }
  void Present(bool vsync) override {
    (void)vsync;
  }
  const char* BackendName() const override {
    return "Null";
  }
  int Width() const override {
    return w;
  }
  int Height() const override {
    return h;
  }
  void* GetNativeDevice() const override {
    return nullptr;
  }
  void* GetNativeContext() const override {
    return nullptr;
  }
  void GetCacheStats(uint64_t& t, uint64_t& d) const override {
    t = total;
    d = discarded;
  }
  RHIBuffer CreateBuffer(uint64_t size, uint32_t stride, bool isIndex, const void* initialData) override {
    RHIBuffer id = CreateBufferEmpty(size, stride, isIndex);
    if (id != 0 && initialData != nullptr) {
      UpdateBufferRange(id, 0, initialData, static_cast<size_t>(size));
    }
    return id;
  }
  RHIBuffer CreateBufferEmpty(uint64_t size, uint32_t stride, bool isIndex) override {
    (void)stride;
    (void)isIndex;
    if (size == 0) {
      return 0;
    }
    uint64_t id = nextId++;
    NullBuffer b;
    b.bytes.assign(static_cast<size_t>(size), 0);
    buffers[id] = b;
    return id;
  }
  bool UpdateBufferRange(RHIBuffer buf, uint64_t offset, const void* data, size_t bytes) override {
    auto it = buffers.find(buf);
    if (it == buffers.end() || data == nullptr || bytes == 0) {
      return false;
    }
    if (offset + bytes > it->second.bytes.size()) {
      return false;
    }
    std::memcpy(it->second.bytes.data() + static_cast<size_t>(offset), data, bytes);
    return true;
  }
  bool UpdateBufferDiscard(RHIBuffer buf, const void* data, size_t bytes) override {
    auto it = buffers.find(buf);
    if (it == buffers.end() || data == nullptr || bytes == 0) {
      return false;
    }
    if (bytes > it->second.bytes.size()) {
      return false;
    }
    std::memcpy(it->second.bytes.data(), data, bytes);
    return true;
  }
  void DestroyBuffer(RHIBuffer buf) override {
    buffers.erase(buf);
  }
  void SetVertexBuffer(RHIBuffer buf, uint32_t offset) override {
    (void)offset;
    if (curVB == buf) {
      Note(true);
      return;
    }
    Note(false);
    curVB = buf;
  }
  void SetIndexBuffer(RHIBuffer buf) override {
    if (curIB == buf) {
      Note(true);
      return;
    }
    Note(false);
    curIB = buf;
  }
  RHIConstBuffer CreateConstantBuffer(uint64_t size, const void* initialData) override {
    if (size == 0) {
      return 0;
    }
    uint64_t id = nextId++;
    std::vector<unsigned char> mem(static_cast<size_t>(size), 0);
    if (initialData != nullptr) {
      std::memcpy(mem.data(), initialData, static_cast<size_t>(size));
    }
    cbuffers[id] = mem;
    return id;
  }
  void UpdateConstantBuffer(RHIConstBuffer buf, const void* data, uint64_t size) override {
    auto it = cbuffers.find(buf);
    if (it == cbuffers.end() || data == nullptr) {
      return;
    }
    size_t n = static_cast<size_t>(size);
    if (n > it->second.size()) {
      n = it->second.size();
    }
    std::memcpy(it->second.data(), data, n);
  }
  void DestroyConstantBuffer(RHIConstBuffer buf) override {
    cbuffers.erase(buf);
  }
  void SetVertexConstantBuffer(uint32_t slot, RHIConstBuffer buf) override {
    (void)slot;
    (void)buf;
    Note(false);
  }
  void SetPixelConstantBuffer(uint32_t slot, RHIConstBuffer buf) override {
    (void)slot;
    (void)buf;
    Note(false);
  }
  RHIVertexShader CreateVertexShaderFromFile(const char* path, const char* entry) override {
    if (path == nullptr || entry == nullptr || path[0] == '\0') {
      return 0;
    }
    uint64_t id = nextId++;
    shaders[id] = 1;
    return id;
  }
  RHIPixelShader CreatePixelShaderFromFile(const char* path, const char* entry) override {
    if (path == nullptr || entry == nullptr || path[0] == '\0') {
      return 0;
    }
    uint64_t id = nextId++;
    shaders[id] = 1;
    return id;
  }
  void SetVertexShader(RHIVertexShader vs) override {
    if (curVS == vs) {
      Note(true);
      return;
    }
    Note(false);
    curVS = vs;
  }
  RHIHullShader CreateHullShaderFromFile(const char* path, const char* entry) override {
    if (path == nullptr || entry == nullptr || path[0] == '\0') {
      return 0;
    }
    uint64_t id = nextId++;
    shaders[id] = 1;
    return id;
  }
  RHIDomainShader CreateDomainShaderFromFile(const char* path, const char* entry) override {
    if (path == nullptr || entry == nullptr || path[0] == '\0') {
      return 0;
    }
    uint64_t id = nextId++;
    shaders[id] = 1;
    return id;
  }
  void SetHullShader(RHIHullShader hs) override {
    (void)hs;
  }
  void SetDomainShader(RHIDomainShader ds) override {
    (void)ds;
  }
  void SetHullConstantBuffer(uint32_t slot, RHIConstBuffer cb) override {
    (void)slot;
    (void)cb;
  }
  void SetDomainConstantBuffer(uint32_t slot, RHIConstBuffer cb) override {
    (void)slot;
    (void)cb;
  }
  void SetDomainTexture(uint32_t slot, RHITexture tex) override {
    (void)slot;
    (void)tex;
  }
  void SetDomainSampler(uint32_t slot, RHISampler sampler) override {
    (void)slot;
    (void)sampler;
  }
  void SetPixelShader(RHIPixelShader ps) override {
    if (curPS == ps) {
      Note(true);
      return;
    }
    Note(false);
    curPS = ps;
  }
  RHIInputLayout CreateInputLayoutPNUT(RHIVertexShader vs) override {
    if (vs == 0) {
      return 0;
    }
    uint64_t id = nextId++;
    shaders[id] = 1;
    return id;
  }
  RHIInputLayout CreateInputLayoutPNUTInstanced(RHIVertexShader vs) override {
    if (vs == 0) {
      return 0;
    }
    uint64_t id = nextId++;
    shaders[id] = 1;
    return id;
  }
  void SetInstanceBuffer(RHIBuffer buf) override {
    (void)buf;
    Note(false);
  }
  void SetInputLayout(RHIInputLayout layout) override {
    if (curLayout == layout) {
      Note(true);
      return;
    }
    Note(false);
    curLayout = layout;
  }
  void SetViewport(const RHIViewport& vp) override {
    if (curVP.x == vp.x && curVP.y == vp.y && curVP.w == vp.w && curVP.h == vp.h && curVP.minD == vp.minD && curVP.maxD == vp.maxD) {
      Note(true);
      return;
    }
    Note(false);
    curVP = vp;
  }
  void SetTopology(RHITopology topo) override {
    if (curTopo == topo) {
      Note(true);
      return;
    }
    Note(false);
    curTopo = topo;
  }
  void SetRasterizerState(const RHIRasterizer& rs) override {
    if (curRS.cull == rs.cull && curRS.fill == rs.fill && curRS.frontCCW == rs.frontCCW) {
      Note(true);
      return;
    }
    Note(false);
    curRS = rs;
  }
  void SetDepthStencilState(const RHIDepthStencil& ds) override {
    if (curDS.depthEnable == ds.depthEnable && curDS.depthWrite == ds.depthWrite) {
      Note(true);
      return;
    }
    Note(false);
    curDS = ds;
  }
  void SetBlendState(const RHIBlend& blend) override {
    if (curBlend.enable == blend.enable) {
      Note(true);
      return;
    }
    Note(false);
    curBlend = blend;
  }
  RHIRenderTarget CreateRenderTarget(int tw, int th, RHIFormat fmt) override {
    if (tw <= 0 || th <= 0) {
      return 0;
    }
    for (auto it = pool.begin(); it != pool.end(); ++it) {
      if (it->w == tw && it->h == th && it->fmt == fmt) {
        NullTarget t = *it;
        pool.erase(it);
        uint64_t id = nextId++;
        targets[id] = t;
        return id;
      }
    }
    uint64_t id = nextId++;
    NullTarget t;
    t.w = tw;
    t.h = th;
    t.fmt = fmt;
    targets[id] = t;
    return id;
  }
  void DestroyRenderTarget(RHIRenderTarget rt) override {
    auto it = targets.find(rt);
    if (it == targets.end()) {
      return;
    }
    if (pool.size() < 32 && it->second.w <= 2048 && it->second.h <= 2048) {
      pool.push_back(it->second);
    }
    targets.erase(it);
  }
  void SetRenderTargets(uint32_t count, const RHIRenderTarget* colorRTs, RHIRenderTarget depthRT) override {
    (void)count;
    (void)colorRTs;
    (void)depthRT;
    Note(false);
  }
  void ClearRenderTarget(RHIRenderTarget rt, float r, float g, float b, float a) override {
    (void)rt;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
  }
  void ClearDepth(RHIRenderTarget depthRT) override {
    (void)depthRT;
  }
  void* GetRenderTargetSRV(RHIRenderTarget rt) const override {
    (void)rt;
    return nullptr;
  }
  void* GetTextureSRV(RHITexture tex) const override {
    (void)tex;
    return nullptr;
  }
  void BindBackbuffer() override {
    Note(false);
  }
  void SetPixelTexture(uint32_t slot, RHIRenderTarget rt) override {
    (void)slot;
    (void)rt;
    Note(false);
  }
  void BeginEvent(const char* name) override {
    (void)name;
  }
  void EndEvent() override {
  }
  RHIComputeShader CreateComputeShaderFromFile(const char* path, const char* entry) override {
    (void)path;
    (void)entry;
    uint64_t id = nextId++;
    shaders[id] = 1;
    return id;
  }
  RHIStructBuffer CreateStructBuffer(uint32_t count, uint32_t stride, const void* initData) override {
    (void)count;
    (void)stride;
    (void)initData;
    uint64_t id = nextId++;
    shaders[id] = 1;
    return id;
  }
  void DestroyStructBuffer(RHIStructBuffer buf) override {
    (void)buf;
  }
  void SetComputeShader(RHIComputeShader cs) override {
    (void)cs;
  }
  void SetComputeConstantBuffer(uint32_t slot, RHIConstBuffer cb) override {
    (void)slot;
    (void)cb;
  }
  void SetComputeSRV(uint32_t slot, RHIStructBuffer buf) override {
    (void)slot;
    (void)buf;
  }
  void SetComputeUAV(uint32_t slot, RHIStructBuffer buf) override {
    (void)slot;
    (void)buf;
  }
  void Dispatch(uint32_t x, uint32_t y, uint32_t z) override {
    (void)x;
    (void)y;
    (void)z;
  }
  bool ReadStructBuffer(RHIStructBuffer buf, void* dst, size_t bytes) override {
    (void)buf;
    if (dst == nullptr || bytes == 0) {
      return false;
    }
    std::memset(dst, 0, bytes);
    return true;
  }
  void SetPixelTexture2D(uint32_t slot, RHITexture tex) override {
    (void)slot;
    (void)tex;
    Note(false);
  }
  void SetPixelTexture3D(uint32_t slot, RHITexture3D tex) override {
    (void)slot;
    (void)tex;
    Note(false);
  }
  RHISampler CreateSamplerLinear() override {
    uint64_t id = nextId++;
    shaders[id] = 1;
    return id;
  }
  RHISampler CreateSamplerPoint() override {
    uint64_t id = nextId++;
    shaders[id] = 1;
    return id;
  }
  RHISampler CreateSamplerWrap() override {
    uint64_t id = nextId++;
    shaders[id] = 1;
    return id;
  }
  void SetPixelSampler(uint32_t slot, RHISampler sampler) override {
    (void)slot;
    (void)sampler;
    Note(false);
  }
  RHIRenderTarget CreateShadowCube(int size) override {
    if (size <= 0) {
      return 0;
    }
    uint64_t id = nextId++;
    NullTarget t;
    t.w = size;
    t.h = size;
    t.fmt = RHIFormat::R32_DEPTH;
    targets[id] = t;
    return id;
  }
  void SetShadowCubeFace(RHIRenderTarget cube, int face) override {
    (void)cube;
    (void)face;
    Note(false);
  }
  RHISampler CreateSamplerShadow() override {
    uint64_t id = nextId++;
    shaders[id] = 1;
    return id;
  }
  RHITexture CreateTexture2D(int tw, int th, int mipLevels, RHITextureFormat fmt) override {
    if (tw <= 0 || th <= 0 || mipLevels <= 0 || mipLevels > 16) {
      return 0;
    }
    uint64_t id = nextId++;
    NullTexture t;
    t.w = tw;
    t.h = th;
    t.mips = mipLevels;
    t.fmt = fmt;
    textures[id] = t;
    return id;
  }
  bool UpdateTextureMip(RHITexture tex, int mip, int mw, int mh, uint32_t rowPitch, const void* data, size_t bytes) override {
    auto it = textures.find(tex);
    if (it == textures.end() || data == nullptr) {
      return false;
    }
    if (!CheckMip(it->second, mip, mw, mh, rowPitch, bytes)) {
      return false;
    }
    it->second.resident.insert(mip);
    return true;
  }
  void DestroyTexture(RHITexture tex) override {
    textures.erase(tex);
  }
  int TextureResidentMips(RHITexture tex) const override {
    auto it = textures.find(tex);
    if (it == textures.end()) {
      return 0;
    }
    return static_cast<int>(it->second.resident.size());
  }
  RHITexture CreateEnvCube(int size) override {
    if (size <= 0) {
      return 0;
    }
    uint64_t id = nextId++;
    NullTexture t;
    t.w = size;
    t.h = size;
    t.mips = 0;
    t.fmt = RHITextureFormat::RGBA16F;
    t.isCube = true;
    textures[id] = t;
    return id;
  }
  RHIRenderTarget GetEnvCubeFace(RHITexture cube, int face) override {
    if (face < 0 || face > 5 || textures.find(cube) == textures.end()) {
      return 0;
    }
    uint64_t id = nextId++;
    shaders[id] = 1;
    return id;
  }
  void GenerateCubeMips(RHITexture cube) override {
    (void)cube;
    Note(false);
  }
  void DrawIndexed(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex) override {
    (void)indexCount;
    (void)startIndex;
    (void)baseVertex;
  }
  void DrawIndexedInstanced(uint32_t indexCount, uint32_t startIndex, int32_t baseVertex, uint32_t instanceCount) override {
    (void)indexCount;
    (void)startIndex;
    (void)baseVertex;
    (void)instanceCount;
  }
  void DrawFullscreenTriangle() override {
  }
  RHITimestamp CreateTimestamp() override {
    uint64_t id = nextId++;
    tstamps[id] = 1;
    return id;
  }
  void DestroyTimestamp(RHITimestamp ts) override {
    tstamps.erase(ts);
  }
  void BeginTimestamp(RHITimestamp ts) override {
    (void)ts;
  }
  void EndTimestamp(RHITimestamp ts) override {
    (void)ts;
  }
  bool GetTimestampMs(RHITimestamp ts, double* ms) override {
    (void)ts;
    (void)ms;
    return false;
  }
  RHIOcclusionQuery CreateOcclusionQuery() override {
    uint64_t id = nextId++;
    tstamps[id] = 1;
    return id;
  }
  void DestroyOcclusionQuery(RHIOcclusionQuery q) override {
    tstamps.erase(q);
  }
  void BeginOcclusionQuery(RHIOcclusionQuery q) override {
    (void)q;
  }
  void EndOcclusionQuery(RHIOcclusionQuery q) override {
    (void)q;
  }
  bool GetOcclusionSamples(RHIOcclusionQuery q, uint64_t* samples) override {
    (void)q;
    (void)samples;
    return false;
  }
  RHITexture3D CreateTexture3D(int tw, int th, int td, RHITextureFormat fmt) override {
    if (tw <= 0 || th <= 0 || td <= 0 || tw > 64 || th > 64 || td > 64) {
      return 0;
    }
    uint64_t id = nextId++;
    NullTexture3D t;
    t.w = tw;
    t.h = th;
    t.d = td;
    t.fmt = fmt;
    tex3ds[id] = t;
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
    it->second.filled = true;
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
    bufferBytes = 0;
    for (const auto& kv : buffers) {
      bufferBytes += static_cast<uint64_t>(kv.second.bytes.size());
    }
    for (const auto& kv : cbuffers) {
      bufferBytes += static_cast<uint64_t>(kv.second.size());
    }
  }
private:
  void Note(bool dup) {
    ++total;
    if (dup) {
      ++discarded;
    }
  }
  int w;
  int h;
  uint64_t nextId;
  uint64_t total;
  uint64_t discarded;
  struct NullBuffer {
    std::vector<unsigned char> bytes;
  };
  std::unordered_map<uint64_t, NullBuffer> buffers;
  std::unordered_map<uint64_t, std::vector<unsigned char>> cbuffers;
  struct NullTarget {
    int w = 0;
    int h = 0;
    RHIFormat fmt = RHIFormat::RGBA8_UNORM;
  };
  std::unordered_map<uint64_t, NullTarget> targets;
  std::list<NullTarget> pool;
  struct NullTexture3D {
    int w = 0;
    int h = 0;
    int d = 0;
    RHITextureFormat fmt = RHITextureFormat::RGBA8_UNORM;
    bool filled = false;
  };
  std::unordered_map<uint64_t, NullTexture3D> tex3ds;
  std::unordered_map<uint64_t, int> shaders;
  std::unordered_map<uint64_t, int> tstamps;
  struct NullTexture {
    int w = 0;
    int h = 0;
    int mips = 0;
    RHITextureFormat fmt = RHITextureFormat::RGBA8_UNORM;
    bool isCube = false;
    std::set<int> resident;
  };
  std::unordered_map<uint64_t, NullTexture> textures;
  static uint64_t TargetBytes(int tw, int th, RHIFormat fmt) {
    uint64_t bpp = 4;
    if (fmt == RHIFormat::RGBA16F) {
      bpp = 8;
    }
    return static_cast<uint64_t>(tw) * static_cast<uint64_t>(th) * bpp;
  }
  static uint64_t TextureBytes(int tw, int th, int mips, RHITextureFormat fmt) {
    uint64_t total = 0;
    int mw = tw;
    int mh = th;
    for (int m = 0; m < mips; ++m) {
      if (fmt == RHITextureFormat::RGBA8_UNORM || fmt == RHITextureFormat::RGBA8_UNORM_SRGB) {
        total += static_cast<uint64_t>(mw) * static_cast<uint64_t>(mh) * 4;
      } else if (fmt == RHITextureFormat::RGBA16F) {
        total += static_cast<uint64_t>(mw) * static_cast<uint64_t>(mh) * 8;
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
  static bool CheckMip(const NullTexture& t, int mip, int mw, int mh, uint32_t rowPitch, size_t bytes) {    if (mip < 0 || mip >= t.mips) {
      return false;
    }
    int ew = t.w >> mip;
    int eh = t.h >> mip;
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
    if (t.fmt == RHITextureFormat::RGBA8_UNORM || t.fmt == RHITextureFormat::RGBA8_UNORM_SRGB) {
      if (rowPitch != static_cast<uint32_t>(mw * 4)) {
        return false;
      }
      expect = static_cast<size_t>(rowPitch) * static_cast<size_t>(mh);
    } else if (t.fmt == RHITextureFormat::RGBA16F) {
      if (rowPitch != static_cast<uint32_t>(mw * 8)) {
        return false;
      }
      expect = static_cast<size_t>(rowPitch) * static_cast<size_t>(mh);
    } else {
      size_t blockBytes = 16;
      if (t.fmt == RHITextureFormat::BC1_UNORM || t.fmt == RHITextureFormat::BC1_UNORM_SRGB) {
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
  RHIVertexShader curVS;
  RHIPixelShader curPS;
  RHIInputLayout curLayout;
  RHIBuffer curVB;
  RHIBuffer curIB;
  RHIRasterizer curRS;
  RHIDepthStencil curDS;
  RHIBlend curBlend;
  RHITopology curTopo;
  RHIViewport curVP;
};
IRHI* CreateNullRHI() {
  return new NullRHI();
}
}
