#include "Kizuri/Origin.h"
#include <cmath>
namespace Kizuri {
OriginRebaser::OriginRebaser()
  : ox(0.0)
  , oy(0.0)
  , oz(0.0)
  , sx(0.0)
  , sy(0.0)
  , sz(0.0) {
}
void OriginRebaser::SetOrigin(double x, double y, double z) {
  sx = x - ox;
  sy = y - oy;
  sz = z - oz;
  ox = x;
  oy = y;
  oz = z;
}
void OriginRebaser::GetOrigin(double& x, double& y, double& z) const {
  x = ox;
  y = oy;
  z = oz;
}
void OriginRebaser::GetLastShift(double& x, double& y, double& z) const {
  x = sx;
  y = sy;
  z = sz;
}
void OriginRebaser::Rebase(double px, double py, double pz) {
  SetOrigin(px, py, pz);
}
bool OriginRebaser::RebaseIfNeeded(double px, double py, double pz, double threshold) {
  double d = DistanceToOrigin(px, py, pz);
  if (d > threshold) {
    Rebase(px, py, pz);
    return true;
  }
  return false;
}
void OriginRebaser::WorldToLocal(double wx, double wy, double wz, float& lx, float& ly, float& lz) const {
  lx = static_cast<float>(wx - ox);
  ly = static_cast<float>(wy - oy);
  lz = static_cast<float>(wz - oz);
}
void OriginRebaser::LocalToWorld(float lx, float ly, float lz, double& wx, double& wy, double& wz) const {
  wx = static_cast<double>(lx) + ox;
  wy = static_cast<double>(ly) + oy;
  wz = static_cast<double>(lz) + oz;
}
double OriginRebaser::DistanceToOrigin(double px, double py, double pz) const {
  double dx = px - ox;
  double dy = py - oy;
  double dz = pz - oz;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}
}
