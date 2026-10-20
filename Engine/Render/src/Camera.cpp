#include "Kizuri/Camera.h"
using namespace DirectX;
namespace Kizuri {
FreeCamera::FreeCamera()
  : moveSpeed(4.0f)
  , lookSpeed(0.0025f)
  , fovY(1.04719755f)
  , nearZ(0.1f)
  , farZ(500.0f)
  , px(0.0f)
  , py(1.5f)
  , pz(-5.0f)
  , yaw(0.0f)
  , pitch(0.0f) {
}
void FreeCamera::SetPosition(float x, float y, float z) {
  px = x;
  py = y;
  pz = z;
}
void FreeCamera::SetYawPitch(float y, float p) {
  yaw = y;
  pitch = p;
}
void FreeCamera::GetPosition(float& x, float& y, float& z) const {
  x = px;
  y = py;
  z = pz;
}
void FreeCamera::GetYawPitch(float& outYaw, float& outPitch) const {
  outYaw = yaw;
  outPitch = pitch;
}
void FreeCamera::Update(float dt, bool fwd, bool back, bool left, bool right, bool up, bool down, float mouseDX, float mouseDY) {
  yaw += mouseDX * lookSpeed;
  pitch += mouseDY * lookSpeed;
  if (pitch > 1.55f) {
    pitch = 1.55f;
  }
  if (pitch < -1.55f) {
    pitch = -1.55f;
  }
  XMVECTOR dir = XMVectorSet(sinf(yaw) * cosf(pitch), -sinf(pitch), cosf(yaw) * cosf(pitch), 0.0f);
  XMVECTOR rgt = XMVectorSet(cosf(yaw), 0.0f, -sinf(yaw), 0.0f);
  XMVECTOR upv = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
  XMVECTOR pos = XMVectorSet(px, py, pz, 0.0f);
  float s = moveSpeed * dt;
  if (fwd) {
    pos = XMVectorAdd(pos, XMVectorScale(dir, s));
  }
  if (back) {
    pos = XMVectorSubtract(pos, XMVectorScale(dir, s));
  }
  if (right) {
    pos = XMVectorAdd(pos, XMVectorScale(rgt, s));
  }
  if (left) {
    pos = XMVectorSubtract(pos, XMVectorScale(rgt, s));
  }
  if (up) {
    pos = XMVectorAdd(pos, XMVectorScale(upv, s));
  }
  if (down) {
    pos = XMVectorSubtract(pos, XMVectorScale(upv, s));
  }
  XMFLOAT3 o;
  XMStoreFloat3(&o, pos);
  px = o.x;
  py = o.y;
  pz = o.z;
}
void FreeCamera::Focus(float x, float y, float z, float distance) {
  float dx = sinf(yaw) * cosf(pitch);
  float dy = -sinf(pitch);
  float dz = cosf(yaw) * cosf(pitch);
  px = x - dx * distance;
  py = y - dy * distance;
  pz = z - dz * distance;
}
XMMATRIX FreeCamera::View() const {
  XMVECTOR pos = XMVectorSet(px, py, pz, 1.0f);
  XMVECTOR dir = XMVectorSet(sinf(yaw) * cosf(pitch), -sinf(pitch), cosf(yaw) * cosf(pitch), 0.0f);
  XMVECTOR at = XMVectorAdd(pos, dir);
  XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
  return XMMatrixLookAtLH(pos, at, up);
}
XMMATRIX FreeCamera::Projection(float aspect) const {
  return XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
}
}
