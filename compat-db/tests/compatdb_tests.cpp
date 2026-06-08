// Unit tests for compat-db: schema validation, query engine, state transitions
#include <compatdb/types.hpp>
#include <compatdb/validator.hpp>
#include <compatdb/database.hpp>
#include <compatdb/native_registry.hpp>
#include <iostream>
#include <fstream>
#include <cassert>
#include <unistd.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { std::cout << "  TEST: " << name << " ... "; } while(0)
#define PASS() do { std::cout << "PASSED\n"; tests_passed++; } while(0)
#define FAIL(msg) do { std::cout << "FAILED: " << msg << "\n"; tests_failed++; return; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); } } while(0)

static compatdb::CompatibilityRecord make_valid_record() {
    compatdb::CompatibilityRecord rec;
    rec.schema_version     = "1.0.0";
    rec.record_id          = "com.test.app";
    rec.bundle_identifier  = "com.test.app";
    rec.application_name   = "TestApp";
    rec.execution_tier     = compatdb::ExecutionTier::NativeSubstitution;
    rec.compatibility_state = compatdb::CompatibilityState::Functional;
    rec.last_updated       = "2026-06-04T00:00:00Z";
    return rec;
}

static void test_valid_record_passes() {
    TEST("Valid record passes schema validation");
    auto rec = make_valid_record();
    auto result = compatdb::validate_record(rec);
    CHECK(result.valid, "Valid record should pass validation");
    PASS();
}

static void test_missing_required_fields() {
    TEST("Missing required fields are detected");
    compatdb::CompatibilityRecord rec;
    rec.schema_version = "1.0.0"; // partial: only this set
    auto result = compatdb::validate_record(rec);
    CHECK(!result.valid, "Record with missing fields should fail");
    bool has_record_id_err = false, has_bundle_err = false, has_name_err = false, has_date_err = false;
    for (const auto& e : result.errors) {
        if (e.field == "record_id" && e.message.find("Missing") != std::string::npos) has_record_id_err = true;
        if (e.field == "bundle_identifier" && e.message.find("Missing") != std::string::npos) has_bundle_err = true;
        if (e.field == "application_name" && e.message.find("Missing") != std::string::npos) has_name_err = true;
        if (e.field == "last_updated" && e.message.find("Missing") != std::string::npos) has_date_err = true;
    }
    CHECK(has_record_id_err, "Should detect missing record_id");
    CHECK(has_bundle_err, "Should detect missing bundle_identifier");
    CHECK(has_name_err, "Should detect missing application_name");
    CHECK(has_date_err, "Should detect missing last_updated");
    PASS();
}

static void test_invalid_schema_version() {
    TEST("Invalid schema version format is rejected");
    auto rec = make_valid_record();
    rec.schema_version = "bad";
    auto result = compatdb::validate_record(rec);
    CHECK(!result.valid, "Invalid schema version should fail");
    PASS();
}

static void test_record_id_validation() {
    TEST("Record ID with invalid characters is rejected");
    auto rec = make_valid_record();
    rec.record_id = "bad id with spaces!";
    auto result = compatdb::validate_record(rec);
    CHECK(!result.valid, "Record ID with spaces should fail");
    PASS();
}

static void test_bundle_id_warns_without_dots() {
    TEST("Bundle identifier without dots triggers warning");
    auto rec = make_valid_record();
    rec.bundle_identifier = "noreverse-dns";
    auto result = compatdb::validate_record(rec);
    // Should still be valid (warning, not error) but have a warning
    CHECK(result.valid, "Bundle ID without dots should warn, not error");
    bool has_warning = false;
    for (const auto& e : result.errors) {
        if (e.level == compatdb::ValidationError::Level::Warning) has_warning = true;
    }
    CHECK(has_warning, "Should emit a warning for non-reverse-DNS bundle ID");
    PASS();
}

