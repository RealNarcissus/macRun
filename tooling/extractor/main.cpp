// extractor: DMG and .app bundle extraction utility
// Architecture: docs/architecture/ARCHITECTURE_V6.md — Tier 0
//
// CLI tool for extracting .asar archives from Electron .app bundles.
// Uses Node.js 'asar' package — the authoritative ASAR implementation.
//
// Usage: extractor [--list | --extract-to <dir>] <asar_file>
//   --list        List contents of the .asar archive
//   --extract-to  Extract archive to specified directory

#include <iostream>
#include <string>
#include <string_view>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options] <asar_file>\n\n";
    std::cerr << "Extract or inspect Electron .asar archives.\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  --list              List contents of the .asar archive\n";
    std::cerr << "  --extract-to <dir>  Extract archive to specified directory\n";
    std::cerr << "  --help, -h          Show this help\n";
}

static bool run_command(const std::string& cmd, std::string& output) {
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return false;

    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) {
        output += buf;
    }
    int rc = pclose(pipe);
    return rc == 0;
}

int main(int argc, char* argv[]) {
    bool list_mode = false;
    std::string extract_dir;
    std::string asar_file;

    for (int i = 1; i < argc; i++) {
        std::string_view arg(argv[i]);
        if (arg == "--list") {
            list_mode = true;
        } else if (arg == "--extract-to") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --extract-to requires a directory argument\n";
                return 1;
            }
            extract_dir = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg.starts_with("-")) {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        } else {
            asar_file = argv[i];
        }
    }

    if (asar_file.empty()) {
        std::cerr << "Error: No .asar file specified.\n\n";
        print_usage(argv[0]);
        return 1;
    }

    if (!fs::exists(asar_file)) {
        std::cerr << "Error: File not found: " << asar_file << "\n";
        return 1;
    }

    std::string cmd;
    if (list_mode) {
        cmd = "npx --yes @electron/asar list \"" + asar_file + "\" 2>&1";
        std::cout << "Listing contents of: " << asar_file << "\n";
        std::cout << "---\n";
    } else if (!extract_dir.empty()) {
        fs::create_directories(extract_dir);
        cmd = "npx --yes @electron/asar extract \"" + asar_file + "\" \"" + extract_dir + "\" 2>&1";
        std::cout << "Extracting: " << asar_file << " → " << extract_dir << "\n";
    } else {
        // Default: list mode
        cmd = "npx --yes @electron/asar list \"" + asar_file + "\" 2>&1";
        std::cout << "Listing contents of: " << asar_file << "\n";
        std::cout << "---\n";
    }

    std::string output;
    if (!run_command(cmd, output)) {
        std::cerr << "Error: ASAR operation failed.\n" << output << "\n";
        return 1;
    }

    std::cout << output;

    if (!extract_dir.empty()) {
        std::cout << "Extraction complete.\n";
    }

    return 0;
}
