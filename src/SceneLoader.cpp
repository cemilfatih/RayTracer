#include "SceneLoader.h"
#include "tinyxml2.h"
#include "stb_image.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <cctype>
#include <map>

using namespace tinyxml2;


// Case insensitive child lookup. The spec is inconsistent
static XMLElement* child(XMLElement* parent, const char* name) {
    if (!parent) return nullptr;
    for (XMLElement* e = parent->FirstChildElement(); e; e = e->NextSiblingElement()) {
        if (strcasecmp(e->Name(), name) == 0) return e;
    }
    return nullptr;
}

// Read text of an element as a stream of floats.
static std::vector<float> readFloats(XMLElement* e) {
    std::vector<float> out;
    if (!e || !e->GetText()) return out;
    std::istringstream iss(e->GetText());
    float v;
    while (iss >> v) out.push_back(v);
    return out;
}

static Vec3 readVec3(XMLElement* e, Vec3 fallback = Vec3(0)) {
    auto f = readFloats(e);
    if (f.size() < 3) return fallback;
    return { f[0], f[1], f[2] };
}

static float readFloat(XMLElement* e, float fallback = 0) {
    if (!e || !e->GetText()) return fallback;
    return std::stof(e->GetText());
}

static int readInt(XMLElement* e, int fallback = 0) {
    if (!e || !e->GetText()) return fallback;
    return std::stoi(e->GetText());
}

static void parseFaceToken(const std::string& tok, int& v, int& t, int& n) {
    v = t = n = -1;
    size_t p1 = tok.find('/');
    if (p1 == std::string::npos) { v = std::stoi(tok) - 1; return; }
    v = std::stoi(tok.substr(0, p1)) - 1;
    size_t p2 = tok.find('/', p1 + 1);
    if (p2 == std::string::npos) {
        if (p1 + 1 < tok.size()) t = std::stoi(tok.substr(p1 + 1)) - 1;
        return;
    }
    if (p2 > p1 + 1) t = std::stoi(tok.substr(p1 + 1, p2 - p1 - 1)) - 1;
    if (p2 + 1 < tok.size()) n = std::stoi(tok.substr(p2 + 1)) - 1;
}

// --- main loader -----------------------------------------------------------

