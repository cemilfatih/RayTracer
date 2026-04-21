#include "Renderer.h"
#include "Ray.h"
#include "BVH.h"
#include "stb_image_write.h"
#include <iostream>
#include <limits>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>


extern bool g_use_bvh;
extern bool g_use_threads;

// Hit record for ray-triangle tests.
struct Hit {
    float t;
    Vec3  point;
    Vec3  normal;
    int   material_id;
    float beta, gamma;
    int   mesh_idx;
    int   face_idx;
};

static BVH g_bvh;

// Texture sampling. UVs in [0,1], wraps out-of-range values.
static Vec3 sampleTexture(const Scene& scene, float u, float v) {
    if (scene.texture_data.empty()) return Vec3(255, 255, 255);

    u = u - std::floor(u);
    v = v - std::floor(v);

    int W = scene.texture_width;
    int H = scene.texture_height;
    int C = scene.texture_channels;

    int px = (int)(u * (W - 1));
    int py = (int)((1.0f - v) * (H - 1));   // flip y: row 0 is top, v=0 is bottom
    if (px < 0) px = 0; if (px >= W) px = W - 1;
    if (py < 0) py = 0; if (py >= H) py = H - 1;

    int idx = (py * W + px) * C;
    float r = scene.texture_data[idx + 0];
    float g = C > 1 ? scene.texture_data[idx + 1] : r;
    float b = C > 2 ? scene.texture_data[idx + 2] : r;
    return Vec3(r, g, b);
}

// Möller-Trumbore ray-triangle intersection.
static bool intersectTriangle(const Ray& ray,
                              const Vec3& v0, const Vec3& v1, const Vec3& v2,
                              float& out_t, float& out_beta, float& out_gamma)
{
    const float EPS = 1e-7f;
    Vec3 edge1 = v1 - v0;
    Vec3 edge2 = v2 - v0;
    Vec3 h = ray.direction.cross(edge2);
    float a = edge1.dot(h);
    if (a > -EPS && a < EPS) return false;

    float f = 1.0f / a;
    Vec3 s = ray.origin - v0;
    float beta = f * s.dot(h);
    if (beta < 0.0f || beta > 1.0f) return false;

    Vec3 q = s.cross(edge1);
    float gamma = f * ray.direction.dot(q);
    if (gamma < 0.0f || beta + gamma > 1.0f) return false;

    float t = f * edge2.dot(q);
    if (t < EPS) return false;

    out_t = t; out_beta = beta; out_gamma = gamma;
    return true;
}

// BVH-accelerated closest-hit search.
static bool traceClosest(const Ray& ray, const Scene& scene, Hit& out) {
    out.t = std::numeric_limits<float>::infinity();
    bool found = false;

    if (!g_use_bvh) {
        for (size_t mi = 0; mi < scene.meshes.size(); ++mi) {
            const Mesh& mesh = scene.meshes[mi];
            for (size_t fi = 0; fi < mesh.faces.size(); ++fi) {
                const Face& f = mesh.faces[fi];
                const Vec3& v0 = scene.vertices[f.v0];
                const Vec3& v1 = scene.vertices[f.v1];
                const Vec3& v2 = scene.vertices[f.v2];
                float t, b, g;
                if (intersectTriangle(ray, v0, v1, v2, t, b, g) && t < out.t) {
                    out.t = t; out.beta = b; out.gamma = g;
                    out.point = ray.at(t);
                    if (f.n0 >= 0) out.normal = scene.normals[f.n0].normalized();
                    else out.normal = (v1 - v0).cross(v2 - v0).normalized();
                    out.material_id = mesh.material_id;
                    out.mesh_idx = (int)mi; out.face_idx = (int)fi;
                    found = true;
                }
            }
        }
        return found;
    }
    if (g_bvh.nodes.empty()) return false;

    int stack[64];
    int sp = 0;
    stack[sp++] = 0;

    while (sp > 0) {
        int ni = stack[--sp];
        const BVHNode& node = g_bvh.nodes[ni];
        if (!node.box.intersect(ray, 1e-4f, out.t)) continue;

        if (node.isLeaf()) {
            for (int i = node.first; i < node.first + node.count; ++i) {
                const TriRef& tr = g_bvh.tri_refs[i];
                const Mesh& mesh = scene.meshes[tr.mesh_idx];
                const Face& f = mesh.faces[tr.face_idx];
                const Vec3& v0 = scene.vertices[f.v0];
                const Vec3& v1 = scene.vertices[f.v1];
                const Vec3& v2 = scene.vertices[f.v2];

                float t, b, g;
                if (intersectTriangle(ray, v0, v1, v2, t, b, g) && t < out.t) {
                    out.t = t;
                    out.beta = b;
                    out.gamma = g;
                    out.point = ray.at(t);
                    if (f.n0 >= 0) {
                        out.normal = scene.normals[f.n0].normalized();
                    } else {
                        out.normal = (v1 - v0).cross(v2 - v0).normalized();
                    }
                    out.material_id = mesh.material_id;
                    out.mesh_idx = tr.mesh_idx;
                    out.face_idx = tr.face_idx;
                    found = true;
                }
            }
        } else {
            if (node.left  != -1) stack[sp++] = node.left;
            if (node.right != -1) stack[sp++] = node.right;
        }
    }
    return found;
}

