// The 3D camera of the research tree (and of the other 3D screens): the
// `Camera` object (0x98 bytes, `Camera_ctor` 0x1b4f0, saved with the game by
// `ResWnd_serialize` 0x44374) and the vector helpers it uses, with the
// original's x87 arithmetic: every intermediate a double (53-bit precision),
// rounded to a `float` only where the original stores it
// (`docs/11-screens.md`, "3D camera helpers").
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "game/float_math.h"

namespace oa {

using Vec3 = game::Vec3;

struct Camera3d {
  // The serialized size.
  static constexpr std::size_t kSize = 0x98;

  std::array<float, 9> rotation{1, 0, 0, 0, 1, 0, 0, 0, 1};        // +0x00 3x3, row-major
  float fov = 0;                                                   // +0x24 degrees
  float focal = 0;                                                 // +0x28 n / tan(fov / 2)
  float near_plane = 0;                                            // +0x2c
  float far_plane = 0;                                             // +0x30
  Vec3 position{};                                                 // +0x34
  std::array<float, 12> view{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};  // +0x40 3x4
  float scale = 0;                                                 // +0x70
  // +0x74 the orientation basis: rows right, up, forward.
  std::array<float, 9> orientation{1, 0, 0, 0, 1, 0, 0, 0, 1};

  // `Camera_set_projection` 0x1b808 then `Camera_set_fov` 0x1b834: the
  // field of view `fov` (degrees), the clip planes, `scale` 1.0 and
  // `focal = n / tan(fov * pi / 360)`.
  void set_projection(float fov_degrees, float near_z, float far_z, std::int32_t n);
  // `Camera_update_view` 0x1b864 (`orient_build_view_matrix` 0x4ef18):
  // `view = [rotation * orientation^T | -rotation * position]`.
  void update_view();
  // `orient_rotate_forward_y` 0x4e974: turns the forward vector about the
  // vertical axis by `degrees` (nothing for +-0), then re-orthonormalises the
  // basis keeping the up vector (`orient_orthonormalize_keep_up` 0x4eb64).
  void rotate_forward_y(float degrees);
  // `vec3_transform_mat34` 0x4e794 of `p` by the view matrix.
  Vec3 transform(const Vec3& p) const;

  // The 0x98 bytes of the save (little-endian floats in field order).
  std::array<std::uint8_t, kSize> serialize() const;
  void deserialize(std::span<const std::uint8_t, kSize> bytes);
};

// `project_point` 0x4e7e4: `chop(v.x * focal * (1 / v.z) + 0.5) + cx` and the
// same for y (no flip: +y is down), as 16-bit values.
struct ScreenPoint {
  std::int16_t x = 0;
  std::int16_t y = 0;
};
ScreenPoint project_point(const Vec3& v, float focal, std::int32_t cx, std::int32_t cy);

}  // namespace oa
