// macrun CLI: Application orchestration launcher
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md
//   — "macrun Contract" section
//   — Execution Pipeline diagram
// Accepts .app, .dmg, or Mach-O binaries, runs the full orchestration pipeline,
// and outputs a structured launch plan (JSON by default).
#include <macrun.hpp>
#include <detector.hpp>
#include <iostream>
#include <cstdlib>
#include <sys/wait.h>
#include <signal.h>

namespace macrun {
    struct ProvisionOptions { bool force = false; bool verbose = false; };
    int provision_command(const std::string& app_path, const ProvisionOptions& opts);
}

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [options] <application_path>\n\n";
    std::cerr << "Orchestrates macOS application launch through the platform's execution pipeline.\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  --json           Output launch plan as JSON (default)\n";
    std::cerr << "  --yaml           Output launch plan as YAML\n";
    std::cerr << "  --diagnostics    Output full diagnostic log to stderr\n";
    std::cerr << "  --plan-only      Generate plan without launching (default behavior)\n";
    std::cerr << "  --launch          Generate plan AND execute it\n";
    std::cerr << "  --wait            Wait for launched process to exit (only with --launch)\n";
    std::cerr << "  --help, -h       Show this help\n";
    std::cerr << "\n";
    std::cerr << "Input formats:\n";
    std::cerr << "  .app bundle       macOS application bundle\n";
    std::cerr << "  .dmg image        DMG disk image (extraction deferred to tooling)\n";
    std::cerr << "  Mach-O binary     Raw executable (limited metadata)\n";
    std::cerr << "\n";
    std::cerr << "Pipeline stages:\n";
    std::cerr << "  1. Input resolution     (format detection, DMG mount, bundle extraction)\n";
    std::cerr << "  2. Bundle normalization  (Info.plist parsing)\n";
    std::cerr << "  3. Capability detection  (5-stage engine: bundle → fingerprint → macho → score → strategy)\n";
    std::cerr << "  4. Compat-db lookup      (known-compatibility records)\n";
    std::cerr << "  5. Backend selection     (tier → runtime backend mapping)\n";
    std::cerr << "  6. Runtime provisioning  (flags, env vars, shim configuration)\n";
    std::cerr << "  7. Launch plan output    (structured JSON/YAML)\n";
    std::cerr << "\n";
    std::cerr << "Exit codes:\n";
    std::cerr << "  0  Launch plan generated, application can launch\n";
    std::cerr << "  1  Input error or unrecognized format\n";
    std::cerr << "  2  Orchestration error\n";
    std::cerr << "  3  Application is unsupported or broken\n";
}

int main(int argc, char* argv[]) {
    bool json_output = true;
    bool show_diag = false;
    bool do_launch = false;
    bool do_wait = false;
    std::string app_path;

    // Check for 'provision' subcommand before normal flag parsing
    if (argc >= 2 && std::string_view(argv[1]) == "provision") {
        macrun::ProvisionOptions opts;
        std::string prov_path;
        for (int i = 2; i < argc; i++) {
            std::string_view arg(argv[i]);
            if (arg == "--force") opts.force = true;
            else if (arg == "--verbose") opts.verbose = true;
            else prov_path = argv[i];
        }
        if (prov_path.empty()) {
            std::cerr << "Usage: " << argv[0] << " provision [--force] [--verbose] <app_path>\n";
            return 1;
        }
        return macrun::provision_command(prov_path, opts);
    }

    for (int i = 1; i < argc; i++) {
        std::string_view arg(argv[i]);
        if (arg == "--json") {
            json_output = true;
        } else if (arg == "--yaml") {
            json_output = false;
        } else if (arg == "--diagnostics") {
            show_diag = true;
        } else if (arg == "--launch") {
            do_launch = true;
        } else if (arg == "--wait") {
            do_wait = true;
        } else if (arg == "--plan-only") {
            // default behavior, accepted for compatibility
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
        std::cerr << "Error: No application path provided.\n\n";
        print_usage(argv[0]);
        return 1;
    }

    // Run the full orchestration pipeline
    auto result = macrun::orchestrate(app_path);

    // Emit diagnostics to stderr if requested
    if (show_diag) {
        for (const auto& entry : result.plan.diagnostic_log) {
            std::cerr << "[macrun:diag] " << entry << "\n";
        }
    }

    // Always emit diagnostics for errors
    if (!result.success) {
        for (const auto& err : result.errors) {
            std::cerr << "[macrun:error] " << err << "\n";
        }
    }

    // Output the launch plan
    if (json_output) {
        std::cout << macrun::launch_plan_to_json(result.plan);
    } else {
        std::cout << macrun::launch_plan_to_yaml(result.plan);
    }

    // Execute the plan if --launch is set
    if (do_launch) {
        if (!result.success || !result.plan.can_launch) {
            std::cerr << "[macrun:error] Cannot launch — plan generation failed or app is unsupported\n";
            return 3;
        }

        std::cerr << "[macrun:launch] Executing launch plan " << result.plan.plan_id << "...\n";

        auto exec = macrun::execute_plan(result.plan);
        if (!exec.success) {
            std::cerr << "[macrun:error] Execution failed: " << exec.error_message << "\n";
            for (const auto& log : exec.adapter_logs) {
                std::cerr << "  " << log << "\n";
            }
            return 4;
        }

        std::cerr << "[macrun:launch] Process launched (pid=" << exec.child_pid
                  << ", backend=" << exec.backend_used << ")\n";

        if (show_diag) {
            for (const auto& log : exec.adapter_logs) {
                std::cerr << "[macrun:adapter] " << log << "\n";
            }
        }

        if (do_wait && exec.child_pid > 0) {
            std::cerr << "[macrun:launch] Waiting for process to exit...\n";
            int status;
            waitpid(exec.child_pid, &status, 0);
            if (WIFEXITED(status)) {
                std::cerr << "[macrun:launch] Process exited with code " << WEXITSTATUS(status) << "\n";
            } else if (WIFSIGNALED(status)) {
                std::cerr << "[macrun:launch] Process killed by signal " << WTERMSIG(status) << "\n";
            }
        }
    }
    if (!result.success) {
        return 2; // orchestration error
    }
    if (!result.plan.can_launch) {
        return 3; // unsupported or broken
    }

    return 0;
}
