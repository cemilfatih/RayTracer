#pragma once
#include "Scene.h"
#include <string>

// Renders the scene and writes a PNG to out_path.
void render(const Scene& scene, const std::string& out_path);

