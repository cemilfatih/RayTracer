#pragma once
#include "Vec3.h"

struct Ray {
    Vec3 origin;
    Vec3 direction;   // assume normalized
    int  depth;

    Ray() : depth(0) {}
    Ray(const Vec3& o, const Vec3& d, int dep = 0)
        : origin(o), direction(d), depth(dep) {}

    Vec3 at(float t) const { return origin + direction * t; }
};