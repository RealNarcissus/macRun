// inspector: Bundle analysis and capability probing CLI tool
// Uses the Capability Detection Engine to inspect .app bundles and produce
// structured JSON/YAML capability reports.
//
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md
//   - "Capability Detection Engine" section
//   - "Detection Pipeline" stages 1-5

#include <detector.hpp>
#include <serialize.hpp>
#include <iostream>
#include <string>
#include <string_view>
#include <cstdlib>

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [--json | --yaml] <app_bundle_path>\n";
    std::cerr << "\n";
    std::cerr << "Options:\n";
    std::cerr << "  --json    Output capability report as JSON (default)\n";
    std::cerr << "  --yaml    Output capability report as YAML\n";
    std::cerr << "\n";
    std::cerr << "Inspects a macOS .app bundle through the 5-stage detection pipeline:\n";
    std::cerr << "  Stage 1 - Bundle Analysis (Info.plist parsing)\n";
    std::cerr << "  Stage 2 - Framework Fingerprinting (Electron, Tauri, AppKit, SwiftUI, etc.)\n";
    std::cerr << "  Stage 3 - Architecture Analysis (Mach-O headers, universal/fat binary)\n";
    std::cerr << "  Stage 4 - Capability Scoring (weighted risk dimensions)\n";
    std::cerr << "  Stage 5 - Execution Strategy Resolution (tier recommendation)\n";
}

int main(int argc, char* argv[]) {
    bool json_output = true;
    std::string app_path;

    for (int i = 1; i < argc; i++) {
        std::string_view arg(argv[i]);
        if (arg == "--json") {
            json_output = true;
        } else if (arg == "--yaml") {
            json_output = false;
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg.starts_with("-")) {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        } else {
            app_path = argv[i];
        }
    }

    if (app_path.empty()) {
        std::cerr << "Error: No .app bundle path provided.\n\n";
        print_usage(argv[0]);
        return 1;
    }

    platform::DetectionResult result = platform::detect_capabilities(app_path);

    if (json_output) {
        std::cout << platform::to_json(result);
    } else {
        std::cout << platform::to_yaml(result);
    }

    // Exit code reflects compatibility state (useful for scripting)
    switch (result.compatibility_state) {
        case platform::CompatibilityState::Unsupported: return 4;
        case platform::CompatibilityState::Broken:      return 3;
        case platform::CompatibilityState::Degraded:    return 2;
        case platform::CompatibilityState::Partial:     return 1;
        default: break;
    }

    return 0;
}
