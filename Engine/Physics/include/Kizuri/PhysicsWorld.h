#pragma once
#include <cstddef>
#include <cstdint>
namespace Kizuri {
class JobSystem;
enum class PhysMotionType {
  Static = 0,
  Kinematic = 1,
  Dynamic = 2
};
enum class PhysColliderType {
  Box = 0,
  Sphere = 1,
  Capsule = 2,
  Mesh = 3
};
struct PhysBodyDesc {
  PhysMotionType motion = PhysMotionType::Dynamic;
  float mass = 1.0f;
  PhysColliderType collider = PhysColliderType::Box;
  float extents[3] = { 0.5f, 0.5f, 0.5f };
  const float* meshVerts = nullptr;
  size_t meshVertCount = 0;
  const uint32_t* meshIndices = nullptr;
  size_t meshIndexCount = 0;
  float friction = 0.5f;
  float restitution = 0.0f;
};
class PhysicsWorld {
public:
  PhysicsWorld();
  ~PhysicsWorld();
  bool Initialize(JobSystem* jobs);
  void Shutdown();
  bool IsInitialized() const { return initialized; }
  uint32_t CreateBody(const PhysBodyDesc& desc, const float pos[3], const float quat[4]);
  void DestroyBody(uint32_t handle);
  bool HasBody(uint32_t handle) const;
  size_t BodyCount() const;
  void TeleportBody(uint32_t handle, const float pos[3], const float quat[4]);
  void SetKinematicTarget(uint32_t handle, const float pos[3], const float quat[4]);
  void SetGravity(float y);
  void Step(float dt);
  float Alpha() const { return stepAlpha; }
  bool GetPose(uint32_t handle, float pos[3], float quat[4]) const;
  bool Raycast(const float origin[3], const float dir[3], float maxDist, float hitPos[3], float hitNormal[3], uint32_t* outHandle) const;
  uint32_t AddPointConstraint(uint32_t handleA, uint32_t handleB, const float worldPivot[3]);
  uint32_t AddHingeConstraint(uint32_t handleA, uint32_t handleB, const float worldPivot[3], const float worldAxis[3], float minLimit, float maxLimit);
  void DestroyConstraint(uint32_t handle);
  size_t ConstraintCount() const;
private:
  struct Impl;
  Impl* impl;
  bool initialized;
  float stepAlpha = 0.0f;
  PhysicsWorld(const PhysicsWorld&) = delete;
  PhysicsWorld& operator=(const PhysicsWorld&) = delete;
};
}
