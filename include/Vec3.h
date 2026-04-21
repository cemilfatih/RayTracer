#pragma once
#include <cmath>
#include <iostream>

struct Vec3 {
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float v) : x(v), y(v), z(v) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    Vec3 operator*(float s)       const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s)       const { return {x / s, y / s, z / s}; }
    Vec3 operator-()              const { return {-x, -y, -z}; }

    Vec3& operator+=(const Vec3& o) { x+=o.x; y+=o.y; z+=o.z; return *this; }
    Vec3& operator*=(float s)       { x*=s; y*=s; z*=s; return *this; }

    float dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }

    Vec3 cross(const Vec3& o) const {
        return { y*o.z - z*o.y,
                 z*o.x - x*o.z,
                 x*o.y - y*o.x };
    }

    float length()  const { return std::sqrt(x*x + y*y + z*z); }
    float length2() const { return x*x + y*y + z*z; }

    Vec3 normalized() const {
        float len = length();
        return len > 0 ? Vec3(x/len, y/len, z/len) : Vec3(0);
    }

    Vec3 clamped01() const {
        auto c = [](float v){ return v < 0 ? 0 : (v > 1 ? 1 : v); };
        return {c(x), c(y), c(z)};
    }
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

inline std::ostream& operator<<(std::ostream& os, const Vec3& v) {
    return os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
}