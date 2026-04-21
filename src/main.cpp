#include <iostream>
#include <string>
#include "SceneLoader.h"
#include "Renderer.h"

bool g_use_bvh = true;
bool g_use_threads = true;

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " scene.xml [out.png] [--no-bvh] [--no-threads]\n";
        return 1;
    }
    std::string scene_path = argv[1];
    std::string out = "output.png";
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "--no-bvh")     g_use_bvh = false;
        else if (a == "--no-threads") g_use_threads = false;
        else                          out = a;
    }

    Scene scene;
    if (!loadScene(scene_path, scene)) return 1;
    render(scene, out);
    return 0;
}