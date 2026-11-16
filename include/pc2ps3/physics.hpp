#pragma once

#include "pc2ps3/core.hpp"

namespace pc2ps3 {

struct AabbContact {
  bool overlapping;
  Vec2 normal;
  float penetration;
};

// Boxes must have finite coordinates and positive width and height; invalid boxes never overlap.
// The normal points from the first box toward the second; touching edges do not overlap.
AabbContact query_aabb(const Aabb& first, const Aabb& second);

}  // namespace pc2ps3
