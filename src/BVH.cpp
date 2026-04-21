#include "BVH.h"
#include <algorithm>
#include <iostream>

void BVH::build(const Scene& scene) {
    tri_refs.clear();
    nodes.clear();

    // Collect all triangles.
    for (size_t mi = 0; mi < scene.meshes.size(); ++mi) {
        const Mesh& mesh = scene.meshes[mi];
        for (size_t fi = 0; fi < mesh.faces.size(); ++fi) {
            const Face& f = mesh.faces[fi];
            const Vec3& v0 = scene.vertices[f.v0];
            const Vec3& v1 = scene.vertices[f.v1];
            const Vec3& v2 = scene.vertices[f.v2];
            TriRef r;
            r.mesh_idx = (int)mi;
            r.face_idx = (int)fi;
            r.box.expand(v0); r.box.expand(v1); r.box.expand(v2);
            r.centroid = (v0 + v1 + v2) * (1.0f/3.0f);
            tri_refs.push_back(r);
        }
    }

    nodes.reserve(tri_refs.size() * 2);
    buildRec(0, (int)tri_refs.size());
    std::cerr << "BVH: " << tri_refs.size() << " tris, "
              << nodes.size() << " nodes\n";
}

int BVH::buildRec(int start, int end) {
    int node_idx = (int)nodes.size();
    nodes.emplace_back();
    // IMPORTANT: after emplace_back, any reference to nodes[node_idx] is unsafe
    // if we recurse (vector may reallocate). So we write to a local first.

    AABB box;
    for (int i = start; i < end; ++i) box.expand(tri_refs[i].box);

    int count = end - start;
    if (count <= LEAF_SIZE) {
        nodes[node_idx].box   = box;
        nodes[node_idx].first = start;
        nodes[node_idx].count = count;
        return node_idx;
    }

    // Compute centroid bounds, split along longest axis at centroid median.
    AABB cb;
    for (int i = start; i < end; ++i) cb.expand(tri_refs[i].centroid);
    int axis = cb.longestAxis();

    int mid = start + count / 2;
    std::nth_element(tri_refs.begin() + start,
                     tri_refs.begin() + mid,
                     tri_refs.begin() + end,
                     [axis](const TriRef& a, const TriRef& b){
                         float av = (axis==0)?a.centroid.x:(axis==1)?a.centroid.y:a.centroid.z;
                         float bv = (axis==0)?b.centroid.x:(axis==1)?b.centroid.y:b.centroid.z;
                         return av < bv;
                     });

    int l = buildRec(start, mid);
    int r = buildRec(mid, end);

    nodes[node_idx].box   = box;
    nodes[node_idx].left  = l;
    nodes[node_idx].right = r;
    nodes[node_idx].count = 0;  // internal
    return node_idx;
}