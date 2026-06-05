// Subsystem diagnostics and logging
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md
//   — "Logging & Diagnostics" section
//   — Requires: subsystem identifier, capability state, execution tier,
//     failure classification, remediation guidance per detection event.
#pragma once
#include "types.hpp"
#include <string>
#include <functional>
#include <iostream>

namespace platform {

enum class LogLevel { Debug, Info, Warn, Error };

// Default writes to stderr. Caller can override for testing or file-based logging.
inline std::function<void(LogLevel, const std::string&)> g_log_sink = nullptr;

inline void log_diag(LogLevel level, const std::string& message) {
    if (g_log_sink) {
        g_log_sink(level, message);
    } else {
        const char* prefix = "";
        switch (level) {
            case LogLevel::Debug: prefix = "[DETECTOR:DEBUG] "; break;
            case LogLevel::Info:  prefix = "[DETECTOR:INFO]  "; break;
            case LogLevel::Warn:  prefix = "[DETECTOR:WARN]  "; break;
            case LogLevel::Error: prefix = "[DETECTOR:ERROR] "; break;
        }
        std::cerr << prefix << message << "\n";
    }
}

// Emit structured diagnostic per architecture requirements:
//   subsystem id, capability state, execution tier, failure classification, remediation
inline void log_classification(const DetectionResult& result) {
    log_diag(LogLevel::Info,
        "capability_detection tier=" + std::string(to_string(result.recommendation.preferred_tier)) +
        " state=" + std::string(to_string(result.compatibility_state)) +
        " bundle=" + result.bundle.bundle_identifier +
        " arch=" + std::string(to_string(result.macho.primary_architecture)) +
        " risk_score=" + std::to_string(result.score.risk_score_total) +
        " vm_required=" + (result.recommendation.vm_required ? "yes" : "no"));

    for (const auto& fw : result.frameworks) {
        log_diag(LogLevel::Debug,
            "detected_framework framework=" + std::string(to_string(fw.id)) +
            " impact=" + std::string(to_string(fw.impact)) +
            " evidence=" + fw.evidence);
    }

    for (const auto& warning : result.recommendation.compatibility_warnings) {
        log_diag(LogLevel::Warn,
            "compatibility_warning bundle=" + result.bundle.bundle_identifier +
            " warning=\"" + warning + "\"");
    }

    for (const auto& risk : result.recommendation.degradation_risks) {
        log_diag(LogLevel::Warn,
            "degradation_risk subsystem=" + risk.subsystem +
            " severity=" + std::string(to_string(risk.severity)) +
            " description=\"" + risk.description + "\"");
    }

    for (const auto& reason : result.unsupported_reasons) {
        log_diag(LogLevel::Error,
            "unsupported bundle=" + result.bundle.bundle_identifier +
            " reason=\"" + reason + "\"");
    }
}

} // namespace platform
