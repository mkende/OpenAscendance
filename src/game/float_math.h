// The original's floating-point arithmetic. The Watcom start-up loads the
// x87 control word 0x127f (0x89bb4, CRT FPU init 0x75217): 53-bit precision,
// round to nearest. Every intermediate is therefore an IEEE double, rounded to
// a `float` only where the original stores it (`fstp dword`); `sqrt`, `sin`
// and `cos` are the `fsqrt`, `fsin` and `fcos` instructions. The helpers below
// reproduce the original's vector routines operation by operation, operand
// order included (it decides the double roundings).
#pragma once

#include <array>
#include <cstdint>

namespace oa::game {

// A position or direction as the original stores it: three `float`s.
using Vec3 = std::array<float, 3>;

// Returns `v` stored into a `float` variable.
constexpr float f32(double v) { return static_cast<float>(v); }

// Returns the Watcom `__CHP` (0x722a8) + `fistp` conversion of `v`: toward
// zero, the x87 "integer indefinite" 0x80000000 when out of `int` range.
std::int32_t chop(double v);

// Returns `a - b`, each component stored as a float (the distance code's
// `fld a; fsub b; fstp`).
Vec3 sub(const Vec3& a, const Vec3& b);

// Returns `sqrt((d.y * d.y + d.x * d.x) + d.z * d.z)` in double: the order
// of every distance computation of the galaxy code (`vec3_normalize`
// 0x4e410, the lane and cluster distances).
double length(const Vec3& d);

// Returns `chop(length(sub(a, b)))`: the integer distance between two stars
// (`Cosmos_link_nearest_stars` 0x1f290, `Cosmos_link_clusters` 0x1f8fc).
std::int32_t chopped_distance(const Vec3& a, const Vec3& b);

// Divides `v` by its length when the length (stored as a float) is
// positive: `vec3_normalize` 0x4e410.
void normalize(Vec3& v);

// Normalizes `v` then scales it by `len` (each component stored as a float):
// `vec3_set_length` 0x4e464.
void set_length(Vec3& v, float len);

// Rotates `v` by `degrees` around the y axis (x' = z sin + x cos, z' = z cos
// - x sin, cos stored as a float); nothing when `degrees` is +-0:
// `vec3_rotate_y` 0x4e6bc.
void rotate_y(Vec3& v, float degrees);

// Rounds each component to the nearest multiple of `grid`, halves away from
// zero (`chop(c +- grid / 2) / grid * grid`): `vec3_snap_to_grid` 0x4e850
// with the grid global 0x86a14.
void snap_to_grid(Vec3& v, std::int32_t grid);

}  // namespace oa::game
