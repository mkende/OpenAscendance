#include "screens/camera3d.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace oa {

namespace {

using game::f32;

double d(float v) { return static_cast<double>(v); }

// `orient_orthonormalize_keep_up` 0x4eb64 on the basis `b` (rows right, up,
// forward): up normalised, right = up x forward, forward = right x up, each
// normalised.
void orthonormalize_keep_up(std::array<float, 9>& b) {
  Vec3 up{b[3], b[4], b[5]};
  game::normalize(up);
  b[3] = up[0];
  b[4] = up[1];
  b[5] = up[2];
  Vec3 right{f32(d(b[4]) * b[8] - d(b[5]) * b[7]), f32(d(b[5]) * b[6] - d(b[3]) * b[8]),
             f32(d(b[3]) * b[7] - d(b[4]) * b[6])};
  game::normalize(right);
  b[0] = right[0];
  b[1] = right[1];
  b[2] = right[2];
  Vec3 forward{f32(d(b[1]) * b[5] - d(b[2]) * b[4]), f32(d(b[2]) * b[3] - d(b[0]) * b[5]),
               f32(d(b[0]) * b[4] - d(b[1]) * b[3])};
  game::normalize(forward);
  b[6] = forward[0];
  b[7] = forward[1];
  b[8] = forward[2];
}

}  // namespace

void Camera3d::set_projection(float fov_degrees, float near_z, float far_z, std::int32_t n) {
  fov = fov_degrees;
  near_plane = near_z;
  far_plane = far_z;
  scale = 1.0F;
  // The degree factor 0.017453292f (0x858e8) and 0.5f (0x807f4).
  const double half_angle = d(fov) * d(0.017453292F) * d(0.5F);
  focal = f32(static_cast<double>(n) / std::tan(half_angle));
}

void Camera3d::update_view() {
  const std::array<float, 9>& m = rotation;
  const std::array<float, 9>& b = orientation;
  const Vec3 c{-position[0], -position[1], -position[2]};
  // The operand order of `orient_build_view_matrix` 0x4ef18 (it decides the
  // double roundings).
  view[0] = f32((d(b[2]) * m[2] + d(b[0]) * m[0]) + d(b[1]) * m[1]);
  view[1] = f32((d(b[5]) * m[2] + d(b[3]) * m[0]) + d(b[4]) * m[1]);
  view[2] = f32((d(b[8]) * m[2] + d(b[6]) * m[0]) + d(b[7]) * m[1]);
  view[4] = f32((d(b[2]) * m[5] + d(b[0]) * m[3]) + d(b[1]) * m[4]);
  view[5] = f32((d(b[5]) * m[5] + d(b[4]) * m[4]) + d(b[3]) * m[3]);
  view[6] = f32((d(b[8]) * m[5] + d(b[7]) * m[4]) + d(b[6]) * m[3]);
  view[8] = f32((d(b[2]) * m[8] + d(b[0]) * m[6]) + d(b[1]) * m[7]);
  view[9] = f32((d(b[5]) * m[8] + d(b[4]) * m[7]) + d(b[3]) * m[6]);
  view[10] = f32((d(b[8]) * m[8] + d(b[7]) * m[7]) + d(b[6]) * m[6]);
  view[3] = f32((d(c[2]) * m[2] + d(c[0]) * m[0]) + d(c[1]) * m[1]);
  view[7] = f32((d(c[2]) * m[5] + d(c[0]) * m[3]) + d(c[1]) * m[4]);
  view[11] = f32((d(c[2]) * m[8] + d(c[0]) * m[6]) + d(c[1]) * m[7]);
}

void Camera3d::update_view_offset(const Vec3& offset) {
  const Vec3 saved = position;
  position = {f32(d(offset[0]) + position[0]), f32(d(offset[1]) + position[1]), f32(d(offset[2]) + position[2])};
  update_view();
  position = saved;
}

void Camera3d::rotate_forward_y(float degrees) {
  if (degrees == 0.0F) {
    return;
  }
  Vec3 forward{orientation[6], orientation[7], orientation[8]};
  game::rotate_y(forward, degrees);
  orientation[6] = forward[0];
  orientation[7] = forward[1];
  orientation[8] = forward[2];
  orthonormalize_keep_up(orientation);
}

Vec3 Camera3d::transform(const Vec3& p) const { return transform_mat34(view, p); }

Vec3 transform_mat34(const std::array<float, 12>& m, const Vec3& p) {
  Vec3 out{};
  for (std::size_t i = 0; i < 3; ++i) {
    const float* r = &m[4 * i];
    out[i] = f32(((d(r[1]) * p[1] + d(r[0]) * p[0]) + d(r[2]) * p[2]) + r[3]);
  }
  return out;
}

