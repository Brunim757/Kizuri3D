#include "Kizuri/Scene.h"
#include <cmath>
#include <cstring>
namespace Kizuri {
void MakeIdentityTransform(Transform& t) {
  t.position[0] = 0.0f;
  t.position[1] = 0.0f;
  t.position[2] = 0.0f;
  t.rotation[0] = 0.0f;
  t.rotation[1] = 0.0f;
  t.rotation[2] = 0.0f;
  t.scale[0] = 1.0f;
  t.scale[1] = 1.0f;
  t.scale[2] = 1.0f;
}
void MakeDefaultLight(LightData& l) {
  l.type = static_cast<int>(LightType::Point);
  l.color[0] = 1.0f;
  l.color[1] = 1.0f;
  l.color[2] = 1.0f;
  l.intensity = 3.0f;
  l.radius = 10.0f;
  l.spotAngle = 45.0f;
  l.falloff = 0.1f;
  l.castShadow = false;
  l.lightSize = 0.3f;
  l.shadowSize = 1024;
  l.softness = 0.3f;
  l.cascades = 3;
  l.lambda = 0.5f;
}
void MakeDefaultMaterial(MaterialData& m) {
  m.albedo[0] = 1.0f;
  m.albedo[1] = 1.0f;
  m.albedo[2] = 1.0f;
  m.metallic = 0.0f;
  m.roughness = 0.5f;
  m.albedoTex.clear();
  m.normalTex.clear();
  m.ormTex.clear();
  m.heightTex.clear();
}
void MakeDefaultDecal(DecalData& d) {
  d.size[0] = 1.0f;
  d.size[1] = 1.0f;
  d.size[2] = 1.0f;
  d.life = 0.0f;
  d.texGuid.clear();
}
void ComposeMatrix(const Transform& t, float m[16]) {  float rx = t.rotation[0] * 0.01745329252f;
  float ry = t.rotation[1] * 0.01745329252f;
  float rz = t.rotation[2] * 0.01745329252f;
  float cx = cosf(rx);
  float sx = sinf(rx);
  float cy = cosf(ry);
  float sy = sinf(ry);
  float cz = cosf(rz);
  float sz = sinf(rz);
  float r00 = cy * cz;
  float r01 = cy * sz;
  float r02 = -sy;
  float r10 = sx * sy * cz - cx * sz;
  float r11 = sx * sy * sz + cx * cz;
  float r12 = sx * cy;
  float r20 = cx * sy * cz + sx * sz;
  float r21 = cx * sy * sz - sx * cz;
  float r22 = cx * cy;
  float s0 = t.scale[0];
  float s1 = t.scale[1];
  float s2 = t.scale[2];
  m[0] = r00 * s0; m[1] = r01 * s0; m[2] = r02 * s0; m[3] = 0.0f;
  m[4] = r10 * s1; m[5] = r11 * s1; m[6] = r12 * s1; m[7] = 0.0f;
  m[8] = r20 * s2; m[9] = r21 * s2; m[10] = r22 * s2; m[11] = 0.0f;
  m[12] = t.position[0]; m[13] = t.position[1]; m[14] = t.position[2]; m[15] = 1.0f;
}
void EntityForward(const Transform& t, float dir[3]) {
  float rx = t.rotation[0] * 0.01745329252f;
  float ry = t.rotation[1] * 0.01745329252f;
  float rz = t.rotation[2] * 0.01745329252f;
  float sx = sinf(rx);
  float cx = cosf(rx);
  float sy = sinf(ry);
  float cy = cosf(ry);
  float sz = sinf(rz);
  float cz = cosf(rz);
  dir[0] = cx * sy * cz + sx * sz;
  dir[1] = cx * sy * sz - sx * cz;
  dir[2] = cx * cy;
  float l = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
  if (l < 1e-6f) {
    dir[0] = 0.0f;
    dir[1] = 0.0f;
    dir[2] = 1.0f;
    return;
  }
  dir[0] /= l;
  dir[1] /= l;
  dir[2] /= l;
}
bool InvertMatrix4(const float m[16], float invOut[16]) {
  if (m == nullptr || invOut == nullptr) {
    return false;
  }
  float a[16];
  for (int i = 0; i < 16; ++i) {
    a[i] = m[i];
  }
  float inv[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
  for (int c = 0; c < 4; ++c) {
    int piv = c;
    float best = a[c * 4 + c] < 0.0f ? -a[c * 4 + c] : a[c * 4 + c];
    for (int r = c + 1; r < 4; ++r) {
      float v = a[r * 4 + c] < 0.0f ? -a[r * 4 + c] : a[r * 4 + c];
      if (v > best) {
        best = v;
        piv = r;
      }
    }
    if (best < 1e-12f) {
      return false;
    }
    if (piv != c) {
      for (int k = 0; k < 4; ++k) {
        float t = a[c * 4 + k];
        a[c * 4 + k] = a[piv * 4 + k];
        a[piv * 4 + k] = t;
        t = inv[c * 4 + k];
        inv[c * 4 + k] = inv[piv * 4 + k];
        inv[piv * 4 + k] = t;
      }
    }
    float d = a[c * 4 + c];
    for (int k = 0; k < 4; ++k) {
      a[c * 4 + k] /= d;
      inv[c * 4 + k] /= d;
    }
    for (int r = 0; r < 4; ++r) {
      if (r == c) {
        continue;
      }
      float f = a[r * 4 + c];
      if (f == 0.0f) {
        continue;
      }
      for (int k = 0; k < 4; ++k) {
        a[r * 4 + k] -= f * a[c * 4 + k];
        inv[r * 4 + k] -= f * inv[c * 4 + k];
      }
    }
  }
  for (int i = 0; i < 16; ++i) {
    invOut[i] = inv[i];
  }
  return true;
}
Scene::Scene()
  : dirty(false) {
}
bool Scene::IsLive(uint32_t idx, uint32_t gen) const {
  if (idx >= slots.size()) {
    return false;
  }
  if (!slots[idx].alive) {
    return false;
  }
  return slots[idx].entity.id.generation == gen;
}
EntityId Scene::CreateEntity(const std::string& name) {
  EntityId id;
  std::string finalName = name.empty() ? "Entity" : name;
  if (!freeList.empty()) {
    uint32_t idx = freeList.back();
    freeList.pop_back();
    Slot& s = slots[idx];
    s.alive = true;
    s.entity.id.index = idx;
    s.entity.id.generation = s.entity.id.generation + 1;
    s.entity.name = finalName;
    MakeIdentityTransform(s.entity.transform);
    s.entity.meshGuid.clear();
    s.entity.hasMesh = false;
    MakeDefaultLight(s.entity.light);
    s.entity.hasLight = false;
    MakeDefaultMaterial(s.entity.material);
    s.entity.hasMaterial = false;
    MakeDefaultDecal(s.entity.decal);
    s.entity.hasDecal = false;
    s.entity.parent = EntityId::Invalid();
    s.entity.children.clear();
    id = s.entity.id;
  } else {
    Slot s;
    s.alive = true;
    s.entity.id.index = static_cast<uint32_t>(slots.size());
    s.entity.id.generation = 1;
    s.entity.name = finalName;
    MakeIdentityTransform(s.entity.transform);
    s.entity.meshGuid.clear();
    s.entity.hasMesh = false;
    MakeDefaultLight(s.entity.light);
    s.entity.hasLight = false;
    MakeDefaultMaterial(s.entity.material);
    s.entity.hasMaterial = false;
    MakeDefaultDecal(s.entity.decal);
    s.entity.hasDecal = false;
    s.entity.parent = EntityId::Invalid();
    slots.push_back(s);
    id = slots.back().entity.id;
  }
  dirty = true;
  return id;
}
bool Scene::DeleteEntity(EntityId id) {
  if (!Has(id)) {
    return false;
  }
  std::vector<EntityId> stack;
  stack.push_back(id);
  while (!stack.empty()) {
    EntityId cur = stack.back();
    stack.pop_back();
    Entity* e = Get(cur);
    if (e == nullptr) {
      continue;
    }
    for (size_t i = 0; i < e->children.size(); ++i) {
      stack.push_back(e->children[i]);
    }
    if (e->parent.IsValid()) {
      Entity* p = Get(e->parent);
      if (p != nullptr) {
        for (size_t i = 0; i < p->children.size(); ++i) {
          if (p->children[i] == cur) {
            p->children.erase(p->children.begin() + i);
            break;
          }
        }
      }
    }
    uint32_t idx = cur.index;
    slots[idx].alive = false;
    slots[idx].entity.children.clear();
    freeList.push_back(idx);
  }
  dirty = true;
  return true;
}
EntityId Scene::DuplicateRecursive(EntityId id, EntityId newParent) {
  const Entity* src = Get(id);
  if (src == nullptr) {
    return EntityId::Invalid();
  }
  EntityId copy = CreateEntity(src->name + " Copy");
  Entity* dst = Get(copy);
  if (dst == nullptr) {
    return EntityId::Invalid();
  }
  dst->transform = src->transform;
  std::vector<EntityId> kids = src->children;
  for (size_t i = 0; i < kids.size(); ++i) {
    DuplicateRecursive(kids[i], copy);
  }
  if (newParent.IsValid()) {
    SetParent(copy, newParent);
  }
  return copy;
}
EntityId Scene::DuplicateEntity(EntityId id) {
  if (!Has(id)) {
    return EntityId::Invalid();
  }
  const Entity* src = Get(id);
  EntityId p = src->parent;
  return DuplicateRecursive(id, p);
}
bool Scene::RenameEntity(EntityId id, const std::string& name) {
  Entity* e = Get(id);
  if (e == nullptr || name.empty()) {
    return false;
  }
  e->name = name;
  dirty = true;
  return true;
}
bool Scene::SetTransform(EntityId id, const Transform& t) {
  Entity* e = Get(id);
  if (e == nullptr) {
    return false;
  }
  e->transform = t;
  dirty = true;
  return true;
}
bool Scene::SetParent(EntityId child, EntityId parent) {
  Entity* c = Get(child);
  if (c == nullptr) {
    return false;
  }
  if (parent.IsValid() && !Has(parent)) {
    return false;
  }
  if (parent.IsValid() && parent == child) {
    return false;
  }
  EntityId probe = parent;
  while (probe.IsValid()) {
    if (probe == child) {
      return false;
    }
    const Entity* pe = Get(probe);
    if (pe == nullptr) {
      break;
    }
    probe = pe->parent;
  }
  if (c->parent.IsValid()) {
    Entity* old = Get(c->parent);
    if (old != nullptr) {
      for (size_t i = 0; i < old->children.size(); ++i) {
        if (old->children[i] == child) {
          old->children.erase(old->children.begin() + i);
          break;
        }
      }
    }
  }
  c->parent = parent;
  if (parent.IsValid()) {
    Entity* np = Get(parent);
    if (np != nullptr) {
      np->children.push_back(child);
    }
  }
  dirty = true;
  return true;
}
bool Scene::Has(EntityId id) const {
  return IsLive(id.index, id.generation);
}
Entity* Scene::Get(EntityId id) {
  if (!IsLive(id.index, id.generation)) {
    return nullptr;
  }
  return &slots[id.index].entity;
}
const Entity* Scene::Get(EntityId id) const {
  if (!IsLive(id.index, id.generation)) {
    return nullptr;
  }
  return &slots[id.index].entity;
}
size_t Scene::Count() const {
  size_t n = 0;
  for (size_t i = 0; i < slots.size(); ++i) {
    if (slots[i].alive) {
      ++n;
    }
  }
  return n;
}
std::vector<EntityId> Scene::All() const {
  std::vector<EntityId> out;
  for (size_t i = 0; i < slots.size(); ++i) {
    if (slots[i].alive) {
      out.push_back(slots[i].entity.id);
    }
  }
  return out;
}
void Scene::Clear() {
  slots.clear();
  freeList.clear();
  dirty = true;
}
bool Scene::IsDirty() const {
  return dirty;
}
void Scene::ClearDirty() {
  dirty = false;
}
void Scene::MarkDirty() {
  dirty = true;
}
}
