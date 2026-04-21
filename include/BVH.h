#pragma once
#include "Vec3.h"
#include "Ray.h"
#include "Scene.h"
#include <vector>
#include <memory>

struct AABB {
    Vec3 mn = Vec3( 1e30f);
    Vec3 mx = Vec3(-1e30f);

    void expand(const Vec3& p) {
        mn.x = std::min(mn.x, p.x); mn.y = std::min(mn.y, p.y); mn.z = std::min(mn.z, p.z);
        mx.x = std::max(mx.x, p.x); mx.y = std::max(mx.y, p.y); mx.z = std::max(mx.z, p.z);
    }
    void expand(const AABB& b) { expand(b.mn); expand(b.mx); }

    Vec3 centroid() const { return (mn + mx) * 0.5f; }
    int longestAxis() const {
        Vec3 d = mx - mn;
        if (d.x > d.y && d.x > d.z) return 0;
        return (d.y > d.z) ? 1 : 2;
    }

    // Slab test. Returns true if ray hits box in [tmin, tmax].
    bool intersect(const Ray& ray, float tmin, float tmax) const {
        for (int a = 0; a < 3; ++a) {
            float ro = (a==0)?ray.origin.x:(a==1)?ray.origin.y:ray.origin.z;
            float rd = (a==0)?ray.direction.x:(a==1)?ray.direction.y:ray.direction.z;
            float bmn= (a==0)?mn.x:(a==1)?mn.y:mn.z;
            float bmx= (a==0)?mx.x:(a==1)?mx.y:mx.z;

            float inv = 1.0f / rd;
            float t0 = (bmn - ro) * inv;
            float t1 = (bmx - ro) * inv;
            if (inv < 0) std::swap(t0, t1);
            tmin = t0 > tmin ? t0 : tmin;
            tmax = t1 < tmax ? t1 : tmax;
            if (tmax <= tmin) return false;
        }
        return true;
    }
};

// One reference to a triangle in the scene.
struct TriRef {
    int mesh_idx;
    int face_idx;
    Vec3 centroid;
    AABB box;
};

struct BVHNode {
    AABB box;
    int left  = -1;       // index in nodes array, -1 if leaf
    int right = -1;
    int first = 0;        // for leaves: start index in tri_refs
    int count = 0;        // for leaves: number of triangles
    bool isLeaf() const { return count > 0; }
};

class BVH {
public:
    std::vector<BVHNode> nodes;
    std::vector<TriRef>  tri_refs;
    static constexpr int LEAF_SIZE = 4;

    void build(const Scene& scene);

private:
    int buildRec(int start, int end);
};