bool loadScene(const std::string& path, Scene& scene) {
    XMLDocument doc;
    if (doc.LoadFile(path.c_str()) != XML_SUCCESS) {
        std::cerr << "XML load failed: " << path << "\n";
        return false;
    }
    XMLElement* root = doc.FirstChildElement("scene");
    if (!root) { std::cerr << "<scene> missing\n"; return false; }

    // header
    scene.max_depth  = readInt (child(root, "maxraytracedepth"), 5);
    // Spec uses both "background" and "backgroundColor" in different places.
    XMLElement* bg = child(root, "background");
    if (!bg) bg = child(root, "backgroundcolor");
    scene.background = readVec3(bg, Vec3(0));

    //camera 
    XMLElement* cam = child(root, "camera");
    if (!cam) { std::cerr << "<camera> missing\n"; return false; }
    Camera& C = scene.camera;
    C.position      = readVec3(child(cam, "position"));
    C.gaze          = readVec3(child(cam, "gaze")).normalized();
    C.up            = readVec3(child(cam, "up")).normalized();
    C.near_distance = readFloat(child(cam, "neardistance"), 1.0f);

    auto np = readFloats(child(cam, "nearplane"));
    if (np.size() >= 4) {
        C.left = np[0]; C.right = np[1]; C.bottom = np[2]; C.top = np[3];
    }
    auto res = readFloats(child(cam, "imageresolution"));
    if (res.size() >= 2) {
        C.image_width  = (int)res[0];
        C.image_height = (int)res[1];
    }

    // Camera basis 
    C.w = (-C.gaze).normalized();
    C.u = C.up.cross(C.w).normalized();
    C.v = C.w.cross(C.u).normalized();

    // --- lights ---
    XMLElement* lights = child(root, "lights");
    if (lights) {
        if (auto* a = child(lights, "ambientlight"))
            scene.ambient_light = readVec3(a);
        for (XMLElement* e = lights->FirstChildElement(); e; e = e->NextSiblingElement()) {
            if (strcasecmp(e->Name(), "pointlight") == 0) {
                PointLight pl;
                pl.position  = readVec3(child(e, "position"));
                pl.intensity = readVec3(child(e, "intensity"));
                scene.point_lights.push_back(pl);
            } else if (strcasecmp(e->Name(), "triangularlight") == 0) {
                TriangularLight tl;
                tl.v1        = readVec3(child(e, "vertex1"));
                tl.v2        = readVec3(child(e, "vertex2"));
                tl.v3        = readVec3(child(e, "vertex3"));
                tl.intensity = readVec3(child(e, "intensity"));
                scene.triangular_lights.push_back(tl);
            }
        }
    }

    std::map<std::string, int> material_id_map;  // ADD THIS LINE (need #include <map> at top)
    if (XMLElement* mats = child(root, "materials")) {
        for (XMLElement* m = mats->FirstChildElement("material"); m;
             m = m->NextSiblingElement("material")) {
            Material mat;
            mat.ambient        = readVec3 (child(m, "ambient"));
            mat.diffuse        = readVec3 (child(m, "diffuse"));
            mat.specular       = readVec3 (child(m, "specular"));
            mat.mirror         = readVec3 (child(m, "mirrorreflectance"));
            mat.phong_exponent = readFloat(child(m, "phongexponent"), 1.0f);
            mat.texture_factor = readFloat(child(m, "texturefactor"), 0.0f);
            const char* id_attr = m->Attribute("id");
            std::string id = id_attr ? id_attr : std::to_string(scene.materials.size() + 1);
            material_id_map[id] = (int)scene.materials.size();
            scene.materials.push_back(mat);
        }
    }

    auto vdata = readFloats(child(root, "vertexdata"));
    for (size_t i = 0; i + 2 < vdata.size(); i += 3)
        scene.vertices.emplace_back(vdata[i], vdata[i+1], vdata[i+2]);

    auto tdata = readFloats(child(root, "texturedata"));
    // spec says "v u" per line - but this is ambiguous. Store as-is in (x,y).
    for (size_t i = 0; i + 1 < tdata.size(); i += 2)
        scene.tex_coords.emplace_back(tdata[i], tdata[i+1], 0.0f);

    auto ndata = readFloats(child(root, "normaldata"));
    for (size_t i = 0; i + 2 < ndata.size(); i += 3)
        scene.normals.emplace_back(ndata[i], ndata[i+1], ndata[i+2]);

    if (XMLElement* ti = child(root, "textureimage")) {
        if (ti->GetText()) {
            scene.texture_filename = ti->GetText();
            // Try to load relative to scene file.
            std::string dir = path.substr(0, path.find_last_of('/') + 1);
            std::string full = dir + scene.texture_filename;
            unsigned char* data = stbi_load(full.c_str(),
                &scene.texture_width, &scene.texture_height,
                &scene.texture_channels, 0);
            if (data) {
                size_t sz = size_t(scene.texture_width) * scene.texture_height * scene.texture_channels;
                scene.texture_data.assign(data, data + sz);
                stbi_image_free(data);
            } else {
                std::cerr << "warn: texture load failed: " << full << "\n";
            }
        }
    }

    // Material id in XML is a string; we'll treat it as a 1-based index for simplicity.
    // (If the scene uses arbitrary string ids we'd need a name->index map.)
    if (XMLElement* objs = child(root, "objects")) {
        for (XMLElement* m = objs->FirstChildElement(); m; m = m->NextSiblingElement()) {
            if (strcasecmp(m->Name(), "mesh") != 0) continue;
            Mesh mesh;
            std::string mid_str;
            if (XMLElement* mid_e = child(m, "materialid")) {
                if (mid_e->GetText()) mid_str = mid_e->GetText();
            }
            // trim whitespace
            while (!mid_str.empty() && std::isspace((unsigned char)mid_str.front())) mid_str.erase(0,1);
            while (!mid_str.empty() && std::isspace((unsigned char)mid_str.back()))  mid_str.pop_back();
            
            auto it = material_id_map.find(mid_str);
            if (it != material_id_map.end()) {
                mesh.material_id = it->second;
            } else {
                // Fallback: try as integer (1-based)
                try {
                    mesh.material_id = std::stoi(mid_str) - 1;
                } catch (...) {
                    mesh.material_id = 0;
                }
            }

            if (XMLElement* faces = child(m, "faces")) {
                if (faces->GetText()) {
                    std::istringstream iss(faces->GetText());
                    std::string tok;
                    std::vector<std::string> toks;
                    while (iss >> tok) toks.push_back(tok);
                    // Every 3 tokens = 1 face.
                    for (size_t i = 0; i + 2 < toks.size(); i += 3) {
                        Face f;
                        parseFaceToken(toks[i],   f.v0, f.t0, f.n0);
                        parseFaceToken(toks[i+1], f.v1, f.t1, f.n1);
                        parseFaceToken(toks[i+2], f.v2, f.t2, f.n2);
                        mesh.faces.push_back(f);
                    }
                }
            }
            scene.meshes.push_back(std::move(mesh));
        }
    }

    std::cout << "Loaded: "
              << scene.vertices.size() << " verts, "
              << scene.normals.size() << " normals, "
              << scene.materials.size() << " materials, "
              << scene.meshes.size() << " meshes, ";
    size_t tri_count = 0;
    for (auto& mm : scene.meshes) tri_count += mm.faces.size();
    std::cout << tri_count << " triangles\n";
    return true;
}