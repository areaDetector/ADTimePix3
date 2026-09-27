/*
 * ADTimePix3 - calibration path containment
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CALIBRATION_PATH_POLICY_H
#define CALIBRATION_PATH_POLICY_H

#include <string>

namespace ADTimePix3Calibration {

enum class PathAccess {
    Read,
    Write
};

enum class PathStatus {
    Ok,
    RootNotConfigured,
    RootNotAbsolute,
    RootUnavailable,
    DirectoryUnavailable,
    DirectoryOutsideRoot,
    InvalidFileName,
    TargetUnavailable,
    TargetNotRegular,
    TargetOutsideRoot
};

class PathPolicy {
public:
    PathPolicy();
    explicit PathPolicy(const std::string& root);

    PathStatus configure(const std::string& root);
    PathStatus validateDirectory(const std::string& directory,
                                 std::string& resolved) const;
    PathStatus resolve(const std::string& directory, const std::string& fileName,
                       PathAccess access, std::string& resolved) const;

    bool configured() const;
    bool permissive() const;
    const std::string& root() const;

private:
    std::string root_;
};

const char* statusMessage(PathStatus status);

}  // namespace ADTimePix3Calibration

#endif
