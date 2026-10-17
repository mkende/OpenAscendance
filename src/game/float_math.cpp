#include "game/float_math.h"

#include <cmath>
#include <limits>

namespace oa::game {

std::int32_t chop(double v) {
  const double t = std::trunc(v);
  if (!(t >= static_cast<double>(std::numeric_limits<std::int32_t>::min()) &&
        t <= static_cast<double>(std::numeric_limits<std::int32_t>::max()))) {
    return std::numeric_limits<std::int32_t>::min();
  }
  return static_cast<std::int32_t>(t);
}

Vec3 sub(const Vec3& a, const Vec3& b) {
  return {f32(static_cast<double>(a[0]) - b[0]), f32(static_cast<double>(a[1]) - b[1]),
          f32(static_cast<double>(a[2]) - b[2])};
}

double length(const Vec3& d) {
  const double x = d[0];
  const double y = d[1];
  const double z = d[2];
  return std::sqrt((y * y + x * x) + z * z);
}

std::int32_t chopped_distance(const Vec3& a, const Vec3& b) { return chop(length(sub(a, b))); }

void normalize(Vec3& v) {
  const float len = f32(length(v));
  if (0.0f < len) {
    const double inv = 1.0 / len;
    v[0] = f32(v[0] * inv);
    v[1] = f32(v[1] * inv);
    v[2] = f32(inv * v[2]);
  }
}

void set_length(Vec3& v, float len) {
  normalize(v);
  v[0] = f32(static_cast<double>(v[0]) * len);
  v[1] = f32(static_cast<double>(v[1]) * len);
  v[2] = f32(static_cast<double>(len) * v[2]);
}

void rotate_y(Vec3& v, float degrees) {
  if (degrees == 0.0f) {
    return;
  }
  // The degree-to-radian factor is the float 0.017453292 (0x85fc0).
  const double radians = static_cast<double>(degrees) * static_cast<double>(0.017453292f);
  const float c = f32(std::cos(radians));
  const double s = std::sin(radians);
  const float x = v[0];
  v[0] = f32(v[2] * s + static_cast<double>(x) * c);
  v[2] = f32(static_cast<double>(v[2]) * c - x * s);
}

void snap_to_grid(Vec3& v, std::int32_t grid) {
  for (float& c : v) {
    const std::int32_t half = (0.0f > c ? -grid : grid) / 2;
    c = static_cast<float>(chop(static_cast<double>(half) + c) / grid * grid);
  }
}

}  // namespace oa::game