static void test_state_transition_verified_with_critical_issue() {
    TEST("Verified state with unresolved critical issue is rejected");
    auto rec = make_valid_record();
    rec.compatibility_state = compatdb::CompatibilityState::Verified;
    rec.known_issues.push_back({
        "BUG-001", compatdb::IssueSeverity::Critical,
        "Critical rendering failure", "", "", ""
    });
    auto result = compatdb::validate_state_transitions(rec);
    CHECK(!result.valid, "Verified + critical issue should fail state transition validation");
    PASS();
}

static void test_state_unsupported_consistent_with_tier() {
    TEST("Unsupported state warns if tier is not unsupported");
    auto rec = make_valid_record();
    rec.compatibility_state = compatdb::CompatibilityState::Unsupported;
    rec.execution_tier = compatdb::ExecutionTier::NativeSubstitution;
    auto result = compatdb::validate_state_transitions(rec);
    // This is a warning, not an error — state can be unsupported on any tier
    // but it's inconsistent. Check it produces a warning.
    bool has_warning = false;
    for (const auto& e : result.errors) {
        if (e.level == compatdb::ValidationError::Level::Warning) has_warning = true;
    }
    CHECK(has_warning, "Unsupported state on non-unsupported tier should warn");
    PASS();
}

static void test_empty_issue_detected() {
    TEST("Known issues with empty required fields are detected");
    auto rec = make_valid_record();
    rec.known_issues.push_back({});
    auto result = compatdb::validate_record(rec);
    CHECK(!result.valid, "Empty issue should fail validation");
    PASS();
}

static void test_database_load_and_query() {
    TEST("Database loads records and supports all query types");
    compatdb::CompatDatabase db;

    // Add a record directly
    auto rec = make_valid_record();
    rec.tags = {"electron", "ai-tool"};
    rec.known_issues.push_back({
        "B-001", compatdb::IssueSeverity::Low,
        "Minor UI glitch", "rendering", "", ""
    });
    db.add_record(rec);

    // Add another
    auto rec2 = make_valid_record();
    rec2.record_id = "com.other.app";
    rec2.bundle_identifier = "com.other.app";
    rec2.application_name = "OtherApp";
    rec2.compatibility_state = compatdb::CompatibilityState::Broken;
    rec2.execution_tier = compatdb::ExecutionTier::Unsupported;
    rec2.tags = {"broken"};
    db.add_record(rec2);

    CHECK(db.record_count() == 2, "Should have 2 records");
    CHECK(db.valid_count() == 2, "Both should be valid");

    // Lookup by ID
    auto* found = db.lookup_by_id("com.test.app");
    CHECK(found != nullptr, "Should find record by ID");
    CHECK(found->application_name == "TestApp", "Should have correct name");

    // Lookup by bundle
    auto bundles = db.lookup_by_bundle_id("com.test.app");
    CHECK(bundles.size() == 1, "Should find exactly 1 by bundle ID");

    // Filter by state
    auto broken = db.filter_by_state(compatdb::CompatibilityState::Broken);
    CHECK(broken.size() == 1, "Should find 1 broken record");

    // Filter by tier
    auto unsupported = db.filter_by_tier(compatdb::ExecutionTier::Unsupported);
    CHECK(unsupported.size() == 1, "Should find 1 unsupported tier record");

    // Filter by tag
    auto electron = db.filter_by_tag("electron");
    CHECK(electron.size() == 1, "Should find 1 electron-tagged record");

    // Search by name
    auto search = db.search_by_name("other");
    CHECK(search.size() == 1, "Should find 1 by name search");

    // All records
    auto all = db.all_records();
    CHECK(all.size() == 2, "Should return all 2 records");

    PASS();
}

static void test_degradation_metadata_validation() {
    TEST("Degradation metadata invalid category is rejected");
    auto rec = make_valid_record();
    compatdb::DegradationMetadata dm;
    dm.category = "invalid_cat";
    rec.degradation = dm;
    auto result = compatdb::validate_record(rec);
    CHECK(!result.valid, "Invalid degradation category should fail");
    PASS();
}

