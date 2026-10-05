#include "geometry.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace cg {

std::vector<Vertex> createVertices() {
    const float phi = (1.0f + std::sqrt(5.0f)) / 2.0f;
    const float invPhi = 1.0f / phi;
    std::vector<Vertex> vertices;
    for (int x : {-1, 1})
        for (int y : {-1, 1})
            for (int z : {-1, 1})
                vertices.push_back({{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)}});
    for (int a : {-1, 1}) for (int b : {-1, 1}) {
        vertices.push_back({{0.0f, a * invPhi, b * phi}});
        vertices.push_back({{a * invPhi, b * phi, 0.0f}});
        vertices.push_back({{a * phi, 0.0f, b * invPhi}});
    }
    return vertices;
}

std::vector<uint32_t> createIndices(const std::vector<Vertex>& vertices) {
    std::set<std::vector<uint32_t>> uniqueFaces;
    const float eps = 0.0001f;
    const int n = static_cast<int>(vertices.size());

    for (int i = 0; i < n; ++i) for (int j = i + 1; j < n; ++j)
        for (int k = j + 1; k < n; ++k) {
            Vec3 a = vertices[i].position;
            Vec3 normal = cross(vertices[j].position - a, vertices[k].position - a);
            if (length(normal) < eps) continue;
            normal = normalize(normal);
            float d = dot(normal, a);
            bool positive = false, negative = false;
            std::vector<uint32_t> face;
            for (int p = 0; p < n; ++p) {
                float side = dot(normal, vertices[p].position) - d;
                if (side > eps) positive = true;
                if (side < -eps) negative = true;
                if (std::abs(side) < eps) face.push_back(static_cast<uint32_t>(p));
            }
            if (!(positive && negative) && face.size() == 5) {
                std::sort(face.begin(), face.end());
                uniqueFaces.insert(face);
            }
        }

    std::vector<uint32_t> indices;
    for (const auto& face : uniqueFaces) {
        Vec3 center{};
        for (uint32_t id : face) center = center + vertices[id].position;
        center = center * (1.0f / 5.0f);
        Vec3 normal = normalize(center);
        Vec3 axis = normalize(vertices[face[0]].position - center);
        Vec3 second = cross(normal, axis);
        std::vector<std::pair<float, uint32_t>> ordered;
        for (uint32_t id : face) {
            Vec3 delta = vertices[id].position - center;
            float angle = std::atan2(dot(delta, second), dot(delta, axis));
            ordered.push_back({angle, id});
        }
        std::sort(ordered.begin(), ordered.end());
        for (int i = 1; i < 4; ++i) {
            indices.push_back(ordered[0].second);
            indices.push_back(ordered[i].second);
            indices.push_back(ordered[i + 1].second);
        }
    }
    return indices;
}


}
