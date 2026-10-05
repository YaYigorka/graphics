#include "math.hpp"

#include <cmath>

namespace cg {

Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
Vec3 operator*(Vec3 a, float s) { return {a.x*s, a.y*s, a.z*s}; }
float dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
float length(Vec3 a) { return std::sqrt(dot(a, a)); }
Vec3 normalize(Vec3 a) { return a * (1.0f / length(a)); }

Mat4 identity() {
    Mat4 m{};
    m.v[0] = m.v[5] = m.v[10] = m.v[15] = 1.0f;
    return m;
}

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 result{};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            for (int k = 0; k < 4; ++k)
                result.v[col * 4 + row] += a.v[k * 4 + row] * b.v[col * 4 + k];
    return result;
}

Mat4 rotationX(float a) {
    Mat4 m = identity();
    m.v[5] = std::cos(a); m.v[9] = -std::sin(a);
    m.v[6] = std::sin(a); m.v[10] = std::cos(a);
    return m;
}

Mat4 rotationY(float a) {
    Mat4 m = identity();
    m.v[0] = std::cos(a); m.v[8] = std::sin(a);
    m.v[2] = -std::sin(a); m.v[10] = std::cos(a);
    return m;
}

Mat4 perspective(float fov, float aspect, float nearPlane, float farPlane) {
    Mat4 m{};
    const float f = 1.0f / std::tan(fov / 2.0f);
    m.v[0] = f / aspect;
    m.v[5] = -f;
    m.v[10] = farPlane / (nearPlane - farPlane);
    m.v[11] = -1.0f;
    m.v[14] = (farPlane * nearPlane) / (nearPlane - farPlane);
    return m;
}

Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up) {
    const Vec3 f = normalize(center - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);
    Mat4 m = identity();
    m.v[0] = s.x;  m.v[4] = s.y;  m.v[8] = s.z;
    m.v[1] = u.x;  m.v[5] = u.y;  m.v[9] = u.z;
    m.v[2] = -f.x; m.v[6] = -f.y; m.v[10] = -f.z;
    m.v[12] = -dot(s, eye);
    m.v[13] = -dot(u, eye);
    m.v[14] = dot(f, eye);
    return m;
}


}