static void test_state_transition_unsafe_cannot_be_verified() {
    TEST("Unsafe degradation cannot have verified compatibility state");
    auto rec = make_valid_record();
    rec.compatibility_state = compatdb::CompatibilityState::Verified;
    compatdb::DegradationMetadata dm;
    dm.category = "unsafe";
    rec.degradation = dm;
    auto result = compatdb::validate_state_transitions(rec);
    CHECK(!result.valid, "Unsafe + Verified state should fail validation");
    PASS();
}

static void test_state_transition_hard_failure_requires_broken() {
    TEST("Hard failure degradation requires broken compatibility state");
    auto rec = make_valid_record();
    rec.compatibility_state = compatdb::CompatibilityState::Functional;
    compatdb::DegradationMetadata dm;
    dm.category = "hard_failure";
    rec.degradation = dm;
    auto result = compatdb::validate_state_transitions(rec);
    CHECK(!result.valid, "Hard failure + Functional state should fail validation");
    PASS();
}

static void test_native_registry_fallback() {
    TEST("NativeRegistry finds replacement with ABI checks and known_bad filtering");
    
    // Write mock registry file
    std::string tmp_reg = "/tmp/mock-native-registry.json";
    std::ofstream out(tmp_reg);
    out << R"({
      "schema_version": "1.0.0",
      "entries": {
        "test-module": {
          "module_name": "test-module",
          "stub_policy": "never_stub",
          "common_role": "state-database",
          "governed_versions": {
            "2.0.0": {
              "npm_version": "2.0.0",
              "electron_abi": { "node_module_version": 118, "electron_versions": ["28"] },
              "known_good": true
            },
            "1.0.0": {
              "npm_version": "1.0.0",
              "electron_abi": { "node_module_version": 110, "electron_versions": ["24"] },
              "known_good": false,
              "known_bad_on": [{"electron_major": 28, "reason": "Failed test"}]
            }
          }
        }
      }
    })";
    out.close();

    compatdb::NativeRegistry reg;
    CHECK(reg.load(tmp_reg), "Should load mock registry");

    // Test 1: Exact match
    auto r1 = reg.find_replacement("test-module", "28.0.0", "x86_64");
    CHECK(r1.has_value(), "Should find replacement for major 28");
    CHECK(r1->npm_version == "2.0.0", "Should exact match to 2.0.0");

    // Test 2: Known bad filtering
    auto r2 = reg.find_replacement("test-module", "24.0.0", "x86_64");
    CHECK(r2.has_value(), "Should find replacement for major 24");
    CHECK(r2->npm_version == "1.0.0", "Should exact match to 1.0.0");

    // Test 3: Closest ABI delta resolution
    auto r3 = reg.find_replacement("test-module", "30.0.0", "x86_64");
    CHECK(r3.has_value(), "Should resolve closest ABI delta");
    CHECK(r3->npm_version == "2.0.0", "2.0.0 is closer to ABI 120 than 1.0.0");

    // Clean up
    unlink(tmp_reg.c_str());
    PASS();
}


int main() {
    std::cout << "\n=== Compat-DB Unit Tests ===\n\n";

    test_valid_record_passes();
    test_missing_required_fields();
    test_invalid_schema_version();
    test_record_id_validation();
    test_bundle_id_warns_without_dots();
    test_state_transition_verified_with_critical_issue();
    test_state_unsupported_consistent_with_tier();
    test_empty_issue_detected();
    test_database_load_and_query();
    test_degradation_metadata_validation();
    test_state_transition_unsafe_cannot_be_verified();
    test_state_transition_hard_failure_requires_broken();
    test_native_registry_fallback();

    std::cout << "\n=== Results: " << tests_passed << " passed, "
              << tests_failed << " failed ===\n";

    return tests_failed > 0 ? 1 : 0;
}
