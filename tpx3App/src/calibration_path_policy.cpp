/*
 * ADTimePix3 - calibration path containment
 *
 * SPDX-License-Identifier: MIT
 */

#include "calibration_path_policy.h"

#include <filesystem>
#include <system_error>

namespace ADTimePix3Calibration {
namespace {

namespace fs = std::filesystem;

bool isWithin(const fs::path& root, const fs::path& candidate)
{
    auto rootIt = root.begin();
    auto candidateIt = candidate.begin();
    for (; rootIt != root.end(); ++rootIt, ++candidateIt) {
        if (candidateIt == candidate.end() || *candidateIt != *rootIt) {
            return false;
        }
    }
    return true;
}

bool isBaseName(const fs::path& name)
{
    return !name.empty() && !name.is_absolute() &&
           name.filename() == name && name != "." && name != "..";
}

}  // namespace

PathPolicy::PathPolicy() = default;

PathPolicy::PathPolicy(const std::string& root)
{
    configure(root);
}

PathStatus PathPolicy::configure(const std::string& root)
{
    root_.clear();
    if (root.empty()) return PathStatus::RootNotConfigured;

    const fs::path requested(root);
    if (!requested.is_absolute()) return PathStatus::RootNotAbsolute;

    std::error_code error;
    const fs::path canonicalRoot = fs::canonical(requested, error);
    if (error || !fs::is_directory(canonicalRoot, error) || error) {
        return PathStatus::RootUnavailable;
    }
    root_ = canonicalRoot.lexically_normal().string();
    return PathStatus::Ok;
}

PathStatus PathPolicy::validateDirectory(const std::string& directory,
                                         std::string& resolved) const
{
    resolved.clear();
    if (root_.empty()) return PathStatus::RootNotConfigured;

    std::error_code error;
    const fs::path canonicalRoot(root_);
    const fs::path canonicalDirectory = fs::canonical(fs::path(directory), error);
    if (error || !fs::is_directory(canonicalDirectory, error) || error) {
        return PathStatus::DirectoryUnavailable;
    }
    if (!isWithin(canonicalRoot, canonicalDirectory)) {
        return PathStatus::DirectoryOutsideRoot;
    }
    resolved = canonicalDirectory.lexically_normal().string();
    return PathStatus::Ok;
}

PathStatus PathPolicy::resolve(const std::string& directory,
                               const std::string& fileName,
                               PathAccess access,
                               std::string& resolved) const
{
    resolved.clear();
    if (root_.empty()) return PathStatus::RootNotConfigured;

    const fs::path name(fileName);
    if (!isBaseName(name)) return PathStatus::InvalidFileName;

    std::string resolvedDirectory;
    const PathStatus directoryStatus = validateDirectory(directory, resolvedDirectory);
    if (directoryStatus != PathStatus::Ok) return directoryStatus;

    std::error_code error;
    const fs::path canonicalRoot(root_);
    const fs::path canonicalDirectory(resolvedDirectory);
    const fs::path requested = canonicalDirectory / name;
    fs::path canonicalTarget;
    if (access == PathAccess::Read || fs::exists(requested, error)) {
        error.clear();
        canonicalTarget = fs::canonical(requested, error);
        if (error) return PathStatus::TargetUnavailable;
        if (access == PathAccess::Read && !fs::is_regular_file(canonicalTarget, error)) {
            return PathStatus::TargetNotRegular;
        }
        if (error) return PathStatus::TargetUnavailable;
    } else {
        error.clear();
        canonicalTarget = fs::weakly_canonical(requested, error);
        if (error) return PathStatus::TargetUnavailable;
    }
    canonicalTarget = canonicalTarget.lexically_normal();
    if (!isWithin(canonicalRoot, canonicalTarget)) {
        return PathStatus::TargetOutsideRoot;
    }
    resolved = canonicalTarget.string();
    return PathStatus::Ok;
}

bool PathPolicy::configured() const
{
    return !root_.empty();
}

bool PathPolicy::permissive() const
{
    return root_ == "/";
}

const std::string& PathPolicy::root() const
{
    return root_;
}

const char* statusMessage(PathStatus status)
{
    switch (status) {
    case PathStatus::Ok: return "ok";
    case PathStatus::RootNotConfigured: return "calibration root is not configured";
    case PathStatus::RootNotAbsolute: return "calibration root is not absolute";
    case PathStatus::RootUnavailable: return "calibration root is not an accessible directory";
    case PathStatus::DirectoryUnavailable: return "calibration directory is not accessible";
    case PathStatus::DirectoryOutsideRoot: return "calibration directory is outside the approved root";
    case PathStatus::InvalidFileName: return "calibration filename must be a basename";
    case PathStatus::TargetUnavailable: return "calibration target is not accessible";
    case PathStatus::TargetNotRegular: return "calibration target is not a regular file";
    case PathStatus::TargetOutsideRoot: return "calibration target resolves outside the approved root";
    }
    return "unknown calibration path error";
}

}  // namespace ADTimePix3Calibration
