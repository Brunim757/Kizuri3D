#include "Kizuri/PhysicsWorld.h"
#include "Kizuri/JobSystemEnki.h"
#include "Kizuri/JobSystem.h"
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystem.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <DirectXMath.h>
#include <cmath>
#include <cstring>
#include <mutex>
#include <vector>
namespace Kizuri {
namespace Layers {
static constexpr JPH::ObjectLayer kNonMoving = 0;
static constexpr JPH::ObjectLayer kMoving = 1;
static constexpr JPH::ObjectLayer kCount = 2;
}
class PhysBroadPhaseLayer final : public JPH::BroadPhaseLayerInterface {
public:
  JPH::uint GetNumBroadPhaseLayers() const override {
    return 2;
  }
  JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
    return JPH::BroadPhaseLayer(inLayer);
  }
  const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
    return inLayer == JPH::BroadPhaseLayer(1) ? "Moving" : "NonMoving";
  }
};
class PhysObjectVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
  bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
    (void)inLayer1;
    (void)inLayer2;
    return true;
  }
};
class PhysObjectPairFilter final : public JPH::ObjectLayerPairFilter {
public:
  bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override {
    if (inObject1 == Layers::kNonMoving) {
      return inObject2 == Layers::kMoving;
    }
    return true;
  }
};
static void PhysTrace(const char* msg, ...) {
  (void)msg;
}
#ifdef JPH_ENABLE_ASSERTS
static bool PhysAssert(const char* expr, const char* msg, const char* file, JPH::uint line) {
  (void)expr;
  (void)file;
  (void)line;
  (void)msg;
  return true;
}
#endif
struct PhysicsWorld::Impl {
  JPH::PhysicsSystem system;
  JPH::TempAllocatorImpl* tempAlloc = nullptr;
  JobSystemEnki* jobAdapter = nullptr;
  PhysBroadPhaseLayer broadPhase;
  PhysObjectVsBroadPhase broadFilter;
  PhysObjectPairFilter pairFilter;
  struct Slot {
    bool active = false;
    JPH::BodyID bodyId;
    float prevPos[3] = { 0.0f, 0.0f, 0.0f };
    float prevRot[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float targetPos[3] = { 0.0f, 0.0f, 0.0f };
    float targetRot[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool hasTarget = false;
    bool isKinematic = false;
  };
  std::vector<Slot> slots;
  std::vector<uint32_t> freeList;
  size_t liveCount = 0;
  float accumulator = 0.0f;
  float gravityY = -9.81f;
  std::mutex listMutex;
};
PhysicsWorld::PhysicsWorld()
  : impl(nullptr)
  , initialized(false)
  , stepAlpha(0.0f) {
}
PhysicsWorld::~PhysicsWorld() {
  Shutdown();
}
bool PhysicsWorld::Initialize(JobSystem* jobs) {
  if (initialized || jobs == nullptr) {
    return false;
  }
  JPH::RegisterDefaultAllocator();
  JPH::Trace = PhysTrace;
#ifdef JPH_ENABLE_ASSERTS
  JPH::AssertFailed = PhysAssert;
#endif
  JPH::Factory::sInstance = new JPH::Factory();
  JPH::RegisterTypes();
  Impl* p = new Impl();
  p->tempAlloc = new JPH::TempAllocatorImpl(10 * 1024 * 1024);
  p->jobAdapter = new JobSystemEnki(jobs);
  p->jobAdapter->Init(8);
  p->system.Init(1024, 0, 2048, 2048, p->broadPhase, p->broadFilter, p->pairFilter);
  p->system.SetGravity(JPH::Vec3(0.0f, p->gravityY, 0.0f));
  impl = p;
  initialized = true;
  stepAlpha = 0.0f;
  return true;
}
void PhysicsWorld::Shutdown() {
  if (!initialized) {
    return;
  }
  Impl* p = impl;
  impl = nullptr;
  initialized = false;
  if (p != nullptr) {
    {
      std::lock_guard<std::mutex> lock(p->listMutex);
      JPH::BodyInterface& bi = p->system.GetBodyInterface();
      for (size_t i = 0; i < p->slots.size(); ++i) {
        if (p->slots[i].active) {
          bi.RemoveBody(p->slots[i].bodyId);
          bi.DestroyBody(p->slots[i].bodyId);
          p->slots[i].active = false;
        }
      }
      p->slots.clear();
      p->freeList.clear();
      p->liveCount = 0;
    }
    delete p->jobAdapter;
    p->jobAdapter = nullptr;
    delete p->tempAlloc;
    p->tempAlloc = nullptr;
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
    delete p;
  }
}
uint32_t PhysicsWorld::CreateBody(const PhysBodyDesc& desc, const float pos[3], const float quat[4]) {
  if (!initialized || pos == nullptr || quat == nullptr) {
    return 0xFFFFFFFFu;
  }
  float hx = desc.extents[0] > 0.01f ? desc.extents[0] : 0.01f;
  float hy = desc.extents[1] > 0.01f ? desc.extents[1] : 0.01f;
  float hz = desc.extents[2] > 0.01f ? desc.extents[2] : 0.01f;
  JPH::Shape::ShapeResult shapeResult;
  JPH::ShapeRefC shape;
  if (desc.collider == PhysColliderType::Sphere) {
    JPH::SphereShapeSettings sphere(hx);
    shapeResult = sphere.Create();
  } else if (desc.collider == PhysColliderType::Capsule) {
    JPH::CapsuleShapeSettings capsule(hy, hx);
    shapeResult = capsule.Create();
  } else if (desc.collider == PhysColliderType::Mesh && desc.meshVerts != nullptr && desc.meshIndices != nullptr && desc.meshVertCount > 0 && desc.meshIndexCount >= 3) {
    JPH::VertexList verts;
    verts.reserve(desc.meshVertCount);
    for (size_t i = 0; i < desc.meshVertCount; ++i) {
      verts.emplace_back(desc.meshVerts[i * 3 + 0], desc.meshVerts[i * 3 + 1], desc.meshVerts[i * 3 + 2]);
    }
    JPH::IndexedTriangleList tris;
    tris.reserve(desc.meshIndexCount / 3);
    for (size_t i = 0; i + 2 < desc.meshIndexCount; i += 3) {
      tris.emplace_back(desc.meshIndices[i], desc.meshIndices[i + 1], desc.meshIndices[i + 2]);
    }
    JPH::MeshShapeSettings mesh(std::move(verts), std::move(tris));
    shapeResult = mesh.Create();
  } else {
    JPH::BoxShapeSettings box(JPH::Vec3(hx, hy, hz));
    shapeResult = box.Create();
  }
  if (shapeResult.HasError()) {
    return 0xFFFFFFFFu;
  }
  shape = shapeResult.Get();
  JPH::EMotionType motion = JPH::EMotionType::Dynamic;
  JPH::ObjectLayer layer = Layers::kMoving;
  if (desc.motion == PhysMotionType::Static) {
    motion = JPH::EMotionType::Static;
    layer = Layers::kNonMoving;
  } else if (desc.motion == PhysMotionType::Kinematic) {
    motion = JPH::EMotionType::Kinematic;
  }
  JPH::BodyCreationSettings settings(shape.GetPtr(), JPH::RVec3(pos[0], pos[1], pos[2]), JPH::Quat(quat[0], quat[1], quat[2], quat[3]), motion, layer);
  settings.mFriction = desc.friction;
  settings.mRestitution = desc.restitution;
  if (desc.motion == PhysMotionType::Dynamic) {
    float mass = desc.mass > 0.01f ? desc.mass : 1.0f;
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateMassAndInertia;
    settings.mMassPropertiesOverride.mMass = mass;
  }
  Impl* p = impl;
  JPH::BodyInterface& bi = p->system.GetBodyInterface();
  JPH::Body* body = bi.CreateBody(settings);
  if (body == nullptr) {
    return 0xFFFFFFFFu;
  }
  JPH::BodyID bodyId = body->GetID();
  bi.AddBody(bodyId, motion == JPH::EMotionType::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
  std::lock_guard<std::mutex> lock(p->listMutex);
  uint32_t slot = 0xFFFFFFFFu;
  if (!p->freeList.empty()) {
    slot = p->freeList.back();
    p->freeList.pop_back();
  } else {
    slot = static_cast<uint32_t>(p->slots.size());
    p->slots.emplace_back();
  }
  Impl::Slot& s = p->slots[slot];
  s.active = true;
  s.bodyId = bodyId;
  s.prevPos[0] = pos[0];
  s.prevPos[1] = pos[1];
  s.prevPos[2] = pos[2];
  s.prevRot[0] = quat[0];
  s.prevRot[1] = quat[1];
  s.prevRot[2] = quat[2];
  s.prevRot[3] = quat[3];
  s.hasTarget = false;
  s.isKinematic = desc.motion == PhysMotionType::Kinematic;
  ++p->liveCount;
  return slot + 1;
}
void PhysicsWorld::DestroyBody(uint32_t handle) {
  if (!initialized || handle == 0) {
    return;
  }
  Impl* p = impl;
  std::lock_guard<std::mutex> lock(p->listMutex);
  uint32_t slot = handle - 1;
  if (slot >= p->slots.size() || !p->slots[slot].active) {
    return;
  }
  JPH::BodyInterface& bi = p->system.GetBodyInterface();
  bi.RemoveBody(p->slots[slot].bodyId);
  bi.DestroyBody(p->slots[slot].bodyId);
  p->slots[slot].active = false;
  p->freeList.push_back(slot);
  --p->liveCount;
}
bool PhysicsWorld::HasBody(uint32_t handle) const {
  if (!initialized || handle == 0 || impl == nullptr) {
    return false;
  }
  const Impl* p = impl;
  uint32_t slot = handle - 1;
  return slot < p->slots.size() && p->slots[slot].active;
}
size_t PhysicsWorld::BodyCount() const {
  if (!initialized || impl == nullptr) {
    return 0;
  }
  return impl->liveCount;
}
void PhysicsWorld::TeleportBody(uint32_t handle, const float pos[3], const float quat[4]) {
  if (!HasBody(handle) || pos == nullptr || quat == nullptr) {
    return;
  }
  Impl* p = impl;
  Impl::Slot& s = p->slots[handle - 1];
  p->system.GetBodyInterface().SetPositionAndRotation(s.bodyId, JPH::RVec3(pos[0], pos[1], pos[2]), JPH::Quat(quat[0], quat[1], quat[2], quat[3]), JPH::EActivation::DontActivate);
  s.prevPos[0] = pos[0];
  s.prevPos[1] = pos[1];
  s.prevPos[2] = pos[2];
  s.prevRot[0] = quat[0];
  s.prevRot[1] = quat[1];
  s.prevRot[2] = quat[2];
  s.prevRot[3] = quat[3];
}
void PhysicsWorld::SetKinematicTarget(uint32_t handle, const float pos[3], const float quat[4]) {
  if (!HasBody(handle) || pos == nullptr || quat == nullptr) {
    return;
  }
  Impl* p = impl;
  Impl::Slot& s = p->slots[handle - 1];
  s.targetPos[0] = pos[0];
  s.targetPos[1] = pos[1];
  s.targetPos[2] = pos[2];
  s.targetRot[0] = quat[0];
  s.targetRot[1] = quat[1];
  s.targetRot[2] = quat[2];
  s.targetRot[3] = quat[3];
  s.hasTarget = true;
}
void PhysicsWorld::SetGravity(float y) {
  if (!initialized || impl == nullptr) {
    return;
  }
  impl->gravityY = y;
  impl->system.SetGravity(JPH::Vec3(0.0f, y, 0.0f));
}
void PhysicsWorld::Step(float dt) {
  if (!initialized || impl == nullptr) {
    return;
  }
  Impl* p = impl;
  const float fixed = 1.0f / 60.0f;
  if (dt < 0.0f) {
    dt = 0.0f;
  }
  if (dt > fixed * 4.0f) {
    dt = fixed * 4.0f;
  }
  JPH::BodyInterface& bi = p->system.GetBodyInterface();
  for (size_t i = 0; i < p->slots.size(); ++i) {
    if (!p->slots[i].active) {
      continue;
    }
    JPH::RVec3 cp;
    JPH::Quat cr;
    bi.GetPositionAndRotation(p->slots[i].bodyId, cp, cr);
    p->slots[i].prevPos[0] = static_cast<float>(cp.GetX());
    p->slots[i].prevPos[1] = static_cast<float>(cp.GetY());
    p->slots[i].prevPos[2] = static_cast<float>(cp.GetZ());
    p->slots[i].prevRot[0] = cr.GetX();
    p->slots[i].prevRot[1] = cr.GetY();
    p->slots[i].prevRot[2] = cr.GetZ();
    p->slots[i].prevRot[3] = cr.GetW();
  }
  p->accumulator += dt;
  int steps = 0;
  while (p->accumulator >= fixed && steps < 4) {
    for (size_t i = 0; i < p->slots.size(); ++i) {
      if (!p->slots[i].active || !p->slots[i].isKinematic || !p->slots[i].hasTarget) {
        continue;
      }
      Impl::Slot& s = p->slots[i];
      bi.MoveKinematic(s.bodyId, JPH::RVec3(s.targetPos[0], s.targetPos[1], s.targetPos[2]), JPH::Quat(s.targetRot[0], s.targetRot[1], s.targetRot[2], s.targetRot[3]), fixed);
    }
    p->system.Update(fixed, 1, p->tempAlloc, p->jobAdapter);
    p->accumulator -= fixed;
    ++steps;
  }
  if (p->accumulator >= fixed) {
    p->accumulator = 0.0f;
  }
  stepAlpha = p->accumulator / fixed;
}
bool PhysicsWorld::GetPose(uint32_t handle, float pos[3], float quat[4]) const {
  if (!HasBody(handle) || pos == nullptr || quat == nullptr || impl == nullptr) {
    return false;
  }
  const Impl* p = impl;
  const Impl::Slot& s = p->slots[handle - 1];
  float t = stepAlpha < 0.0f ? 0.0f : (stepAlpha > 1.0f ? 1.0f : stepAlpha);
  JPH::RVec3 cp;
  JPH::Quat cr;
  p->system.GetBodyInterface().GetPositionAndRotation(s.bodyId, cp, cr);
  float cx = static_cast<float>(cp.GetX());
  float cy = static_cast<float>(cp.GetY());
  float cz = static_cast<float>(cp.GetZ());
  pos[0] = s.prevPos[0] + (cx - s.prevPos[0]) * t;
  pos[1] = s.prevPos[1] + (cy - s.prevPos[1]) * t;
  pos[2] = s.prevPos[2] + (cz - s.prevPos[2]) * t;
  DirectX::XMVECTOR q0 = DirectX::XMVectorSet(s.prevRot[0], s.prevRot[1], s.prevRot[2], s.prevRot[3]);
  DirectX::XMVECTOR q1 = DirectX::XMVectorSet(cr.GetX(), cr.GetY(), cr.GetZ(), cr.GetW());
  float dot = s.prevRot[0] * cr.GetX() + s.prevRot[1] * cr.GetY() + s.prevRot[2] * cr.GetZ() + s.prevRot[3] * cr.GetW();
  if (dot < 0.0f) {
    q1 = DirectX::XMVectorNegate(q1);
  }
  DirectX::XMVECTOR qr = DirectX::XMQuaternionSlerp(q0, q1, t);
  quat[0] = DirectX::XMVectorGetX(qr);
  quat[1] = DirectX::XMVectorGetY(qr);
  quat[2] = DirectX::XMVectorGetZ(qr);
  quat[3] = DirectX::XMVectorGetW(qr);
  return true;
}
bool PhysicsWorld::Raycast(const float origin[3], const float dir[3], float maxDist, float hitPos[3], float hitNormal[3], uint32_t* outHandle) const {
  if (!initialized || impl == nullptr || origin == nullptr || dir == nullptr || maxDist <= 0.0f) {
    return false;
  }
  float len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
  if (len < 1e-9f) {
    return false;
  }
  JPH::RRayCast ray(JPH::RVec3(origin[0], origin[1], origin[2]), JPH::Vec3(dir[0] / len * maxDist, dir[1] / len * maxDist, dir[2] / len * maxDist));
  JPH::RayCastResult hit;
  bool had = impl->system.GetNarrowPhaseQuery().CastRay(ray, hit);
  if (!had || hit.mBodyID.IsInvalid()) {
    return false;
  }
  float f = hit.mFraction;
  if (hitPos != nullptr) {
    hitPos[0] = origin[0] + dir[0] / len * maxDist * f;
    hitPos[1] = origin[1] + dir[1] / len * maxDist * f;
    hitPos[2] = origin[2] + dir[2] / len * maxDist * f;
  }
  const Impl* p = impl;
  const JPH::BodyLockRead lock(p->system.GetBodyLockInterface(), hit.mBodyID);
  if (hitNormal != nullptr) {
    if (lock.Succeeded()) {
      JPH::Vec3 n = lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, JPH::RVec3(hitPos[0], hitPos[1], hitPos[2]));
      hitNormal[0] = n.GetX();
      hitNormal[1] = n.GetY();
      hitNormal[2] = n.GetZ();
    } else {
      hitNormal[0] = 0.0f;
      hitNormal[1] = 1.0f;
      hitNormal[2] = 0.0f;
    }
  }
  if (outHandle != nullptr) {
    *outHandle = 0;
    for (size_t i = 0; i < p->slots.size(); ++i) {
      if (p->slots[i].active && p->slots[i].bodyId == hit.mBodyID) {
        *outHandle = static_cast<uint32_t>(i) + 1;
        break;
      }
    }
  }
  return true;
}
}