// BVH-accelerated shadow ray test.
static bool inShadow(const Vec3& point, const Vec3& normal,
                     const Vec3& target, const Scene& scene)
{
    Vec3 dir = target - point;
    float dist_to_target = dir.length();
    Vec3 d = dir / dist_to_target;

    Ray shadow(point + normal * Scene::SHADOW_EPSILON, d, 0);
    float tmax = dist_to_target - Scene::SHADOW_EPSILON;

    if (g_bvh.nodes.empty()) return false;

    int stack[64];
    int sp = 0;
    stack[sp++] = 0;

    while (sp > 0) {
        int ni = stack[--sp];
        const BVHNode& node = g_bvh.nodes[ni];
        if (!node.box.intersect(shadow, 1e-4f, tmax)) continue;

        if (node.isLeaf()) {
            for (int i = node.first; i < node.first + node.count; ++i) {
                const TriRef& tr = g_bvh.tri_refs[i];
                const Mesh& mesh = scene.meshes[tr.mesh_idx];
                const Face& f = mesh.faces[tr.face_idx];
                const Vec3& v0 = scene.vertices[f.v0];
                const Vec3& v1 = scene.vertices[f.v1];
                const Vec3& v2 = scene.vertices[f.v2];
                float t, b, g;
                if (intersectTriangle(shadow, v0, v1, v2, t, b, g)) {
                    if (t < tmax) return true;
                }
            }
        } else {
            if (node.left  != -1) stack[sp++] = node.left;
            if (node.right != -1) stack[sp++] = node.right;
        }
    }
    return false;
}

// Camera ray for pixel (i, j). j=0 is TOP row.
static Ray generateRay(const Camera& C, int i, int j) {
    float su = (C.right - C.left)  * (i + 0.5f) / C.image_width;
    float sv = (C.top   - C.bottom)* (j + 0.5f) / C.image_height;

    Vec3 m = C.position + (-C.w) * C.near_distance;
    Vec3 q = m + C.u * C.left + C.v * C.top;
    Vec3 s = q + C.u * su - C.v * sv;

    Vec3 dir = (s - C.position).normalized();
    return Ray(C.position, dir, 0);
}

// Phong shading (ambient + diffuse + specular) with shadows.
static Vec3 shade(const Ray& ray, const Hit& hit, const Scene& scene) {
    const Material& M = scene.materials[hit.material_id];
    Vec3 N = hit.normal;
    Vec3 P = hit.point;
    Vec3 V = (-ray.direction).normalized();

    if (N.dot(V) < 0) N = -N;

    // Ambient
    Vec3 color = M.ambient * scene.ambient_light;

    // Point lights
    for (const PointLight& pl : scene.point_lights) {
        if (inShadow(P, N, pl.position, scene)) continue;

        Vec3 Lvec = pl.position - P;
        float r2 = Lvec.length2();
        float r  = std::sqrt(r2);
        Vec3 L = Lvec / r;
        Vec3 E = pl.intensity / r2;

        float NdotL = N.dot(L);
        if (NdotL > 0) {
            color += M.diffuse * E * NdotL;
            Vec3 H = (L + V).normalized();
            float NdotH = N.dot(H);
            if (NdotH > 0) {
                color += M.specular * E * std::pow(NdotH, M.phong_exponent);
            }
        }
    }

    // Triangular lights — direction per spec: (v1-v2) x (v1-v3)
    for (const TriangularLight& tl : scene.triangular_lights) {
        Vec3 light_dir = (tl.v1 - tl.v2).cross(tl.v1 - tl.v3).normalized();
        Vec3 L = -light_dir;

        Vec3 centroid = (tl.v1 + tl.v2 + tl.v3) * (1.0f/3.0f);
        if (inShadow(P, N, centroid, scene)) continue;

        Vec3 Lvec = centroid - P;
        float r2 = Lvec.length2();
        Vec3 E = tl.intensity / r2;

        float NdotL = N.dot(L);
        if (NdotL > 0) {
            color += M.diffuse * E * NdotL;
            Vec3 H = (L + V).normalized();
            float NdotH = N.dot(H);
            if (NdotH > 0) {
                color += M.specular * E * std::pow(NdotH, M.phong_exponent);
            }
        }
    }

    // Texture blending
    const Face& face = scene.meshes[hit.mesh_idx].faces[hit.face_idx];
    bool has_uvs = face.t0 >= 0 && face.t1 >= 0 && face.t2 >= 0
                   && !scene.texture_data.empty()
                   && M.texture_factor > 0;
    if (has_uvs) {
        const Vec3& uv0 = scene.tex_coords[face.t0];
        const Vec3& uv1 = scene.tex_coords[face.t1];
        const Vec3& uv2 = scene.tex_coords[face.t2];
        float alpha = 1.0f - hit.beta - hit.gamma;
        float u = alpha * uv0.x + hit.beta * uv1.x + hit.gamma * uv2.x;
        float v = alpha * uv0.y + hit.beta * uv1.y + hit.gamma * uv2.y;
        Vec3 tex = sampleTexture(scene, u, v);
        color = color * (1.0f - M.texture_factor) + tex * M.texture_factor;
    }

    return color;
}

