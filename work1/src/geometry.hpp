#pragma once

#include "math.hpp"

#include <cstdint>
#include <vector>

namespace cg {

struct Vertex { Vec3 position; };

std::vector<Vertex> createVertices();
std::vector<uint32_t> createIndices(const std::vector<Vertex>& vertices);

}
