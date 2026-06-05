// compdb-query: Compatibility Database Query CLI
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md — "compat-db Contract"
// Supports: lookup by bundle ID, filter by state/tier, search by name,
// list all records, and validate individual records.
#include <compatdb/database.hpp>
#include <compatdb/types.hpp>
#include <compatdb/validator.hpp>
#include <iostream>
#include <cstdlib>

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <command> [options]\n\n";
    std::cerr << "Commands:\n";
    std::cerr << "  load <directory>              Load records from JSON directory\n";
    std::cerr << "  lookup <id|bundle-id>         Exact record lookup by ID or bundle identifier\n";
    std::cerr << "  search <name>                 Search by application name (substring)\n";
    std::cerr << "  filter-state <state>          Filter by compatibility state\n";
    std::cerr << "  filter-tier <tier>            Filter by execution tier\n";
    std::cerr << "  filter-tag <tag>              Filter by tag\n";
    std::cerr << "  list                          List all loaded records\n";
    std::cerr << "  stats                         Database statistics\n";
    std::cerr << "  validate <file>               Validate a single record JSON file\n";
    std::cerr << "\n";
    std::cerr << "States:  verified, functional, partial, degraded, unsupported, broken\n";
    std::cerr << "Tiers:   native-substitution, darling-compatible, vm-recommended, unsupported\n";
}

static void print_record(const compatdb::CompatibilityRecord& rec) {
    std::cout << "  Record:      " << rec.record_id << "\n";
    std::cout << "  Bundle:      " << rec.bundle_identifier << "\n";
    std::cout << "  Name:        " << rec.application_name;
    if (!rec.application_version.empty())
        std::cout << " v" << rec.application_version;
    std::cout << "\n";
    std::cout << "  State:       " << compatdb::state_to_string(rec.compatibility_state) << "\n";
    std::cout << "  Tier:        " << compatdb::tier_to_string(rec.execution_tier) << "\n";
    std::cout << "  Updated:     " << rec.last_updated << "\n";

    if (!rec.tags.empty()) {
        std::cout << "  Tags:       ";
        for (size_t i = 0; i < rec.tags.size(); i++) {
            if (i) std::cout << ", ";
            std::cout << rec.tags[i];
        }
        std::cout << "\n";
    }

    if (!rec.known_issues.empty()) {
        std::cout << "  Issues (" << rec.known_issues.size() << "):\n";
        for (const auto& issue : rec.known_issues) {
            std::cout << "    [" << compatdb::severity_to_string(issue.severity) << "] "
                      << issue.id << ": " << issue.description << "\n";
        }
    }

    if (!rec.workarounds.empty()) {
        std::cout << "  Workarounds (" << rec.workarounds.size() << "):\n";
        for (const auto& w : rec.workarounds) {
            std::cout << "    - " << w.description << "\n";
        }
    }

    if (!rec.notes.empty())
        std::cout << "  Notes:       " << rec.notes << "\n";

    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string cmd = argv[1];
    compatdb::CompatDatabase db;

    // Commands that don't require loading
    if (cmd == "validate" && argc >= 3) {
        if (db.load_file(argv[2])) {
            std::cout << "VALID: " << argv[2] << "\n";
            return 0;
        } else {
            std::cout << "INVALID: " << argv[2] << "\n";
            for (const auto& err : db.load_errors()) {
                std::cout << "  " << err.first << ": " << err.second << "\n";
            }
            return 1;
        }
    }

    // Commands that require loading a data directory
    if (cmd == "load") {
        if (argc < 3) {
            std::cerr << "Error: load requires a directory path\n";
            return 1;
        }
        size_t count = db.load_directory(argv[2]);
        std::cout << "Loaded " << count << " records from " << argv[2] << "\n";
        std::cout << "Valid: " << db.valid_count() << ", Invalid: " << db.invalid_count() << "\n";

        if (!db.load_errors().empty()) {
            std::cout << "Load errors:\n";
            for (const auto& err : db.load_errors()) {
                std::cout << "  " << err.first << ": " << err.second << "\n";
            }
        }
        return db.invalid_count() > 0 ? 1 : 0;
    }

    // All remaining commands need a pre-loaded DB directory as second arg
    if (argc < 3) {
        std::cerr << "Error: Missing data directory path\n";
        print_usage(argv[0]);
        return 1;
    }

    std::string data_dir = argv[2];
    size_t count = db.load_directory(data_dir);
    if (count == 0) {
        std::cerr << "No valid records found in " << data_dir << "\n";
        if (!db.load_errors().empty()) {
            for (const auto& err : db.load_errors())
                std::cerr << "  " << err.second << "\n";
        }
        return 1;
    }

    if (cmd == "lookup" && argc >= 4) {
        std::string key = argv[3];
        // Try exact ID first
        auto* rec = db.lookup_by_id(key);
        if (rec) {
            print_record(*rec);
            return 0;
        }
        // Try bundle ID
        auto results = db.lookup_by_bundle_id(key);
        if (results.empty()) {
            // Try pattern match
            results = db.lookup_by_bundle_pattern(key);
        }
        for (const auto& r : results) {
            print_record(r.record);
        }
        if (results.empty())
            std::cout << "No records found for '" << key << "'\n";
        return results.empty() ? 1 : 0;

    } else if (cmd == "search" && argc >= 4) {
        auto results = db.search_by_name(argv[3]);
        for (const auto& r : results) print_record(r.record);
        std::cout << results.size() << " result(s)\n";
        return results.empty() ? 1 : 0;

    } else if (cmd == "filter-state" && argc >= 4) {
        auto state = compatdb::state_from_string(argv[3]);
        auto results = db.filter_by_state(state);
        for (const auto& r : results) print_record(r.record);
        std::cout << results.size() << " result(s)\n";
        return results.empty() ? 1 : 0;

    } else if (cmd == "filter-tier" && argc >= 4) {
        auto tier = compatdb::tier_from_string(argv[3]);
        auto results = db.filter_by_tier(tier);
        for (const auto& r : results) print_record(r.record);
        std::cout << results.size() << " result(s)\n";
        return results.empty() ? 1 : 0;

    } else if (cmd == "filter-tag" && argc >= 4) {
        auto results = db.filter_by_tag(argv[3]);
        for (const auto& r : results) print_record(r.record);
        std::cout << results.size() << " result(s)\n";
        return results.empty() ? 1 : 0;

    } else if (cmd == "list") {
        auto results = db.all_records();
        for (const auto& r : results) print_record(r.record);
        std::cout << results.size() << " result(s)\n";

    } else if (cmd == "stats") {
        std::cout << "Total records:  " << db.record_count() << "\n";
        std::cout << "Valid:          " << db.valid_count() << "\n";
        std::cout << "Invalid:        " << db.invalid_count() << "\n\n";

        std::cout << "By state:\n";
        for (int s = 0; s <= 5; s++) {
            auto state = static_cast<compatdb::CompatibilityState>(s);
            auto cnt = db.filter_by_state(state).size();
            if (cnt > 0)
                std::cout << "  " << compatdb::state_to_string(state) << ": " << cnt << "\n";
        }

        std::cout << "\nBy tier:\n";
        for (int t = 0; t <= 3; t++) {
            auto tier = static_cast<compatdb::ExecutionTier>(t);
            auto cnt = db.filter_by_tier(tier).size();
            if (cnt > 0)
                std::cout << "  " << compatdb::tier_to_string(tier) << ": " << cnt << "\n";
        }

    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