// Recursive ray color: shading + mirror reflection.
static Vec3 rayColor(const Ray& ray, const Scene& scene) {
    Hit hit;
    if (!traceClosest(ray, scene, hit)) {
        return scene.background;
    }

    Vec3 color = shade(ray, hit, scene);

    const Material& M = scene.materials[hit.material_id];
    bool is_mirror = (M.mirror.x > 0 || M.mirror.y > 0 || M.mirror.z > 0);
    if (is_mirror && ray.depth < scene.max_depth) {
        Vec3 N = hit.normal;
        if (N.dot(-ray.direction) < 0) N = -N;

        Vec3 R = (ray.direction - N * (2.0f * ray.direction.dot(N))).normalized();
        Vec3 origin = hit.point + N * Scene::SHADOW_EPSILON;
        Ray reflected(origin, R, ray.depth + 1);

        Vec3 refl_color = rayColor(reflected, scene);
        color += M.mirror * refl_color;
    }

    return color;
}

// Render: build BVH, dispatch threads, write PNG.
void render(const Scene& scene, const std::string& out_path) {
    if (g_use_bvh) g_bvh.build(scene);

    const Camera& C = scene.camera;
    int W = C.image_width, H = C.image_height;
    std::vector<unsigned char> img(W * H * 3);

    auto t_start = std::chrono::high_resolution_clock::now();

    if (g_use_threads) {
        unsigned num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;
        std::cerr << "rendering " << W << "x" << H << " with " << num_threads << " threads\n";

        std::atomic<int> next_row(0);
        auto worker = [&]() {
            while (true) {
                int j = next_row.fetch_add(1);
                if (j >= H) break;
                for (int i = 0; i < W; ++i) {
                    Ray ray = generateRay(C, i, j);
                    Vec3 col = rayColor(ray, scene);
                    auto c = [](float v){ return (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v)); };
                    int idx = (j * W + i) * 3;
                    img[idx+0] = c(col.x);
                    img[idx+1] = c(col.y);
                    img[idx+2] = c(col.z);
                }
            }
        };

        std::vector<std::thread> workers;
        for (unsigned t = 0; t < num_threads; ++t) workers.emplace_back(worker);
        for (auto& th : workers) th.join();
    } else {
        std::cerr << "rendering " << W << "x" << H << " single-threaded\n";
        for (int j = 0; j < H; ++j) {
            for (int i = 0; i < W; ++i) {
                Ray ray = generateRay(C, i, j);
                Vec3 col = rayColor(ray, scene);
                auto c = [](float v){ return (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v)); };
                int idx = (j * W + i) * 3;
                img[idx+0] = c(col.x);
                img[idx+1] = c(col.y);
                img[idx+2] = c(col.z);
            }
        }
    }

    auto t_end = std::chrono::high_resolution_clock::now();
    long render_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
    std::cerr << "TIMING: render=" << render_ms << "ms\n";

    stbi_write_png(out_path.c_str(), W, H, 3, img.data(), W * 3);
    std::cout << "wrote " << out_path << "\n";
}