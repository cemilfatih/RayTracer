#pragma once
#include "Scene.h"
#include <string>

// Parses the XML scene file. Returns true on success, false + prints error on failure.
bool loadScene(const std::string& xml_path, Scene& scene);