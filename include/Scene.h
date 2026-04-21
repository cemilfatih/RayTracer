#pragma once
#include "Vec3.h"
#include <string>
#include <vector>

struct Camera {
    Vec3 position;
    Vec3 gaze;
    Vec3 up;
    float left, right, bottom, top;
    float near_distance;
    int image_width, image_height;
    Vec3 u, v, w;  // computed basis
};

struct Material {
    Vec3 ambient;
    Vec3 diffuse;
    Vec3 specular;
    Vec3 mirror;
    float phong_exponent;
    float texture_factor;
};

struct PointLight {
    Vec3 position;
    Vec3 intensity;
};

struct TriangularLight {
    Vec3 v1, v2, v3;
    Vec3 intensity;
};

struct Face {
    int v0, v1, v2;
    int t0, t1, t2;
    int n0, n1, n2;
};

struct Mesh {
    int material_id;
    std::vector<Face> faces;
};

struct Scene {
    int max_depth = 5;
    Vec3 background = Vec3(0);

    Camera camera;
    Vec3 ambient_light = Vec3(0);
    std::vector<PointLight>       point_lights;
    std::vector<TriangularLight>  triangular_lights;

    std::vector<Material> materials;

    std::vector<Vec3> vertices;
    std::vector<Vec3> tex_coords;
    std::vector<Vec3> normals;

    std::vector<Mesh> meshes;

    std::string texture_filename;
    std::vector<unsigned char> texture_data;
    int texture_width = 0, texture_height = 0, texture_channels = 0;

    static constexpr float SHADOW_EPSILON = 1e-3f;
};