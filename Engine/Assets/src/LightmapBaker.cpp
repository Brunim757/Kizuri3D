#include "Kizuri/Assets/LightmapBaker.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
namespace Kizuri {
namespace {
struct BakeTri {
  float v0[3];
  float v1[3];
  float v2[3];
  float n[3];
  float albedo[3];
};
struct BakeLight {
  int kind;
  float pos[3];
  float dir[3];
  float color[3];
  float range;
  float cosOuter;
  float coneSoft;
};
struct BvhNode {
  float bmin[3];
  float bmax[3];
  int left;
  int right;
  int start;
  int count;
};
bool RayTri(const float o[3], const float d[3], const BakeTri& t, float maxDist, float& distOut) {
  float e1[3] = { t.v1[0] - t.v0[0], t.v1[1] - t.v0[1], t.v1[2] - t.v0[2] };
  float e2[3] = { t.v2[0] - t.v0[0], t.v2[1] - t.v0[1], t.v2[2] - t.v0[2] };
  float p[3] = { d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0] };
  float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
  if (det > -1e-9f && det < 1e-9f) {
    return false;
  }
  float inv = 1.0f / det;
  float s[3] = { o[0] - t.v0[0], o[1] - t.v0[1], o[2] - t.v0[2] };
  float u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) * inv;
  if (u < -1e-6f || u > 1.0f + 1e-6f) {
    return false;
  }
  float q[3] = { s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0] };
  float v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv;
  if (v < -1e-6f || u + v > 1.0f + 1e-6f) {
    return false;
  }
  float dist = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
  if (dist < 1e-4f || dist > maxDist) {
    return false;
  }
  distOut = dist;
  return true;
}
bool RayBox(const float o[3], const float inv[3], const float bmin[3], const float bmax[3], float maxDist) {
  float t0 = 0.0f;
  float t1 = maxDist;
  for (int c = 0; c < 3; ++c) {
    float a = (bmin[c] - o[c]) * inv[c];
    float b = (bmax[c] - o[c]) * inv[c];
    if (a > b) {
      float t = a;
      a = b;
      b = t;
    }
    if (a > t0) {
      t0 = a;
    }
    if (b < t1) {
      t1 = b;
    }
    if (t0 > t1) {
      return false;
    }
  }
  return true;
}
struct Bvh {
  std::vector<BvhNode> nodes;
  std::vector<int> order;
  const std::vector<BakeTri>* tris;
};
int BvhBuild(Bvh& bvh, int start, int count, int depth) {
  int nodeIdx = static_cast<int>(bvh.nodes.size());
  bvh.nodes.push_back(BvhNode());
  BvhNode& node = bvh.nodes.back();
  node.bmin[0] = 1e30f;
  node.bmin[1] = 1e30f;
  node.bmin[2] = 1e30f;
  node.bmax[0] = -1e30f;
  node.bmax[1] = -1e30f;
  node.bmax[2] = -1e30f;
  for (int i = 0; i < count; ++i) {
    const BakeTri& t = (*bvh.tris)[static_cast<size_t>(bvh.order[static_cast<size_t>(start + i)])];
    const float* vs[3] = { t.v0, t.v1, t.v2 };
    for (int v = 0; v < 3; ++v) {
      for (int c = 0; c < 3; ++c) {
        if (vs[v][c] < node.bmin[c]) {
          node.bmin[c] = vs[v][c];
        }
        if (vs[v][c] > node.bmax[c]) {
          node.bmax[c] = vs[v][c];
        }
      }
    }
  }
  node.start = start;
  node.count = count;
  node.left = -1;
  node.right = -1;
  if (count <= 4 || depth > 32) {
    return nodeIdx;
  }
  float ext[3] = { node.bmax[0] - node.bmin[0], node.bmax[1] - node.bmin[1], node.bmax[2] - node.bmin[2] };
  int axis = 0;
  if (ext[1] > ext[0] && ext[1] > ext[2]) {
    axis = 1;
  } else if (ext[2] > ext[0] && ext[2] > ext[1]) {
    axis = 2;
  }
  int mid = start + count / 2;
  std::nth_element(bvh.order.begin() + start, bvh.order.begin() + mid, bvh.order.begin() + start + count,
    [&bvh, axis](int a, int b) {
      const BakeTri& ta = (*bvh.tris)[static_cast<size_t>(a)];
      const BakeTri& tb = (*bvh.tris)[static_cast<size_t>(b)];
      float ca = (ta.v0[axis] + ta.v1[axis] + ta.v2[axis]) / 3.0f;
      float cb = (tb.v0[axis] + tb.v1[axis] + tb.v2[axis]) / 3.0f;
      return ca < cb;
    });
  if (mid == start || mid == start + count) {
    return nodeIdx;
  }
  node.left = BvhBuild(bvh, start, mid - start, depth + 1);
  node.right = BvhBuild(bvh, mid, start + count - mid, depth + 1);
  node.start = -1;
  node.count = 0;
  return nodeIdx;
}
bool BvhOccluded(const Bvh& bvh, const float o[3], const float d[3], float maxDist, int ignoreFace) {
  float inv[3] = { 1.0f / d[0], 1.0f / d[1], 1.0f / d[2] };
  int stack[64];
  int sp = 0;
  stack[sp++] = 0;
  while (sp > 0) {
    const BvhNode& node = bvh.nodes[static_cast<size_t>(stack[--sp])];
    if (!RayBox(o, inv, node.bmin, node.bmax, maxDist)) {
      continue;
    }
    if (node.left < 0) {
      for (int i = 0; i < node.count; ++i) {
        int fi = bvh.order[static_cast<size_t>(node.start + i)];
        if (fi == ignoreFace) {
          continue;
        }
        float dist = 0.0f;
        if (RayTri(o, d, (*bvh.tris)[static_cast<size_t>(fi)], maxDist, dist)) {
          return true;
        }
      }
    } else {
      if (sp + 2 > 64) {
        return true;
      }
      stack[sp++] = node.left;
      stack[sp++] = node.right;
    }
  }
  return false;
}
bool BvhClosest(const Bvh& bvh, const float o[3], const float d[3], float maxDist, int& faceOut, float& distOut) {
  float inv[3] = { 1.0f / d[0], 1.0f / d[1], 1.0f / d[2] };
  bool hit = false;
  float best = maxDist;
  int stack[64];
  int sp = 0;
  stack[sp++] = 0;
  while (sp > 0) {
    const BvhNode& node = bvh.nodes[static_cast<size_t>(stack[--sp])];
    if (!RayBox(o, inv, node.bmin, node.bmax, best)) {
      continue;
    }
    if (node.left < 0) {
      for (int i = 0; i < node.count; ++i) {
        int fi = bvh.order[static_cast<size_t>(node.start + i)];
        float dist = 0.0f;
        if (RayTri(o, d, (*bvh.tris)[static_cast<size_t>(fi)], best, dist)) {
          best = dist;
          faceOut = fi;
          hit = true;
        }
      }
    } else {
      if (sp + 2 > 64) {
        continue;
      }
      stack[sp++] = node.left;
      stack[sp++] = node.right;
    }
  }
  distOut = best;
  return hit;
}
float WrapDiffuse(float ndotl) {
  float w = (ndotl + 0.5f) / 1.5f;
  if (w < 0.0f) {
    w = 0.0f;
  }
  if (w > 1.0f) {
    w = 1.0f;
  }
  return w * w;
}
void GatherLights(const Scene& scene, std::vector<BakeLight>& lights) {
  std::vector<EntityId> all = scene.All();
  for (size_t i = 0; i < all.size(); ++i) {
    const Entity* e = scene.Get(all[i]);
    if (e == nullptr || !e->hasLight) {
      continue;
    }
    BakeLight bl;
    bl.kind = e->light.type;
    if (bl.kind < 0 || bl.kind > 2) {
      bl.kind = 0;
    }
    bl.pos[0] = e->transform.position[0];
    bl.pos[1] = e->transform.position[1];
    bl.pos[2] = e->transform.position[2];
    float dir[3] = { 0, -1, 0 };
    EntityForward(e->transform, dir);
    bl.dir[0] = dir[0];
    bl.dir[1] = dir[1];
    bl.dir[2] = dir[2];
    bl.color[0] = e->light.color[0] * e->light.intensity;
    bl.color[1] = e->light.color[1] * e->light.intensity;
    bl.color[2] = e->light.color[2] * e->light.intensity;
    bl.range = e->light.radius < 0.5f ? 0.5f : e->light.radius;
    float ang = e->light.spotAngle * 0.01745329252f * 0.5f;
    float c = std::cos(ang);
    if (c < -1.0f) {
      c = -1.0f;
    }
    if (c > 1.0f) {
      c = 1.0f;
    }
    bl.cosOuter = c;
    bl.coneSoft = e->light.falloff < 0.01f ? 0.01f : e->light.falloff;
    lights.push_back(bl);
  }
}
void GatherTris(const Scene& scene, const AssetDatabase& assets, std::vector<BakeTri>& tris) {
  std::vector<EntityId> all = scene.All();
  for (size_t i = 0; i < all.size(); ++i) {
    const Entity* e = scene.Get(all[i]);
    if (e == nullptr || !e->hasMesh || e->meshGuid.empty()) {
      continue;
    }
    const MeshRecord* rec = assets.GetByGuid(e->meshGuid);
    if (rec == nullptr || !rec->loaded) {
      continue;
    }
    const MeshAssetData& md = rec->data;
    size_t vn = md.positions.size() / 3;
    if (vn == 0 || md.indices.size() % 3 != 0) {
      continue;
    }
    float world[16];
    ComposeMatrix(e->transform, world);
    float inv[16];
    if (!InvertMatrix4(world, inv)) {
      continue;
    }
    float nm[9] = { inv[0], inv[4], inv[8], inv[1], inv[5], inv[9], inv[2], inv[6], inv[10] };
    std::vector<int> triMat(md.indices.size() / 3, 0);
    for (size_t p = 0; p < md.parts.size(); ++p) {
      size_t b = md.parts[p].indexOffset / 3;
      size_t c = b + md.parts[p].indexCount / 3;
      if (b < triMat.size()) {
        if (c > triMat.size()) {
          c = triMat.size();
        }
        for (size_t t = b; t < c; ++t) {
          triMat[t] = static_cast<int>(md.parts[p].material);
        }
      }
    }
    for (size_t t = 0; t + 2 < md.indices.size(); t += 3) {
      uint32_t idx[3] = { md.indices[t], md.indices[t + 1], md.indices[t + 2] };
      if (idx[0] >= vn || idx[1] >= vn || idx[2] >= vn) {
        continue;
      }
      BakeTri bt;
      bool bad = false;
      for (int k = 0; k < 3; ++k) {
        float p[3] = { md.positions[idx[k] * 3 + 0], md.positions[idx[k] * 3 + 1], md.positions[idx[k] * 3 + 2] };
        float* dst = k == 0 ? bt.v0 : (k == 1 ? bt.v1 : bt.v2);
        dst[0] = world[0] * p[0] + world[4] * p[1] + world[8] * p[2] + world[12];
        dst[1] = world[1] * p[0] + world[5] * p[1] + world[9] * p[2] + world[13];
        dst[2] = world[2] * p[0] + world[6] * p[1] + world[10] * p[2] + world[14];
        float n[3] = { 0, 1, 0 };
        if (md.normals.size() == md.positions.size()) {
          n[0] = md.normals[idx[k] * 3 + 0];
          n[1] = md.normals[idx[k] * 3 + 1];
          n[2] = md.normals[idx[k] * 3 + 2];
        }
        float wn[3] = { nm[0] * n[0] + nm[1] * n[1] + nm[2] * n[2], nm[3] * n[0] + nm[4] * n[1] + nm[5] * n[2], nm[6] * n[0] + nm[7] * n[1] + nm[8] * n[2] };
        float l = std::sqrt(wn[0] * wn[0] + wn[1] * wn[1] + wn[2] * wn[2]);
        if (l < 1e-9f) {
          bad = true;
          break;
        }
        if (k == 0) {
          bt.n[0] = wn[0] / l;
          bt.n[1] = wn[1] / l;
          bt.n[2] = wn[2] / l;
        }
      }
      if (bad) {
        continue;
      }
      bt.albedo[0] = 1.0f;
      bt.albedo[1] = 1.0f;
      bt.albedo[2] = 1.0f;
      int mi = triMat[t / 3];
      if (mi >= 0 && static_cast<size_t>(mi) < md.materials.size()) {
        bt.albedo[0] = md.materials[static_cast<size_t>(mi)].albedo[0];
        bt.albedo[1] = md.materials[static_cast<size_t>(mi)].albedo[1];
        bt.albedo[2] = md.materials[static_cast<size_t>(mi)].albedo[2];
      }
      tris.push_back(bt);
    }
  }
}
void ShadeDirect(const Bvh& bvh, const std::vector<BakeLight>& lights, const float p[3], const float n[3], float irr[3]) {
  irr[0] = 0.0f;
  irr[1] = 0.0f;
  irr[2] = 0.0f;
  float o[3] = { p[0] + n[0] * 1e-3f, p[1] + n[1] * 1e-3f, p[2] + n[2] * 1e-3f };
  for (size_t li = 0; li < lights.size(); ++li) {
    const BakeLight& bl = lights[li];
    if (bl.kind == 2) {
      float L[3] = { -bl.dir[0], -bl.dir[1], -bl.dir[2] };
      float dl = std::sqrt(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]);
      if (dl < 1e-9f) {
        continue;
      }
      L[0] /= dl;
      L[1] /= dl;
      L[2] /= dl;
      if (BvhOccluded(bvh, o, L, 1e9f, -1)) {
        continue;
      }
      float w = WrapDiffuse(n[0] * L[0] + n[1] * L[1] + n[2] * L[2]);
      irr[0] += bl.color[0] * w;
      irr[1] += bl.color[1] * w;
      irr[2] += bl.color[2] * w;
    } else {
      float toL[3] = { bl.pos[0] - p[0], bl.pos[1] - p[1], bl.pos[2] - p[2] };
      float dist = std::sqrt(toL[0] * toL[0] + toL[1] * toL[1] + toL[2] * toL[2]);
      if (dist < 1e-6f || dist > bl.range) {
        continue;
      }
      float L[3] = { toL[0] / dist, toL[1] / dist, toL[2] / dist };
      float att = 1.0f - (dist * dist) / (bl.range * bl.range);
      att *= att;
      if (bl.kind == 1) {
        float cosT = L[0] * -bl.dir[0] + L[1] * -bl.dir[1] + L[2] * -bl.dir[2];
        if (cosT < bl.cosOuter) {
          continue;
        }
        float spot = (cosT - bl.cosOuter) / bl.coneSoft;
        if (spot > 1.0f) {
          spot = 1.0f;
        }
        if (spot <= 0.0f) {
          continue;
        }
        att *= spot;
      }
      if (BvhOccluded(bvh, o, L, dist - 1e-3f, -1)) {
        continue;
      }
      float w = WrapDiffuse(n[0] * L[0] + n[1] * L[1] + n[2] * L[2]);
      irr[0] += bl.color[0] * w * att;
      irr[1] += bl.color[1] * w * att;
      irr[2] += bl.color[2] * w * att;
    }
  }
}
}
bool BakeLightmapFor(Scene& scene, AssetDatabase& assets, EntityId target, int resolution, std::string& texPathOut, std::string& texGuidOut, std::string& errorOut) {
  texPathOut.clear();
  texGuidOut.clear();
  errorOut.clear();
  if (resolution < 16) {
    resolution = 16;
  }
  if (resolution > 512) {
    resolution = 512;
  }
  Entity* te = scene.Get(target);
  if (te == nullptr || !te->hasMesh || te->meshGuid.empty()) {
    errorOut = "No mesh on entity";
    return false;
  }
  const MeshRecord* trec = assets.GetByGuid(te->meshGuid);
  if (trec == nullptr || !trec->loaded) {
    errorOut = "Mesh not loaded";
    return false;
  }
  const MeshAssetData& tdata = trec->data;
  size_t tvn = tdata.positions.size() / 3;
  if (tvn == 0 || tdata.uv2s.size() != tvn * 2) {
    errorOut = "Mesh has no lightmap UVs";
    return false;
  }
  std::vector<BakeTri> tris;
  GatherTris(scene, assets, tris);
  if (tris.empty()) {
    errorOut = "No triangles in scene";
    return false;
  }
  std::vector<BakeLight> lights;
  GatherLights(scene, lights);
  Bvh bvh;
  bvh.tris = &tris;
  bvh.order.resize(tris.size());
  for (size_t i = 0; i < tris.size(); ++i) {
    bvh.order[i] = static_cast<int>(i);
  }
  BvhBuild(bvh, 0, static_cast<int>(tris.size()), 0);
  int res = resolution;
  size_t npix = static_cast<size_t>(res) * static_cast<size_t>(res);
  std::vector<float> direct(npix * 3, 0.0f);
  std::vector<float> texPos(npix * 3, 0.0f);
  std::vector<float> texNrm(npix * 3, 0.0f);
  std::vector<char> texValid(npix, 0);
  float tworld[16];
  ComposeMatrix(te->transform, tworld);
  float tinv[16];
  if (!InvertMatrix4(tworld, tinv)) {
    errorOut = "Bad transform";
    return false;
  }
  float tnm[9] = { tinv[0], tinv[4], tinv[8], tinv[1], tinv[5], tinv[9], tinv[2], tinv[6], tinv[10] };
  for (size_t t = 0; t + 2 < tdata.indices.size(); t += 3) {
    uint32_t idx[3] = { tdata.indices[t], tdata.indices[t + 1], tdata.indices[t + 2] };
    if (idx[0] >= tvn || idx[1] >= tvn || idx[2] >= tvn) {
      continue;
    }
    float uv[3][2] = { { tdata.uv2s[idx[0] * 2 + 0], tdata.uv2s[idx[0] * 2 + 1] }, { tdata.uv2s[idx[1] * 2 + 0], tdata.uv2s[idx[1] * 2 + 1] }, { tdata.uv2s[idx[2] * 2 + 0], tdata.uv2s[idx[2] * 2 + 1] } };
    float wp[3][3];
    float wn[3][3];
    for (int k = 0; k < 3; ++k) {
      float p[3] = { tdata.positions[idx[k] * 3 + 0], tdata.positions[idx[k] * 3 + 1], tdata.positions[idx[k] * 3 + 2] };
      wp[k][0] = tworld[0] * p[0] + tworld[4] * p[1] + tworld[8] * p[2] + tworld[12];
      wp[k][1] = tworld[1] * p[0] + tworld[5] * p[1] + tworld[9] * p[2] + tworld[13];
      wp[k][2] = tworld[2] * p[0] + tworld[6] * p[1] + tworld[10] * p[2] + tworld[14];
      float n[3] = { 0, 1, 0 };
      if (tdata.normals.size() == tdata.positions.size()) {
        n[0] = tdata.normals[idx[k] * 3 + 0];
        n[1] = tdata.normals[idx[k] * 3 + 1];
        n[2] = tdata.normals[idx[k] * 3 + 2];
      }
      wn[k][0] = tnm[0] * n[0] + tnm[1] * n[1] + tnm[2] * n[2];
      wn[k][1] = tnm[3] * n[0] + tnm[4] * n[1] + tnm[5] * n[2];
      wn[k][2] = tnm[6] * n[0] + tnm[7] * n[1] + tnm[8] * n[2];
      float l = std::sqrt(wn[k][0] * wn[k][0] + wn[k][1] * wn[k][1] + wn[k][2] * wn[k][2]);
      if (l > 1e-9f) {
        wn[k][0] /= l;
        wn[k][1] /= l;
        wn[k][2] /= l;
      }
    }
    float minu = uv[0][0];
    float maxu = uv[0][0];
    float minv = uv[0][1];
    float maxv = uv[0][1];
    for (int k = 1; k < 3; ++k) {
      if (uv[k][0] < minu) minu = uv[k][0];
      if (uv[k][0] > maxu) maxu = uv[k][0];
      if (uv[k][1] < minv) minv = uv[k][1];
      if (uv[k][1] > maxv) maxv = uv[k][1];
    }
    int x0 = static_cast<int>(minu * res);
    int x1 = static_cast<int>(maxu * res);
    int y0 = static_cast<int>(minv * res);
    int y1 = static_cast<int>(maxv * res);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= res) x1 = res - 1;
    if (y1 >= res) y1 = res - 1;
    float denom = (uv[1][1] - uv[2][1]) * (uv[0][0] - uv[2][0]) + (uv[2][0] - uv[1][0]) * (uv[0][1] - uv[2][1]);
    if (denom > -1e-9f && denom < 1e-9f) {
      continue;
    }
    for (int y = y0; y <= y1; ++y) {
      for (int x = x0; x <= x1; ++x) {
        float pu = (static_cast<float>(x) + 0.5f) / static_cast<float>(res);
        float pv = (static_cast<float>(y) + 0.5f) / static_cast<float>(res);
        float b0 = ((uv[1][1] - uv[2][1]) * (pu - uv[2][0]) + (uv[2][0] - uv[1][0]) * (pv - uv[2][1])) / denom;
        float b1 = ((uv[2][1] - uv[0][1]) * (pu - uv[2][0]) + (uv[0][0] - uv[2][0]) * (pv - uv[2][1])) / denom;
        float b2 = 1.0f - b0 - b1;
        if (b0 < -1e-4f || b1 < -1e-4f || b2 < -1e-4f) {
          continue;
        }
        size_t px = static_cast<size_t>(y) * static_cast<size_t>(res) + static_cast<size_t>(x);
        texValid[px] = 1;
        for (int c = 0; c < 3; ++c) {
          texPos[px * 3 + c] = b0 * wp[0][c] + b1 * wp[1][c] + b2 * wp[2][c];
          texNrm[px * 3 + c] = b0 * wn[0][c] + b1 * wn[1][c] + b2 * wn[2][c];
        }
        float nl = std::sqrt(texNrm[px * 3 + 0] * texNrm[px * 3 + 0] + texNrm[px * 3 + 1] * texNrm[px * 3 + 1] + texNrm[px * 3 + 2] * texNrm[px * 3 + 2]);
        if (nl > 1e-9f) {
          texNrm[px * 3 + 0] /= nl;
          texNrm[px * 3 + 1] /= nl;
          texNrm[px * 3 + 2] /= nl;
        }
      }
    }
  }
  for (size_t px = 0; px < npix; ++px) {
    if (!texValid[px]) {
      continue;
    }
    float p[3] = { texPos[px * 3 + 0], texPos[px * 3 + 1], texPos[px * 3 + 2] };
    float n[3] = { texNrm[px * 3 + 0], texNrm[px * 3 + 1], texNrm[px * 3 + 2] };
    float irr[3];
    ShadeDirect(bvh, lights, p, n, irr);
    direct[px * 3 + 0] = irr[0];
    direct[px * 3 + 1] = irr[1];
    direct[px * 3 + 2] = irr[2];
  }
  float avgDirect[3] = { 0, 0, 0 };
  size_t validCount = 0;
  for (size_t px = 0; px < npix; ++px) {
    if (!texValid[px]) {
      continue;
    }
    avgDirect[0] += direct[px * 3 + 0];
    avgDirect[1] += direct[px * 3 + 1];
    avgDirect[2] += direct[px * 3 + 2];
    ++validCount;
  }
  if (validCount > 0) {
    avgDirect[0] /= static_cast<float>(validCount);
    avgDirect[1] /= static_cast<float>(validCount);
    avgDirect[2] /= static_cast<float>(validCount);
  }
  const float hemi[8][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0.577f, 0.577f, 0.577f }, { -0.577f, -0.577f, -0.577f } };
  std::vector<float> total = direct;
  for (size_t px = 0; px < npix; ++px) {
    if (!texValid[px]) {
      continue;
    }
    float p[3] = { texPos[px * 3 + 0], texPos[px * 3 + 1], texPos[px * 3 + 2] };
    float n[3] = { texNrm[px * 3 + 0], texNrm[px * 3 + 1], texNrm[px * 3 + 2] };
    float up[3] = { 0, 1, 0 };
    if (n[1] > 0.99f || n[1] < -0.99f) {
      up[0] = 1.0f;
      up[1] = 0.0f;
    }
    float T[3] = { up[1] * n[2] - up[2] * n[1], up[2] * n[0] - up[0] * n[2], up[0] * n[1] - up[1] * n[0] };
    float tl = std::sqrt(T[0] * T[0] + T[1] * T[1] + T[2] * T[2]);
    if (tl < 1e-9f) {
      continue;
    }
    T[0] /= tl;
    T[1] /= tl;
    T[2] /= tl;
    float B[3] = { n[1] * T[2] - n[2] * T[1], n[2] * T[0] - n[0] * T[2], n[0] * T[1] - n[1] * T[0] };
    float acc = 0.0f;
    float wsum = 0.0f;
    for (int k = 0; k < 8; ++k) {
      float rd[3] = { T[0] * hemi[k][0] + B[0] * hemi[k][1] + n[0] * 0.7f, T[1] * hemi[k][0] + B[1] * hemi[k][1] + n[1] * 0.7f, T[2] * hemi[k][0] + B[2] * hemi[k][1] + n[2] * 0.7f };
      float rl = std::sqrt(rd[0] * rd[0] + rd[1] * rd[1] + rd[2] * rd[2]);
      rd[0] /= rl;
      rd[1] /= rl;
      rd[2] /= rl;
      float o[3] = { p[0] + n[0] * 1e-3f, p[1] + n[1] * 1e-3f, p[2] + n[2] * 1e-3f };
      int face = -1;
      float dist = 0.0f;
      if (!BvhClosest(bvh, o, rd, 50.0f, face, dist)) {
        continue;
      }
      const BakeTri& ht = tris[static_cast<size_t>(face)];
      float hp[3] = { o[0] + rd[0] * dist, o[1] + rd[1] * dist, o[2] + rd[2] * dist };
      float hd = std::sqrt((hp[0] - p[0]) * (hp[0] - p[0]) + (hp[1] - p[1]) * (hp[1] - p[1]) + (hp[2] - p[2]) * (hp[2] - p[2]));
      if (hd < 1e-4f) {
        continue;
      }
      float wgt = (rd[0] * n[0] + rd[1] * n[1] + rd[2] * n[2]) / (hd * hd + 1.0f);
      if (wgt <= 0.0f) {
        continue;
      }
      float incoming = (ht.albedo[0] * avgDirect[0] + ht.albedo[1] * avgDirect[1] + ht.albedo[2] * avgDirect[2]) / 3.0f;
      acc += incoming * wgt;
      wsum += wgt;
    }
    if (wsum > 1e-6f) {
      float bounce = acc / wsum * 0.5f;
      total[px * 3 + 0] += bounce;
      total[px * 3 + 1] += bounce;
      total[px * 3 + 2] += bounce;
    }
  }
  std::vector<unsigned char> rgba(npix * 4);
  for (size_t px = 0; px < npix; ++px) {
    for (int c = 0; c < 3; ++c) {
      float v = total[px * 3 + c];
      if (v < 0.0f) {
        v = 0.0f;
      }
      if (v > 1.0f) {
        v = 1.0f;
      }
      rgba[px * 4 + c] = static_cast<unsigned char>(v * 255.0f + 0.5f);
    }
    rgba[px * 4 + 3] = 255;
  }
  std::filesystem::path mp(trec->meshPath);
  std::string base = mp.stem().string() + "_lm";
  std::string texOut;
  for (int n = 0; n < 1000; ++n) {
    std::string cand = (mp.parent_path() / (base + std::to_string(n) + ".kztex")).string();
    std::error_code ec;
    if (!std::filesystem::exists(cand, ec)) {
      texOut = cand;
      break;
    }
  }
  if (texOut.empty()) {
    errorOut = "No free lightmap name";
    return false;
  }
  TextureAssetData lmtex;
  if (!CompressTexturePixels(rgba.data(), res, res, "", lmtex, "lightmap", TexKind::Orm, "", 0)) {
    errorOut = "Lightmap compress failed";
    return false;
  }
  if (!EncodeTextureFile(lmtex, texOut)) {
    errorOut = "Lightmap encode failed";
    return false;
  }
  texPathOut = texOut;
  texGuidOut = lmtex.guid;
  return true;
}
}
