#pragma once
#include "detector.hpp"
#include <string>

namespace platform {

// Serialize DetectionResult to JSON
std::string to_json(const DetectionResult& result);

// Serialize DetectionResult to YAML (simplified — outputs valid YAML)
std::string to_yaml(const DetectionResult& result);

} // namespace platform