Vec3 mul_mat3(const std::array<float, 9>& m, const Vec3& v) {
  Vec3 out{};
  for (std::size_t i = 0; i < 3; ++i) {
    const float* r = &m[3 * i];
    out[i] = f32((d(v[1]) * r[1] + d(v[0]) * r[0]) + d(v[2]) * r[2]);
  }
  return out;
}

void rotate_x(Vec3& v, float degrees) {
  if (degrees == 0.0F) {
    return;
  }
  const double radians = d(degrees) * d(0.017453292F);
  const float c = f32(std::cos(radians));
  const double s = std::sin(radians);
  const float y = v[1];
  v[1] = f32(d(y) * c - v[2] * s);
  v[2] = f32(d(v[2]) * c + y * s);
}

void orthonormalize_keep_forward(std::array<float, 9>& b) {
  // a x b, each component stored as a float.
  const auto cross = [](const float* a, const float* c) -> Vec3 {
    return {f32(d(a[1]) * c[2] - d(a[2]) * c[1]), f32(d(a[2]) * c[0] - d(a[0]) * c[2]),
            f32(d(a[0]) * c[1] - d(a[1]) * c[0])};
  };
  const auto set_row = [&](std::size_t r, Vec3 v) {
    game::normalize(v);
    std::copy(v.begin(), v.end(), b.begin() + static_cast<std::ptrdiff_t>(3 * r));
  };
  set_row(2, {b[6], b[7], b[8]});
  set_row(0, cross(&b[3], &b[6]));
  set_row(1, cross(&b[6], &b[0]));
}

namespace {

template <typename Rotate>
void orient_rotate(std::array<float, 9>& m, float degrees, Rotate rotate) {
  if (degrees == 0.0F) {
    return;
  }
  for (const std::size_t r : {2U, 1U}) {
    Vec3 v = {m[3 * r], m[3 * r + 1], m[3 * r + 2]};
    rotate(v, degrees);
    std::copy(v.begin(), v.end(), m.begin() + static_cast<std::ptrdiff_t>(3 * r));
  }
  orthonormalize_keep_forward(m);
}

}  // namespace

void orient_rotate_y(std::array<float, 9>& basis, float degrees) { orient_rotate(basis, degrees, game::rotate_y); }

void orient_rotate_x(std::array<float, 9>& basis, float degrees) { orient_rotate(basis, degrees, rotate_x); }

double angle_between(const Vec3& a, const Vec3& b) {
  // The radian-to-degree factor (0x85fc4).
  constexpr float kRadToDeg = 57.29578F;
  float t = f32((d(a[1]) * b[1] + d(a[0]) * b[0]) + d(a[2]) * b[2]);
  t = f32(t / game::length(a));
  const double c = t / game::length(b);
  return std::acos(c) * d(kRadToDeg);
}

std::array<std::uint8_t, Camera3d::kSize> Camera3d::serialize() const {
  std::array<float, kSize / 4> f{};
  std::size_t i = 0;
  const auto put = [&](std::span<const float> v) {
    for (const float x : v) {
      f[i++] = x;
    }
  };
  put(rotation);
  put(std::array<float, 4>{fov, focal, near_plane, far_plane});
  put(position);
  put(view);
  put(std::array<float, 1>{scale});
  put(orientation);
  std::array<std::uint8_t, kSize> bytes{};
  std::memcpy(bytes.data(), f.data(), kSize);
  return bytes;
}

void Camera3d::deserialize(std::span<const std::uint8_t, kSize> bytes) {
  std::array<float, kSize / 4> f{};
  std::memcpy(f.data(), bytes.data(), kSize);
  std::size_t i = 0;
  const auto get = [&](std::span<float> v) {
    for (float& x : v) {
      x = f[i++];
    }
  };
  get(rotation);
  std::array<float, 4> projection{};
  get(projection);
  fov = projection[0];
  focal = projection[1];
  near_plane = projection[2];
  far_plane = projection[3];
  get(position);
  get(view);
  std::array<float, 1> s{};
  get(s);
  scale = s[0];
  get(orientation);
}

ScreenPoint project_point(const Vec3& v, float focal, std::int32_t cx, std::int32_t cy) {
  const double inv = 1.0 / d(v[2]);
  const double x = d(v[0]) * focal * inv;
  const double y = d(focal) * v[1] * inv;
  ScreenPoint p;
  p.x = static_cast<std::int16_t>(static_cast<std::int16_t>(game::chop(x + 0.5)) + cx);
  p.y = static_cast<std::int16_t>(static_cast<std::int16_t>(game::chop(y + 0.5)) + cy);
  return p;
}

}  // namespace oa
