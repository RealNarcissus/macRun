// Compatibility Database Storage, Indexing, and Query Engine
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md
//   — "compat-db Contract": never launches applications, performs runtime
//     analysis, or owns execution logic. Pure data layer.
#pragma once
#include <compatdb/types.hpp>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>

namespace compatdb {

// Internal wrapper: record + its source file path
struct InternalRecord {
    CompatibilityRecord record;
    std::string source_file;
};

class CompatDatabase {
public:
    CompatDatabase();

    size_t load_directory(const std::string& directory_path);
    bool load_file(const std::string& file_path);
    void add_record(CompatibilityRecord record);

    const CompatibilityRecord* lookup_by_id(const std::string& record_id) const;
    std::vector<QueryResult> lookup_by_bundle_id(const std::string& bundle_identifier) const;
    std::vector<QueryResult> lookup_by_bundle_pattern(const std::string& pattern) const;
    std::vector<QueryResult> filter_by_state(CompatibilityState state) const;
    std::vector<QueryResult> filter_by_tier(ExecutionTier tier) const;
    std::vector<QueryResult> filter_by_tag(const std::string& tag) const;
    std::vector<QueryResult> search_by_name(const std::string& name) const;
    std::vector<QueryResult> all_records() const;

    size_t record_count() const { return records_.size(); }
    size_t valid_count() const { return valid_count_; }
    size_t invalid_count() const { return invalid_count_; }
    const std::vector<std::pair<std::string, std::string>>& load_errors() const { return load_errors_; }

    // Exposed for unit testing; production callers should use load_directory/load_file
    static CompatibilityRecord parse_json(const std::string& json_text, const std::string& source_file);
    static std::string serialize_json(const CompatibilityRecord& record);

private:
    std::vector<InternalRecord> records_;
    std::unordered_map<std::string, size_t> id_index_;
    std::unordered_map<std::string, std::vector<size_t>> bundle_index_;
    std::unordered_map<std::string, std::vector<size_t>> tag_index_;
    size_t valid_count_ = 0;
    size_t invalid_count_ = 0;
    std::vector<std::pair<std::string, std::string>> load_errors_;

    void rebuild_indices();
};

} // namespace compatdb
