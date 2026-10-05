#pragma once

namespace cg {

struct Vec3 { float x, y, z; };
struct Mat4 { float v[16]{}; };

Vec3 operator-(Vec3 a, Vec3 b);
Vec3 operator+(Vec3 a, Vec3 b);
Vec3 operator*(Vec3 a, float s);
float dot(Vec3 a, Vec3 b);
Vec3 cross(Vec3 a, Vec3 b);
float length(Vec3 a);
Vec3 normalize(Vec3 a);

Mat4 identity();
Mat4 multiply(const Mat4& a, const Mat4& b);
Mat4 rotationX(float angle);
Mat4 rotationY(float angle);
Mat4 perspective(float fov, float aspect, float nearPlane, float farPlane);
Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up);

}
