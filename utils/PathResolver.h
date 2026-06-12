#ifndef UTILS_PATHRESOLVER_H
#define UTILS_PATHRESOLVER_H

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace utils {

inline std::string resolve_path(const std::string& requested_path) {
    const std::filesystem::path rel(requested_path);
    if (rel.is_absolute()) {
        if (std::filesystem::exists(rel)) return requested_path;
        throw std::runtime_error("File not found: " + requested_path);
    }

    std::vector<std::string> candidates;
    candidates.push_back(requested_path);

    auto push_unique = [&](const std::string& c) {
        if (c.empty()) return;
        for (const auto& e : candidates)
            if (e == c) return;
        candidates.push_back(c);
    };

    const char* workspace_dir = std::getenv("BUILD_WORKSPACE_DIRECTORY");
    if (workspace_dir != nullptr && workspace_dir[0] != '\0') {
        push_unique((std::filesystem::path(workspace_dir) / rel).string());
    }

    const char* runfiles_dir = std::getenv("RUNFILES_DIR");
    if (runfiles_dir != nullptr && runfiles_dir[0] != '\0') {
        const std::filesystem::path base(runfiles_dir);
        push_unique((base / requested_path).string());
        push_unique((base / "_main" / requested_path).string());
    }

    const char* test_srcdir = std::getenv("TEST_SRCDIR");
    if (test_srcdir != nullptr && test_srcdir[0] != '\0') {
        const std::filesystem::path base(test_srcdir);
        const char* test_workspace = std::getenv("TEST_WORKSPACE");
        if (test_workspace != nullptr && test_workspace[0] != '\0') {
            push_unique((base / test_workspace / requested_path).string());
        }
        push_unique((base / "_main" / requested_path).string());
    }

    push_unique(std::filesystem::absolute(rel).string());

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return std::filesystem::absolute(candidate).string();
        }
    }

    std::string checked;
    for (size_t i = 0; i < candidates.size(); ++i) {
        checked += candidates[i];
        if (i + 1 < candidates.size()) checked += ", ";
    }
    throw std::runtime_error("File not found: " + requested_path + " (checked: " + checked + ")");
}

}  // namespace utils

#endif  // UTILS_PATHRESOLVER_H
